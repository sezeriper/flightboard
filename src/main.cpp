#include "app.hpp"

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <SDL3_shadercross/SDL_shadercross.h>

#include <chrono>
using namespace std::chrono_literals;

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
  // must be deleted in SDL_AppQuit
  auto state = new flb::app::State;
  flb::app::init(*state);
  *appstate = state;

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
  if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
  {
    return SDL_APP_SUCCESS;
  }
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
  auto& state = *static_cast<flb::app::State*>(appstate);
  SDL_AppResult drawResult = flb::app::draw(state);

  if (drawResult != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }


  // print fps
  const auto now = std::chrono::steady_clock::now();
  if (state.startTime.time_since_epoch().count() == 0) {
      state.startTime = now;
      return SDL_APP_CONTINUE;
  }

  const auto dt = now - state.startTime;
  ++state.frameCount;
  if (dt >= 1s) {
    const auto fps = static_cast<double>(state.frameCount) / std::chrono::duration<double>(dt).count();
    SDL_Log("FPS: %.2f", fps);
    state.startTime = now;
    state.frameCount = 0;
  }

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
  auto state = (flb::app::State*)appstate;
  flb::app::cleanup(*state);
  delete state;
}
