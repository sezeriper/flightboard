#include "app.hpp"
#include "math.hpp"

#include <glm/ext/vector_common.hpp>

using namespace flb;

SDL_AppResult App::init()
{
  {
    Timer timer("Initialization");

    if (window.init() != SDL_APP_CONTINUE)
    {
      return SDL_APP_FAILURE;
    }

    if (renderer.init(window.getPtr()) != SDL_APP_CONTINUE)
    {
      return SDL_APP_FAILURE;
    }

    allocator.init(renderer.getDevice().getPtr());

    GeoCoords startCoords{39.811124, 30.528396};
    // camera.position = geoToECEF(startCoords, 1'000'000.0);
    camera.position = geoToECEF(startCoords);
    camera.up = getSurfaceNormal(startCoords);
    camera.speed = 3000.0;

    tileManager.init(&registry, &allocator);

    renderer.initDebugSphere(allocator);
    renderer.initTileIndexBuffer(allocator);
  }

  return SDL_APP_CONTINUE;
}

void App::cleanup()
{
  tileManager.cleanup();
  registry.clear();
  allocator.cleanup();
  renderer.cleanup(window.getPtr());
  window.cleanup();
}

SDL_AppResult App::handleEvent(SDL_Event* event)
{
  if (event->type == SDL_EVENT_QUIT || event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
  {
    return SDL_APP_SUCCESS;
  }

  if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
  {
    auto width = event->window.data1;
    auto height = event->window.data2;
    camera.aspect = static_cast<float>(width) / static_cast<float>(height);

    SDL_AppResult result = renderer.getDevice().createDepthTexture(width, height);
    if (result != SDL_APP_CONTINUE)
    {
      return SDL_APP_FAILURE;
    }
    return SDL_APP_CONTINUE;
  }

  if (event->type == SDL_EVENT_MOUSE_MOTION)
  {
    const auto dtx = event->motion.xrel;
    const auto dty = event->motion.yrel;
    camera.updateMouse(dtx, dty);
    return SDL_APP_CONTINUE;
  }

  if (event->type == SDL_EVENT_MOUSE_WHEEL)
  {
    const auto scrollAmount = glm::abs(event->wheel.y);
    const auto direction = event->wheel.y > 0.0f ? 1.0f : -1.0f;

    if (direction > 0.0f)
      camera.speed *= scrollAmount * 5.0f;
    else
      camera.speed /= scrollAmount * 5.0f;

    camera.speed = glm::max(camera.speed, 10.0);
    return SDL_APP_CONTINUE;
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult App::update(float dt)
{
  const bool* keyStates = SDL_GetKeyboardState(NULL);
  camera.updateKeyboard(dt, keyStates);

  tileManager.update(camera, now());

  return SDL_APP_CONTINUE;
}

SDL_AppResult App::draw() { return renderer.draw(registry, camera, window); }
