#pragma once

#include <SDL3/SDL.h>

namespace flb 
{
namespace app
{
  struct State {
    SDL_Window* window = NULL;
    SDL_GPUDevice* device = NULL;
    SDL_GPUGraphicsPipeline* pipeline = NULL;
  };

  SDL_AppResult init(State& state);
  void cleanup(const State& state);
  SDL_AppResult draw(const State& state);
} // namespace app
} // namespace flb