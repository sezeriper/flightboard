#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <glm/glm.hpp>

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

    SDL_GPUTextureCreateInfo depthTextCreateInfo{
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

private:
  SDL_GPUDevice* device = NULL;
  SDL_GPUTexture* depthTexture = NULL;
};
} // namespace gpu
} // namespace flb
