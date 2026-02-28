#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <cstdint>
#include <span>
#include <vector>

namespace flb
{
namespace gpu
{
struct BufferHandle
{
  SDL_GPUBuffer* buffer;
  Uint32 size;
};

struct TextureHandle
{
  SDL_GPUTexture* texture;
  Uint32 size;
  Uint32 width;
  Uint32 height;
};

class Allocator
{
public:
  void init(SDL_GPUDevice* device) { this->device = device; };

  void cleanup()
  {
    for (const auto& chunk : transferChunks)
    {
      SDL_UnmapGPUTransferBuffer(device, chunk.transferBuffer);
      SDL_ReleaseGPUTransferBuffer(device, chunk.transferBuffer);
    }
  }

  std::span<std::byte> allocateBuffer(BufferHandle destinationBuffer)
  {
    Uint32 allocOffset = 0;
    auto span = allocateRaw(destinationBuffer.size, allocOffset);
    if (span.empty())
      return {};

    pendingBufferCopies.emplace_back(destinationBuffer, activeChunkIndex, allocOffset);

    return span;
  }

  std::span<std::byte> allocateTexture(TextureHandle destinationTexture)
  {
    Uint32 allocOffset = 0;
    auto span = allocateRaw(destinationTexture.size, allocOffset);
    if (span.empty())
      return {};

    pendingTextureCopies.emplace_back(destinationTexture, activeChunkIndex, allocOffset);

    return span;
  }

  void upload()
  {
    if (pendingBufferCopies.empty() && pendingTextureCopies.empty())
      return;

    SDL_GPUCommandBuffer* commandBuffer = SDL_AcquireGPUCommandBuffer(device);
    if (commandBuffer == NULL)
    {
      SDL_Log("AcquireGPUCommandBuffer failed: %s", SDL_GetError());
      return;
    }

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commandBuffer);
    for (const auto& copy : pendingBufferCopies)
    {
      SDL_GPUTransferBufferLocation source{
        .transfer_buffer = transferChunks[copy.transferChunkIndex].transferBuffer,
        .offset = copy.offsetInTransferBuffer,
      };
      SDL_GPUBufferRegion destination{
        .buffer = copy.destinationBuffer.buffer,
        .offset = 0,
        .size = copy.destinationBuffer.size,
      };

      SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);
    }

    for (const auto& copy : pendingTextureCopies)
    {
      SDL_GPUTextureTransferInfo source{
        .transfer_buffer = transferChunks[copy.transferChunkIndex].transferBuffer,
        .offset = copy.offsetInTransferBuffer,
      };
      SDL_GPUTextureRegion destination{
        .texture = copy.destinationTexture.texture,
        .w = copy.destinationTexture.width,
        .h = copy.destinationTexture.height,
        .d = 1,
      };

      SDL_UploadToGPUTexture(copyPass, &source, &destination, false);
    }

    SDL_EndGPUCopyPass(copyPass);
    SDL_SubmitGPUCommandBuffer(commandBuffer);

    activeChunkIndex = -1;
    availableMemorySize = 0;
    currentOffset = 0;
    pendingBufferCopies.clear();
    pendingTextureCopies.clear();
  }

  BufferHandle createVertexBuffer(Uint32 size)
  {
    SDL_GPUBufferCreateInfo bufferCreateInfo{
      .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
      .size = size,
    };
    SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device, &bufferCreateInfo);
    if (buffer == NULL)
    {
      SDL_Log("CreateGPUBuffer failed: %s", SDL_GetError());
      return {NULL, 0};
    }

    // buffersToRelease.push_back(buffer);
    return {buffer, size};
  }

  BufferHandle createIndexBuffer(Uint32 size)
  {
    SDL_GPUBufferCreateInfo bufferCreateInfo{
      .usage = SDL_GPU_BUFFERUSAGE_INDEX,
      .size = size,
    };
    SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device, &bufferCreateInfo);
    if (buffer == NULL)
    {
      SDL_Log("CreateGPUBuffer failed: %s", SDL_GetError());
      return {NULL, 0};
    }

    // buffersToRelease.push_back(buffer);
    return {buffer, size};
  }

  TextureHandle createTexture(Uint32 width, Uint32 height)
  {
    SDL_GPUTextureCreateInfo textureCreateInfo{
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

    // texturesToRelease.push_back(gpuTexture);
    return {gpuTexture, width * height * 4, width, height};
  }

  /**
   * Releases the buffer and safely removes it from any pending copy operations.
   */
  void releaseBuffer(SDL_GPUBuffer* buffer)
  {
    // TODO: Use a handle instead of raw pointers to avoid this kind of issues. This is a temporary solution to prevent
    // crashes when a buffer is released while it's still pending for copy.
    std::erase_if(
      pendingBufferCopies, [buffer](const PendingBufferCopy& copy) { return copy.destinationBuffer.buffer == buffer; });
    SDL_ReleaseGPUBuffer(device, buffer);
  }

  /**
   * Releases the texture and safely removes it from any pending copy operations.
   */
  void releaseTexture(SDL_GPUTexture* texture)
  {
    // TODO: Use a handle instead of raw pointers to avoid this kind of issues. This is a temporary solution to prevent
    // crashes when a texture is released while it's still pending for copy.
    std::erase_if(
      pendingTextureCopies,
      [texture](const PendingTextureCopy& copy) { return copy.destinationTexture.texture == texture; });
    SDL_ReleaseGPUTexture(device, texture);
  }

private:
  SDL_GPUDevice* device = NULL;

  static constexpr Uint32 TRANSFER_BUFFER_ALLOCATION_SIZE = 1 * 1024 * 1024 * 1024;
  Uint32 availableMemorySize = 0;
  Uint32 currentOffset = 0;

  struct TransferChunk
  {
    SDL_GPUTransferBuffer* transferBuffer;
    std::byte* mappedMemory;
    Uint32 capacity;
  };
  std::vector<TransferChunk> transferChunks;
  std::int32_t activeChunkIndex = -1;

  struct PendingBufferCopy
  {
    BufferHandle destinationBuffer;
    std::size_t transferChunkIndex;
    Uint32 offsetInTransferBuffer;
  };
  std::vector<PendingBufferCopy> pendingBufferCopies;

  struct PendingTextureCopy
  {
    TextureHandle destinationTexture;
    std::size_t transferChunkIndex;
    Uint32 offsetInTransferBuffer;
  };
  std::vector<PendingTextureCopy> pendingTextureCopies;

  SDL_AppResult getOrCreateChunk()
  {
    ++activeChunkIndex;

    // not enough transfer buffers so add one
    if (activeChunkIndex == transferChunks.size())
    {
      SDL_GPUTransferBufferCreateInfo transferBufCreateInfo{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = TRANSFER_BUFFER_ALLOCATION_SIZE,
      };

      auto transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferBufCreateInfo);
      if (transferBuffer == NULL)
      {
        SDL_Log("CreateGPUTransferBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
      }
      transferChunks.emplace_back(transferBuffer, nullptr, TRANSFER_BUFFER_ALLOCATION_SIZE);
    }

    auto& chunk = transferChunks[activeChunkIndex];
    chunk.mappedMemory = static_cast<std::byte*>(SDL_MapGPUTransferBuffer(device, chunk.transferBuffer, true));
    if (chunk.mappedMemory == NULL)
    {
      SDL_Log("MapGPUTransferBuffer failed: %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }

    currentOffset = 0;
    availableMemorySize = TRANSFER_BUFFER_ALLOCATION_SIZE;

    return SDL_APP_CONTINUE;
  }

  std::span<std::byte> allocateRaw(Uint32 size, Uint32& outAllocatedOffset)
  {
    if (size > availableMemorySize || activeChunkIndex == -1)
    {
      SDL_AppResult result = getOrCreateChunk();
      if (result == SDL_APP_FAILURE)
        return {};
    }

    outAllocatedOffset = currentOffset;
    currentOffset += size;
    availableMemorySize -= size;

    return std::span<std::byte>(transferChunks[activeChunkIndex].mappedMemory + outAllocatedOffset, size);
  }
};
} // namespace gpu
} // namespace flb
