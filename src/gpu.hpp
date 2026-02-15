#pragma once

#include <glm/glm.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <ranges>
#include <type_traits>

namespace flb
{
struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;
  glm::vec2 uv;
};

using Index = Uint16;

template<typename T>
concept VertexOrIndex = std::is_same_v<T, Vertex> || std::is_same_v<T, Index>;

struct Mesh {
  std::vector<Vertex> vertices;
  std::vector<Index> indices;
};

struct Texture {
  std::size_t width;
  std::size_t height;
  std::vector<std::byte> data;
};

struct GPUMesh {
  SDL_GPUBuffer* vertex;
  SDL_GPUBuffer* index;
  Uint32 numOfIndices;
};
using GPUTexture = SDL_GPUTexture*;

using Transform = glm::mat4;

struct Uniforms {
  Transform viewProjection;
  Transform model;
};


class Device
{
public:
  SDL_AppResult init()
  {
    SDL_InitSubSystem(SDL_INIT_VIDEO);

    device = SDL_CreateGPUDevice(
      SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB,
      true, NULL);
    if (device == NULL)
    {
      SDL_Log("CreateGPUDevice failed: %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }
    return SDL_APP_CONTINUE;
  }

  void cleanup()
  {
    SDL_DestroyGPUDevice(device);
  }

  SDL_GPUDevice* getDevice() const { return device; }

  /**
   * Creates a GPU mesh by uploading the vertex and index data to GPU buffers.
   * The caller is responsible for releasing the GPU with destroyGPUMesh buffers after use.
   * Returnes a GPUMesh with NULL buffers and 0 indices if creation fails.
   */
  GPUMesh createGPUMesh(const Mesh& mesh) const
  {
    SDL_GPUBuffer* vertexBuf = createGPUBuffer(mesh.vertices);
    if (vertexBuf == NULL) {
      return {};
    }

    SDL_GPUBuffer* indexBuf = createGPUBuffer(mesh.indices);
    if (indexBuf == NULL)
    {
      return {};
    }

    return {
      .vertex = vertexBuf,
      .index = indexBuf,
      .numOfIndices = static_cast<Uint32>(mesh.indices.size()),
    };
  }

  void releaseGPUMesh(const GPUMesh& gpuMesh) const
  {
    SDL_ReleaseGPUBuffer(device, gpuMesh.vertex);
    SDL_ReleaseGPUBuffer(device, gpuMesh.index);
  }

  GPUTexture createGPUTexture(const Texture& texture) const
  {
    const Uint32 bufferSize = static_cast<Uint32>(texture.data.size());
    const Uint32 width = static_cast<Uint32>(texture.width);
    const Uint32 height = static_cast<Uint32>(texture.height);

    SDL_GPUTransferBuffer* transferBuf = createTransferBuffer(texture.data);
    if (transferBuf == NULL) {
      return NULL;
    }

    SDL_GPUTextureCreateInfo textureCreateInfo {
      .type = SDL_GPU_TEXTURETYPE_2D,
      .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
      .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
      .width = width,
      .height = height,
      .layer_count_or_depth = 1,
      .num_levels = 1,
    };
    SDL_GPUTexture* gputexture =
      SDL_CreateGPUTexture(device, &textureCreateInfo);
    if (gputexture == NULL)
    {
      SDL_Log("CreateGPUTexture failed: %s", SDL_GetError());
      return NULL;
    }

    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);
    SDL_GPUTextureTransferInfo sourceTextureLocation {
      .transfer_buffer = transferBuf,
      .offset = 0,
    };
    SDL_GPUTextureRegion destTextureRegion {
      .texture = gputexture,
      .w = static_cast<Uint32>(texture.width),
      .h = static_cast<Uint32>(texture.height),
      .d = 1,
    };

    SDL_UploadToGPUTexture(copyPass, &sourceTextureLocation, &destTextureRegion, false);
    SDL_EndGPUCopyPass(copyPass);
    bool success = SDL_SubmitGPUCommandBuffer(cmdBuf);
    if (!success)
    {
      SDL_Log("SubmitGPUCommandBuffer failed: %s", SDL_GetError());
      return NULL;
    }

    SDL_ReleaseGPUTransferBuffer(device, transferBuf);

    return gputexture;
  }

  void releaseGPUTexture(const GPUTexture& gpuTexture) const
  {
    SDL_ReleaseGPUTexture(device, gpuTexture);
  }

private:
  SDL_GPUDevice* device = NULL;

  template<typename R>
  requires std::ranges::contiguous_range<R> &&
          std::ranges::sized_range<R>
  SDL_GPUTransferBuffer* createTransferBuffer(const R& data) const
  {
    using T = std::ranges::range_value_t<R>;
    const Uint32 bufferSize = static_cast<Uint32>(std::ranges::size(data) * sizeof(T));

    SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo {
      .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
      .size = bufferSize,
      .props = 0,
    };
    SDL_GPUTransferBuffer* transferBuf =
      SDL_CreateGPUTransferBuffer(device, &transferBufferCreateInfo);
    if (transferBuf == NULL)
    {
      SDL_Log("CreateGPUTransferBuffer failed: %s", SDL_GetError());
      return NULL;
    }

    T* mappedBuf = reinterpret_cast<T*>(
      SDL_MapGPUTransferBuffer(device, transferBuf, false));
    if (mappedBuf == NULL)
    {
      SDL_Log("MapGPUTransferBuffer failed: %s", SDL_GetError());
      return NULL;
    }

    std::ranges::copy(data, mappedBuf);
    SDL_UnmapGPUTransferBuffer(device, transferBuf);
    mappedBuf = NULL;

    return transferBuf;
  }

  template<typename R>
  requires std::ranges::contiguous_range<R> &&
           std::ranges::sized_range<R> &&
           VertexOrIndex<std::ranges::range_value_t<R>>
  SDL_GPUBuffer* createGPUBuffer(const R& data) const
  {
    SDL_GPUTransferBuffer* transferBuf = createTransferBuffer(data);
    if (transferBuf == NULL) {
      return NULL;
    }

    using T = std::ranges::range_value_t<R>;
    const Uint32 bufferSize = static_cast<Uint32>(std::ranges::size(data) * sizeof(T));

    SDL_GPUBufferCreateInfo bufferCreateInfo {
      .size = bufferSize,
      .props = 0,
    };

    if constexpr (std::same_as<T, Vertex>)
    {
      bufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    }
    else if constexpr (std::same_as<T, Index>)
    {
      bufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    }

    SDL_GPUBuffer* buf =
      SDL_CreateGPUBuffer(device, &bufferCreateInfo);
    if (buf == NULL)
    {
      SDL_Log("CreateGPUBuffer failed: %s", SDL_GetError());
      return NULL;
    }

    SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);
    SDL_GPUTransferBufferLocation sourceBufferLocation {
      .transfer_buffer = transferBuf,
      .offset = 0,
    };
    SDL_GPUBufferRegion destBufferRegion {
      .buffer = buf,
      .offset = 0,
      .size = bufferSize,
    };

    SDL_UploadToGPUBuffer(copyPass, &sourceBufferLocation, &destBufferRegion, false);
    SDL_EndGPUCopyPass(copyPass);
    bool success = SDL_SubmitGPUCommandBuffer(cmdBuf);
    if (!success)
    {
      SDL_Log("SubmitGPUCommandBuffer failed: %s", SDL_GetError());
      return nullptr;
    }

    SDL_ReleaseGPUTransferBuffer(device, transferBuf);

    return buf;
  }
};
}
