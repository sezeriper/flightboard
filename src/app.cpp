#include "app.hpp"
#include "math.hpp"
#include "quadtree.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#import <glm/gtx/norm.hpp>

using namespace flb;

SDL_AppResult App::init()
{
  {
    Timer timer("Initialization");

    if (window.init() != SDL_APP_CONTINUE)
    {
      return SDL_APP_FAILURE;
    }

    if (renderer.init(window.getWindow()) != SDL_APP_CONTINUE)
    {
      return SDL_APP_FAILURE;
    }

    allocator.init(renderer.getDevice().getDevice());

    GeoCoords startCoords{39.811124, 30.528396};
    // camera.position = geoToECEF(startCoords, 1'000'000.0);
    camera.position = geoToECEF(startCoords);
    camera.up = getSurfaceNormal(startCoords);
    camera.speed = 30000.0;

    tileManager.init(&registry, &allocator);
  }

  return SDL_APP_CONTINUE;
}

void App::cleanup()
{
  tileManager.cleanup();
  registry.clear();
  allocator.cleanup();
  renderer.cleanup(window.getWindow());
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

  return SDL_APP_CONTINUE;
}

SDL_AppResult App::update(float dt)
{
  const bool* keyStates = SDL_GetKeyboardState(NULL);
  camera.updateKeyboard(dt, keyStates);

  TimePoint currentTime = now();
  registry.clear<component::Visible>();

  auto cameraPosition = camera.position;
  {
    // Timer timer("Quadtree construction and traversal");
    QuadTree quadtree;
    {
      // Timer quadTreeTimer("QuadTree build");
      constexpr std::uint32_t MAX_DEPTH = 19;
      quadtree.build(
        [this, cameraPosition, currentTime](NodeCoords coords)
        {
          if (coords.level == MAX_DEPTH)
            return false;

          const auto tileCenter = tileToECEF(coords.level, coords.x + 0.5, coords.y + 0.5);
          const auto tileRadius = TILE_BOUNDING_SPHERE_RADII[coords.level];
          constexpr double cameraRadius = 10'000.0;

          const auto distance2 = glm::distance2(cameraPosition, tileCenter);
          if (distance2 > (tileRadius * tileRadius) + (cameraRadius * cameraRadius))
            return false;

          std::array<NodeCoords, 4> children = QuadTree::getChildren(coords);
          for (auto child : children)
          {
            auto entity = tileManager.getTile(child.level, child.x, child.y, currentTime);
            if (entity == entt::null)
            {
              return false;
            }
          }

          return true;
        });
    }
    {
      // Timer traversalTimer("QuadTree traversal");
      quadtree.traverseLeaves(
        [this, currentTime](NodeCoords coords)
        {
          auto entity = tileManager.getTile(coords.level, coords.x, coords.y, currentTime);
          registry.emplace<component::Visible>(entity);
        });
    }

    allocator.upload();

    // SDL_Log("Quadtree nodes: %zu", quadtree.size());
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult App::draw() { return renderer.draw(registry, camera, window); }
