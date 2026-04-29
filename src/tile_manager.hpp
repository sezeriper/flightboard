#pragma once

#include "components.hpp"
#include "culling.hpp"
#include "gpu/allocator.hpp"
#include "lru_cache.hpp"
#include "math.hpp"
#include "quadtree.hpp"
#include "texture_manager.hpp"
#include "tile_generator.hpp"
#include "time.hpp"
#include "utils.hpp"

#include <entt/entt.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

#include <cstdint>

namespace flb
{
class TileManager
{
public:
  static constexpr std::size_t CAPACITY = 8192;

  void init(entt::registry* registry, gpu::Allocator* allocator, TextureManager* textureManager)
  {
    this->registry = registry;
    this->allocator = allocator;
    this->textureManager = textureManager;

    registry->group<component::Position, component::VertexBuffer, component::Texture>();
  }

  void cleanup()
  {
    cache.clear(
      [this](NodeCoords /*key*/, entt::entity entity)
      {
        if (entity != entt::null)
        {
          destroyTile(entity);
        }
      });
  }

  void update(const Camera& camera, TimePoint currentTime)
  {
    registry->clear<component::Visible>();

    const auto cameraPosition = camera.position;
    const auto frustum = camera.createFrustum();

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
          const double splitThreshold = boundingSphere.radius * 4.0;

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
          const auto entity = getOrCreateTile(coords, currentTime);
          if (entity == entt::null)
            return;

          const auto boundingSphere = registry->get<component::BoundingSphere>(entity).value;
          const auto horizonCullingPoint = registry->get<component::HorizonCullingPoint>(entity).value;

          if (isOccluded(cameraPosition, frustum, boundingSphere, horizonCullingPoint))
            return;

          registry->emplace<component::Visible>(entity);
        });
    }

    allocator->upload();
  }

  entt::entity getOrCreateTile(NodeCoords coords, TimePoint currentTime)
  {
    auto cachedValue = cache.get(coords, currentTime);
    if (cachedValue.has_value())
    {
      return cachedValue.value();
    }

    // try to create the tile
    auto tile = createTile(coords, currentTime);

    auto evicted = cache.insert(coords, tile, currentTime);
    if (evicted.has_value() && evicted.value().second != entt::null)
    {
      destroyTile(evicted.value().second);
    }

    return tile;
  }

private:
  static constexpr std::size_t PROBE_LIMIT = 16;

  entt::registry* registry = nullptr;
  gpu::Allocator* allocator = nullptr;
  TextureManager* textureManager = nullptr;

  struct NodeCoordsHasher
  {
    // Generates a 64-bit Morton Code (Quadkey)
    static constexpr std::uint64_t splitBy1(std::uint32_t a)
    {
      std::uint64_t x = a & 0x00000000FFFFFFFF;
      x = (x | (x << 16)) & 0x0000FFFF0000FFFF;
      x = (x | (x << 8)) & 0x00FF00FF00FF00FF;
      x = (x | (x << 4)) & 0x0F0F0F0F0F0F0F0F;
      x = (x | (x << 2)) & 0x3333333333333333;
      x = (x | (x << 1)) & 0x5555555555555555;
      return x;
    }

    static constexpr std::uint64_t hash(std::uint64_t value)
    {
      value ^= value >> 30;
      value *= 0xbf58476d1ce4e5b9ULL;
      value ^= value >> 27;
      value *= 0x94d049bb133111ebULL;
      value ^= value >> 31;
      return value;
    }

    static constexpr std::uint64_t getKey(std::uint32_t zoom, std::uint32_t x, std::uint32_t y)
    {
      std::uint64_t morton = splitBy1(x) | (splitBy1(y) << 1);
      return (static_cast<std::uint64_t>(zoom) << 56) | morton;
    }

    constexpr std::uint64_t operator()(NodeCoords key) const { return hash(getKey(key.level, key.x, key.y)); }
  };

  LRUCache<NodeCoords, entt::entity, CAPACITY, NodeCoordsHasher, PROBE_LIMIT> cache;

  entt::entity createTile(const NodeCoords coords, TimePoint currentTime)
  {
    NodeCoords loadedCoords = coords;
    TextureHandle finalTextureHandle;

    // the tile image present on the disk, use it
    const auto tilePath = getTilePath("content/tiles/eskisehir", coords);
    auto rawFile = loadFileBinary(tilePath);
    if (!rawFile.empty())
    {
      finalTextureHandle = textureManager->allocate(256, 256);
      auto texture = textureManager->get(finalTextureHandle);
      const std::span<std::byte> memory = allocator->allocateTexture(texture);
      loadJPG(rawFile, memory);
    }

    // if the image file not found for the tile, use its parents texture
    else if (coords.level > 0)
    {
      const NodeCoords parentCoords{coords.level - 1, coords.x / 2, coords.y / 2};

      auto parent = getOrCreateTile(parentCoords, currentTime);
      if (parent != entt::null)
      {
        finalTextureHandle = registry->get<component::TextureHandle>(parent).value;
        textureManager->addRef(finalTextureHandle);
        loadedCoords = registry->get<NodeCoords>(parent);
      }
    }

    // no texture found for the tile in disk or from parents
    if (!finalTextureHandle.isValid())
    {
      return entt::null;
    }

    const auto vertexBuffer = allocator->createVertexBuffer(VERTEX_BUFFER_SIZE_PER_TILE);
    std::span<std::byte> vertexBufferMemory = allocator->allocateBuffer(vertexBuffer);
    std::span<gpu::Vertex> vertices(reinterpret_cast<gpu::Vertex*>(vertexBufferMemory.data()), NUM_VERTICES_PER_TILE);
    auto tileCenter = tileToECEF(coords.level, coords.x + 0.5, coords.y + 0.5);
    generateTileVertices(coords, loadedCoords, tileCenter, vertices);

    auto entity = registry->create();
    // for rendering
    registry->emplace<component::Position>(entity, tileCenter);
    registry->emplace<component::VertexBuffer>(entity, vertexBuffer.buffer);
    registry->emplace<component::Texture>(entity, textureManager->get(finalTextureHandle).texture);

    // for TextureManager
    registry->emplace<component::TextureHandle>(entity, finalTextureHandle);
    // remember the source of the tile data for proper UV mapping
    registry->emplace<NodeCoords>(entity, loadedCoords);

    // for culling
    const auto boundingSphere = generateTileBoundingSphere(vertices, tileCenter);
    const auto horizonCullingPoint = generateHorizonCullingPoint(vertices, tileCenter, boundingSphere);
    registry->emplace<component::BoundingSphere>(entity, boundingSphere);
    registry->emplace<component::HorizonCullingPoint>(entity, horizonCullingPoint);

    return entity;
  };

  void destroyTile(entt::entity entity)
  {
    auto vertexBufferComp = registry->get<component::VertexBuffer>(entity).value;
    allocator->releaseBuffer(vertexBufferComp);

    auto textureHandle = registry->get<component::TextureHandle>(entity).value;
    textureManager->release(textureHandle);

    registry->destroy(entity);
  }
};
} // namespace flb
