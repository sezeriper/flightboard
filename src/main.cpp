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
  auto app = new flb::App();
  SDL_AppResult result = app->init();
  if (result != SDL_APP_CONTINUE) {
    delete app;
    return result;
  }
  *appstate = app;

  app->lastFrameTime = std::chrono::steady_clock::now();
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
  auto* app = static_cast<flb::App*>(appstate);
  SDL_AppResult result = app->handleEvent(event);
  if (result != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
  auto* app = static_cast<flb::App*>(appstate);

  const auto now = std::chrono::steady_clock::now();
  const auto dt = std::chrono::duration<float>(now - app->lastFrameTime).count();
  app->lastFrameTime = now;

  SDL_AppResult result = app->update(dt);
  if (result != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }

  result = app->draw();
  if (result != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }

  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
  auto app = (flb::App*)appstate;
  if (app) {
    app->cleanup();
    delete app;
  }
}
