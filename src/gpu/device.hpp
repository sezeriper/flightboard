#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <glm/glm.hpp>

#include <span>
#include <vector>

namespace flb
{
namespace gpu
{
struct Uniforms
{
  glm::mat4 viewProjection;
  glm::vec4 modelPosition;
  glm::mat4 modelTransform;
};

struct GPUBufferHandle
{
  SDL_GPUBuffer* buffer;
  Uint32 size;
};

struct GPUTextureHandle
{
  SDL_GPUTexture* texture;
  Uint32 size;
  Uint32 width;
  Uint32 height;
};

class Device
{
public:
  SDL_AppResult init()
  {
    SDL_InitSubSystem(SDL_INIT_VIDEO);

    device = SDL_CreateGPUDevice(
      SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB,
      true,
      NULL);
    if (device == NULL)
    {
      SDL_Log("CreateGPUDevice failed: %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
  }

  void cleanup()
  {
    for (SDL_GPUBuffer* buffer : buffersToRelease)
    {
      SDL_ReleaseGPUBuffer(device, buffer);
    }

    for (SDL_GPUTexture* texture : texturesToRelease)
    {
      SDL_ReleaseGPUTexture(device, texture);
    }

    for (const auto& chunk : transferChunks)
    {
      SDL_UnmapGPUTransferBuffer(device, chunk.transferBuffer);
      SDL_ReleaseGPUTransferBuffer(device, chunk.transferBuffer);
    }

    SDL_ReleaseGPUTexture(device, depthTexture);
    SDL_DestroyGPUDevice(device);
  }

  SDL_GPUDevice* getDevice() const { return device; }
  SDL_GPUCommandBuffer* getDrawCommandBuffer() const { return SDL_AcquireGPUCommandBuffer(device); }
  SDL_GPUTexture* getDepthTexture() const { return depthTexture; }

  SDL_AppResult createDepthTexture(Uint32 width, Uint32 height)
  {
    if (depthTexture != NULL)
    {
      SDL_ReleaseGPUTexture(device, depthTexture);
      depthTexture = NULL;
    }

    SDL_GPUTextureCreateInfo depthTextCreateInfo {
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
      .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
      .width = width,
      .height = height,
      .layer_count_or_depth = 1,
      .num_levels = 1,
    };
    depthTexture = SDL_CreateGPUTexture(device, &depthTextCreateInfo);
    if (depthTexture == NULL)
    {
      SDL_Log("CreateGPUTexture failed: %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }
    return SDL_APP_CONTINUE;
  }

  std::span<std::byte> allocateBuffer(GPUBufferHandle destinationBuffer)
  {
    Uint32 allocOffset = 0;
    auto span = allocateRaw(destinationBuffer.size, allocOffset);
    if (span.empty())
      return {};

    pendingBufferCopies.emplace_back(destinationBuffer, activeTransferChunkIndex, allocOffset);

    return span;
  }

  std::span<std::byte> allocateTexture(GPUTextureHandle destinationTexture)
  {
    Uint32 allocOffset = 0;
    auto span = allocateRaw(destinationTexture.size, allocOffset);
    if (span.empty())
      return {};

    pendingTextureCopies.emplace_back(destinationTexture, activeTransferChunkIndex, allocOffset);

    return span;
  }

  void upload()
  {
    if (pendingBufferCopies.empty() && pendingTextureCopies.empty())
      return;

    SDL_GPUCommandBuffer* uploadCmdBuf = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmdBuf);
    for (const auto& copy : pendingBufferCopies)
    {
      SDL_GPUTransferBufferLocation source {
        .transfer_buffer = transferChunks[copy.transferChunkIndex].transferBuffer,
        .offset = copy.offsetInTransferBuffer,
      };
      SDL_GPUBufferRegion destination {
        .buffer = copy.destinationBuffer.buffer,
        .offset = 0,
        .size = copy.destinationBuffer.size,
      };

      SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);
    }

    for (const auto& copy : pendingTextureCopies)
    {
      SDL_GPUTextureTransferInfo source {
        .transfer_buffer = transferChunks[copy.transferChunkIndex].transferBuffer,
        .offset = copy.offsetInTransferBuffer,
      };
      SDL_GPUTextureRegion destination {
        .texture = copy.destinationTexture.texture,
        .w = copy.destinationTexture.width,
        .h = copy.destinationTexture.height,
        .d = 1,
      };

      SDL_UploadToGPUTexture(copyPass, &source, &destination, false);
    }

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(uploadCmdBuf);

    pendingBufferCopies.clear();
    pendingTextureCopies.clear();
    currentOffset = 0;
  }

  GPUBufferHandle createVertexBuffer(Uint32 size)
  {
    SDL_GPUBufferCreateInfo bufferCreateInfo {
      .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
      .size = size,
    };
    SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device, &bufferCreateInfo);
    if (buffer == NULL)
    {
      SDL_Log("CreateGPUBuffer failed: %s", SDL_GetError());
      return {NULL, 0};
    }

    buffersToRelease.push_back(buffer);
    return {buffer, size};
  }

  GPUBufferHandle createIndexBuffer(Uint32 size)
  {
    SDL_GPUBufferCreateInfo bufferCreateInfo {
      .usage = SDL_GPU_BUFFERUSAGE_INDEX,
      .size = size,
    };
    SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device, &bufferCreateInfo);
    if (buffer == NULL)
    {
      SDL_Log("CreateGPUBuffer failed: %s", SDL_GetError());
      return {NULL, 0};
    }

    buffersToRelease.push_back(buffer);
    return {buffer, size};
  }

  GPUTextureHandle createTexture(Uint32 width, Uint32 height)
  {
    SDL_GPUTextureCreateInfo textureCreateInfo {
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
      .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
      .width = width,
      .height = height,
      .layer_count_or_depth = 1,
      .num_levels = 1,
    };
    SDL_GPUTexture* gpuTexture = SDL_CreateGPUTexture(device, &textureCreateInfo);
    if (gpuTexture == NULL)
    {
      SDL_Log("CreateGPUTexture failed: %s", SDL_GetError());
      return {NULL, 0, 0};
    }

    texturesToRelease.push_back(gpuTexture);
    return {gpuTexture, width * height * 4, width, height};
  }

private:
  SDL_GPUDevice* device = NULL;
  SDL_GPUTexture* depthTexture = NULL;

  const Uint32 TRANSFER_BUFFER_ALLOCATION_SIZE = 1 * 1024 * 1024 * 1024;
  Uint32 availableMemorySize = 0;
  Uint32 currentOffset = 0;

  struct TransferChunk
  {
    SDL_GPUTransferBuffer* transferBuffer;
    std::byte* mappedMemory;
  };
  std::vector<TransferChunk> transferChunks;
  std::size_t activeTransferChunkIndex = 0;

  struct PendingBufferCopy
  {
    GPUBufferHandle destinationBuffer;
    std::size_t transferChunkIndex;
    Uint32 offsetInTransferBuffer;
  };
  std::vector<PendingBufferCopy> pendingBufferCopies;

  struct PendingTextureCopy
  {
    GPUTextureHandle destinationTexture;
    std::size_t transferChunkIndex;
    Uint32 offsetInTransferBuffer;
  };
  std::vector<PendingTextureCopy> pendingTextureCopies;

  std::vector<SDL_GPUBuffer*> buffersToRelease;
  std::vector<SDL_GPUTexture*> texturesToRelease;

  SDL_AppResult addTransferChunk()
  {
    SDL_GPUTransferBufferCreateInfo transferBufCreateInfo {
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = TRANSFER_BUFFER_ALLOCATION_SIZE,
    };

    auto transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferBufCreateInfo);
    if (transferBuffer == NULL)
    {
      SDL_Log("CreateGPUTransferBuffer failed: %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }

    auto mappedMemory = static_cast<std::byte*>(SDL_MapGPUTransferBuffer(device, transferBuffer, false));
    if (mappedMemory == NULL)
    {
      SDL_Log("MapGPUTransferBuffer failed: %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }

    activeTransferChunkIndex = transferChunks.size();
    transferChunks.emplace_back(transferBuffer, mappedMemory);
    currentOffset = 0;
    availableMemorySize = TRANSFER_BUFFER_ALLOCATION_SIZE;

    return SDL_APP_SUCCESS;
  }

  std::span<std::byte> allocateRaw(Uint32 size, Uint32& outAllocatedOffset)
  {
    if (size > availableMemorySize)
    {
      SDL_AppResult result = addTransferChunk();
      if (result != SDL_APP_SUCCESS)
        return {};
    }

    outAllocatedOffset = currentOffset;
    currentOffset += size;
    availableMemorySize -= size;

    return std::span<std::byte>(transferChunks[activeTransferChunkIndex].mappedMemory + outAllocatedOffset, size);
  }
};
} // namespace gpu
} // namespace flb
