#pragma once

#include "gpu/render_context.hpp"

#include <SDL3/SDL.h>

namespace flb
{
class Window
{
public:
  SDL_AppResult init()
  {
    window = SDL_CreateWindow("flightboard v0.0.1", 1280, 720, SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE);
    if (window == NULL)
    {
      SDL_Log("CreateWindow failed %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }

    SDL_SetWindowRelativeMouseMode(window, false);

    return SDL_APP_CONTINUE;
  }

  void cleanup() { SDL_DestroyWindow(window); }

  SDL_Window* getPtr() const { return window; }

  SDL_GPUTexture* getSwapChainTexture(const gpu::RenderContext& context) const
  {
    SDL_GPUTexture* swapchainTexture = NULL;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(context.commandBuffer, window, &swapchainTexture, NULL, NULL))
    {
      SDL_Log("WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
      return NULL;
    }
    return swapchainTexture;
  }

private:
  SDL_Window* window;
};
} // namespace flb
