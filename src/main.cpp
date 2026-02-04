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

  state->lastFrameTime = std::chrono::steady_clock::now();
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
  auto& state = *static_cast<flb::app::State*>(appstate);
  SDL_AppResult result = flb::app::handleEvent(state, event);
  if (result != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
  auto& state = *static_cast<flb::app::State*>(appstate);

  const auto now = std::chrono::steady_clock::now();
  const auto dt = std::chrono::duration<float>(now - state.lastFrameTime).count();
  state.lastFrameTime = now;

  SDL_AppResult result = flb::app::update(state, dt);
  if (result != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }

  result = flb::app::draw(state);
  if (result != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
  auto state = (flb::app::State*)appstate;
  flb::app::cleanup(*state);
  delete state;
}
