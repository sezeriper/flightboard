#pragma once

#include "device.hpp"

namespace flb
{
namespace gpu
{

class Sampler
{
public:
  SDL_AppResult init(const Device& device)
  {
    SDL_GPUSamplerCreateInfo samplerCreateInfo {
     	.min_filter = SDL_GPU_FILTER_LINEAR,
     	.mag_filter = SDL_GPU_FILTER_LINEAR,
     	.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
     	.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
     	.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
     	.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    sampler = SDL_CreateGPUSampler(device.getDevice(), &samplerCreateInfo);
    if (sampler == NULL)
    {
      SDL_Log("CreateGPUSampler failed: %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
  }

  void cleanup(const Device& device)
  {
    SDL_ReleaseGPUSampler(device.getDevice(), sampler);
  }

  SDL_GPUSampler* getSampler() const { return sampler; }

private:
  SDL_GPUSampler* sampler = NULL;
};

}
}
