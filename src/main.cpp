#include "app.hpp"
#include "time.hpp"

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <SDL3_shadercross/SDL_shadercross.h>


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

  app->lastFrame = flb::Time::now();
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

  const flb::Time::TimePoint now = flb::Time::now();
  const flb::Time::Duration dt = now - app->lastFrame;
  app->lastFrame = now;

  SDL_AppResult result = app->update(flb::Time::toSeconds(dt));
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
