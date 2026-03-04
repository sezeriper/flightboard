#include "app.hpp"
#include "culling.hpp"
#include "math.hpp"
#include "quadtree.hpp"

#include <glm/ext/vector_common.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

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
    camera.speed = 3000.0;

    tileManager.init(&registry, &allocator);

    renderer.initDebugSphere(allocator);
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

  TimePoint currentTime = now();
  registry.clear<component::Visible>();

  const auto cameraPosition = camera.position;
  const auto frustum = createFrustum(camera);
  {
    // Timer timer("Quadtree construction and traversal");
    QuadTree quadtree;
    {
      // Timer quadTreeTimer("QuadTree build");
      constexpr std::uint32_t MAX_DEPTH = 19;
      quadtree.build(
        [cameraPosition, frustum](NodeCoords coords)
        {
          if (coords.level == MAX_DEPTH)
            return false;

          if (coords.level < 1)
            return true;

          const auto boundingSphere = generateBoundingSphereLoose(coords.level, coords.x, coords.y);
          const auto horizonCullingPoint = generateHorizonCullingPointLoose(boundingSphere);
          if (isOccluded(cameraPosition, frustum, boundingSphere, horizonCullingPoint))
            return false;

          const auto distance2 = glm::distance2(cameraPosition, boundingSphere.position);
          const double splitThreshold = boundingSphere.radius;

          if (distance2 > splitThreshold * splitThreshold)
            return false;

          return true;
        });
    }
    {
      // Timer traversalTimer("QuadTree traversal");
      quadtree.traverseLeaves(
        [this, cameraPosition, frustum, currentTime](NodeCoords coords)
        {
          const auto entity = tileManager.getOrCreateTile(coords, currentTime);
          if (entity == entt::null)
            return;

          const auto boundingSphere = registry.get<component::BoundingSphere>(entity).value;
          const auto horizonCullingPoint = registry.get<component::HorizonCullingPoint>(entity).value;

          if (isOccluded(cameraPosition, frustum, boundingSphere, horizonCullingPoint))
            return;

          registry.emplace<component::Visible>(entity);
        });
    }

    SDL_Log("Number of visible tiles: %zu", registry.view<component::Visible>().size());

    allocator.upload();
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult App::draw() { return renderer.draw(registry, camera, window); }
