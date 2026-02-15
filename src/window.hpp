#pragma once

#include <SDL3/SDL.h>

namespace flb
{
class Window
{
public:
  SDL_AppResult init()
  {
    window = SDL_CreateWindow("flightboard v0.0.1", 1280, 720,
      SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE);
    if (window == NULL)
    {
      SDL_Log("CreateWindow failed %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }
    return SDL_APP_CONTINUE;
  }

  void cleanup()
  {
    SDL_DestroyWindow(window);
  }

  SDL_Window* getWindow() const
  {
    return window;
  }

private:
  SDL_Window* window;
};
}
