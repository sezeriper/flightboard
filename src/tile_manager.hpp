#pragma once

#include "components.hpp"
#include "gpu/allocator.hpp"
#include "math.hpp"
#include "texture_manager.hpp"
#include "tile_generator.hpp"
#include "time.hpp"

#include <entt/entt.hpp>

#include <cstdint>
#include <limits>

namespace flb
{
class TileManager
{
public:
  static constexpr std::size_t CAPACITY = 8192;

  void init(entt::registry* registry, gpu::Allocator* allocator)
  {
    this->registry = registry;
    this->allocator = allocator;

    const auto bufHandle = allocator->createIndexBuffer(TILE_INDEX_BUFFER_SIZE);
    tileIndexBuffer = bufHandle.buffer;
    const auto bufMemory = allocator->allocateBuffer(bufHandle);
    std::span<gpu::Index> indices(reinterpret_cast<gpu::Index*>(bufMemory.data()), TILE_NUM_INDICES);
    generateTileIndices(indices);

    registry->group<component::Position, component::VertexBuffer, component::IndexBuffer, component::Texture>();
  }

  void cleanup()
  {
    for (const auto& slot : slots)
    {
      if (slot.quadKey != NULL_KEY)
      {
        destroyTile(slot.entity);
      }
    }

    allocator->releaseBuffer(tileIndexBuffer);
  }

  entt::entity getOrCreateTile(std::uint32_t zoom, std::uint32_t x, std::uint32_t y, TimePoint currentTime)
  {
    auto quadKey = getQuadKey(zoom, x, y);
    auto startIndex = hash64(quadKey) & MASK;

    std::size_t insertIndex = startIndex;
    TimePoint oldestUsage = currentTime;

    // linear probing to find the tile or an empty slot
    for (std::size_t i = 0; i < PROBE_LIMIT; ++i)
    {
      std::size_t probeIndex = (startIndex + i) & MASK;
      Slot& slot = slots[probeIndex];

      // cache hit
      if (slot.quadKey == quadKey)
      {
        ++cacheHitCount;
        slot.lastUsed = currentTime;
        return slot.entity;
      }

      if (slot.quadKey == NULL_KEY)
      {
        insertIndex = probeIndex;
        break;
      }

      ++collisionCount;

      if (slot.lastUsed < oldestUsage)
      {
        oldestUsage = slot.lastUsed;
        insertIndex = probeIndex; // candidate for eviction
      }
    }

    // try to create the tile
    auto tile = createTile(zoom, x, y, currentTime);
    if (tile == entt::null)
      return entt::null;

    Slot& targetSlot = slots[insertIndex];
    if (targetSlot.quadKey != NULL_KEY)
    {
      destroyTile(targetSlot.entity);
    }

    targetSlot.quadKey = quadKey;
    targetSlot.lastUsed = currentTime;
    targetSlot.entity = tile;
    return targetSlot.entity;
  }

  // for debug purposes
  std::uint64_t cacheHitCount = 0;
  std::uint64_t collisionCount = 0;

private:
  static constexpr std::uint64_t MASK = CAPACITY - 1;
  static constexpr std::uint64_t NULL_KEY = std::numeric_limits<std::uint64_t>::max();
  static constexpr std::size_t PROBE_LIMIT = 16;
  entt::registry* registry;
  gpu::Allocator* allocator;
  TextureManager textureManager;

  struct Slot
  {
    std::uint64_t quadKey = NULL_KEY;
    TimePoint lastUsed = 0;
    entt::entity entity = entt::null;
  };
  std::array<Slot, CAPACITY> slots;

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

  // Scrambles the bits of a 64-bit integer to ensure uniform hash map distribution
  static constexpr std::uint64_t hash64(std::uint64_t key)
  {
    key ^= key >> 30;
    key *= 0xbf58476d1ce4e5b9ULL;
    key ^= key >> 27;
    key *= 0x94d049bb133111ebULL;
    key ^= key >> 31;
    return key;
  }

  static constexpr std::uint64_t getQuadKey(std::uint32_t zoom, std::uint32_t x, std::uint32_t y)
  {
    std::uint64_t morton = splitBy1(x) | (splitBy1(y) << 1);
    return (static_cast<std::uint64_t>(zoom) << 56) | morton;
  }

  /**
   * Index buffer shared by all tiles.
   */
  SDL_GPUBuffer* tileIndexBuffer;

  entt::entity createTile(std::uint32_t zoom, std::uint32_t x, std::uint32_t y, TimePoint currentTime)
  {
    TileCoords tileCoords{zoom, x, y};
    TileCoords loadedTileCoords = tileCoords;
    TextureHandle finalTextureHandle;

    // the tile image present on the disk, use it
    const auto tilePath = getTilePath("content/tiles/eskisehir", tileCoords);
    auto rawFile = loadFileBinary(tilePath);
    if (!rawFile.empty())
    {
      finalTextureHandle = textureManager.allocate(allocator, 256, 256);
      auto texture = textureManager.get(finalTextureHandle);
      const std::span<std::byte> memory = allocator->allocateTexture(texture);
      loadJPG(rawFile, memory);
    }

    // if the image file not found for the tile, use its parents texture
    else if (zoom > 0)
    {
      auto parentZoom = zoom - 1;
      auto parentX = x / 2;
      auto parentY = y / 2;

      auto parent = getOrCreateTile(parentZoom, parentX, parentY, currentTime);
      finalTextureHandle = registry->get<component::TextureHandle>(parent).value;
      textureManager.addRef(finalTextureHandle);
      loadedTileCoords = registry->get<TileCoords>(parent);
    }

    const auto vertexBuffer = allocator->createVertexBuffer(VERTEX_BUFFER_SIZE_PER_TILE);
    std::span<std::byte> vertexBufferMemory = allocator->allocateBuffer(vertexBuffer);
    std::span<gpu::Vertex> vertices(reinterpret_cast<gpu::Vertex*>(vertexBufferMemory.data()), NUM_VERTICES_PER_TILE);
    auto tileCenter = tileToECEF(zoom, x + 0.5, y + 0.5);
    generateTileVertices(tileCoords, loadedTileCoords, tileCenter, vertices);

    auto entity = registry->create();
    // for rendering
    registry->emplace<component::Position>(entity, tileCenter);
    registry->emplace<component::VertexBuffer>(entity, vertexBuffer.buffer);
    registry->emplace<component::IndexBuffer>(entity, tileIndexBuffer);
    registry->emplace<component::Texture>(entity, textureManager.get(finalTextureHandle).texture);

    // for TextureManager
    registry->emplace<component::TextureHandle>(entity, finalTextureHandle);
    // remember the source of the tile data for proper UV mapping
    registry->emplace<TileCoords>(entity, loadedTileCoords);
    return entity;
  };

  void destroyTile(entt::entity entity)
  {
    auto vertexBufferComp = registry->get<component::VertexBuffer>(entity).value;
    allocator->releaseBuffer(vertexBufferComp);

    auto textureHandle = registry->get<component::TextureHandle>(entity).value;
    textureManager.destroy(textureHandle, allocator);

    registry->destroy(entity);
  }
};
} // namespace flb
