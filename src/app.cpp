#include "app.hpp"
#include "tile_generator.hpp"
#include "components.hpp"

using namespace flb;
namespace
{
void loadTiles(gpu::Device& device, entt::registry& registry)
{
    Timer timer("Tile loading");
    TilesetDescription tilesetDescription {
      .tileImageSize = 256,
      .zoomMin = 6,
      .zoomMax = 6,
      .zoomRegion {
        // .min {39.808129, 30.497973},
        // .max {39.820784, 30.541466},
        .min {-85.0, -179.0},
        .max { 85.0,  179.0},
      }
    };

    const auto tileCoords = getIntersectingTileCoords(tilesetDescription);
    const auto tileOrigins = getTileOrigins(tileCoords);

    const auto tileIdxBufHandle = device.createIndexBuffer(INDEX_BUFFER_SIZE_PER_TILE);
    const auto tileIndexBuffer = tileIdxBufHandle.buffer;
    const auto indexBufferMemory = device.allocateBuffer(tileIdxBufHandle);
    std::span<gpu::Index> indices(reinterpret_cast<gpu::Index*>(indexBufferMemory.data()), NUM_INDICES_PER_TILE);
    generateTileIndices(indices);

    registry.group<
      component::Position,
      component::VertexBuffer,
      component::IndexBuffer,
      component::Texture>();

    for (std::size_t i = 0; i < tileCoords.size(); ++i)
    {
      const auto vertexBuffer = device.createVertexBuffer(VERTEX_BUFFER_SIZE_PER_TILE);
      std::span<std::byte> vertexBufferMemory = device.allocateBuffer(vertexBuffer);
      std::span<gpu::Vertex> vertices(reinterpret_cast<gpu::Vertex*>(vertexBufferMemory.data()), NUM_VERTICES_PER_TILE);
      generateTileVertices(tileCoords[i], tileOrigins[i], vertices);

      const auto texture = device.createTexture(tilesetDescription.tileImageSize, tilesetDescription.tileImageSize);
      std::span<std::byte> memory = device.allocateTexture(texture);
      const auto tilePath = getTilePath("content/tiles/eskisehir", tileCoords[i]);
      loadJPG(tilePath, memory);

      auto entity = registry.create();
      registry.emplace<component::Position>(entity, tileOrigins[i]);
      registry.emplace<component::VertexBuffer>(entity, vertexBuffer.buffer);
      registry.emplace<component::IndexBuffer>(entity, tileIndexBuffer);
      registry.emplace<component::Texture>(entity, texture.texture);
    }

    device.upload();

    SDL_Log("Loaded %zu tiles", tileCoords.size());
}
}
SDL_AppResult App::init()
{
  {
    Timer timer("Initialization");

    if (window.init() != SDL_APP_CONTINUE)
    {
      return SDL_APP_FAILURE;
    }

    if (renderer.init(window) != SDL_APP_CONTINUE)
    {
      return SDL_APP_FAILURE;
    }

    GeoCoords startCoords {39.811123, 30.528396};
    auto position = geoToECEF(startCoords, 1'000'000.0);
    // auto position = map::geoToECEF(origin);
    camera.up = getSurfaceNormal({39.811123, 30.528396});
    camera.position = position;
    camera.speed = 300000.0;

    loadTiles(renderer.getDevice(), registry);
  }

  return SDL_APP_CONTINUE;
}

void App::cleanup()
{
  registry.clear();
  renderer.cleanup(window);
  window.cleanup();
}

SDL_AppResult App::handleEvent(SDL_Event* event)
{
  if (event->type == SDL_EVENT_QUIT ||
      event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
  {
    return SDL_APP_SUCCESS;
  }

  if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
  {
    auto width = event->window.data1;
    auto height = event->window.data2;
    camera.aspect =
      static_cast<float>(width) /
      static_cast<float>(height);

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

  return SDL_APP_CONTINUE;
}

SDL_AppResult App::draw()
{
  return renderer.draw(registry, camera, window);
}
