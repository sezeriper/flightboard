#pragma once

#include "components.hpp"
#include "gpu/allocator.hpp"
#include "math.hpp"
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
  static constexpr std::size_t CAPACITY = 4096;

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
      if (slot.quadKey != NULL_KEY && slot.entity != entt::null)
      {
        destroyTile(slot.entity);
      }
    }

    allocator->releaseBuffer(tileIndexBuffer);
  }

  entt::entity getTile(std::uint32_t zoom, std::uint32_t x, std::uint32_t y, TimePoint currentTime)
  {
    auto quadKey = getQuadKey(zoom, x, y);
    auto startIndex = quadKey & MASK;

    std::size_t insertIndex = slots.size();
    TimePoint oldestUsage = currentTime;

    // linear probing to find the tile or an empty slot
    for (std::size_t i = 0; i < slots.size(); ++i)
    {
      std::size_t probeIndex = (startIndex + i) & MASK;
      Slot& slot = slots[probeIndex];

      if (slot.quadKey == NULL_KEY)
      {
        if (insertIndex == slots.size())
        {
          insertIndex = probeIndex;
        }
        break; // stop probing after finding an empty slot
      }

      std::uint64_t baseKey = slot.quadKey & ~FAILURE_BIT;
      if (baseKey == quadKey)
      {
        slot.lastUsed = currentTime;

        if ((slot.quadKey & FAILURE_BIT) != 0)
        {
          return entt::null;
        }
        return slot.entity;
      }

      if (slot.lastUsed < oldestUsage)
      {
        oldestUsage = slot.lastUsed;
        insertIndex = probeIndex; // candidate for eviction
      }
    }

    // This should never happen since we always have an eviction candidate
    if (insertIndex == slots.size())
    {
      SDL_Log("TileManager is full and no eviction candidate found!");
      return entt::null;
    }

    Slot& targetSlot = slots[insertIndex];
    if (targetSlot.quadKey != NULL_KEY && targetSlot.entity != entt::null)
    {
      destroyTile(targetSlot.entity);
    }

    targetSlot = {};

    targetSlot.entity = createTile(zoom, x, y);
    targetSlot.lastUsed = currentTime;

    if (targetSlot.entity == entt::null)
    {
      targetSlot.quadKey = quadKey | FAILURE_BIT;
      return entt::null;
    }

    targetSlot.quadKey = quadKey;
    return targetSlot.entity;
  }

private:
  static constexpr std::uint64_t MASK = CAPACITY - 1;
  static constexpr std::uint64_t NULL_KEY = std::numeric_limits<std::uint64_t>::max();
  static constexpr std::uint64_t FAILURE_BIT = 1ULL << 63;
  entt::registry* registry;
  gpu::Allocator* allocator;

  struct Slot
  {
    std::uint64_t quadKey = NULL_KEY;
    std::uint64_t lastUsed = 0;
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

  static constexpr std::uint64_t getQuadKey(std::uint32_t zoom, std::uint32_t x, std::uint32_t y)
  {
    std::uint64_t morton = splitBy1(x) | (splitBy1(y) << 1);
    return (morton << 8) | (zoom & 0xFF);
  }

  /**
   * Index buffer shared by all tiles.
   */
  SDL_GPUBuffer* tileIndexBuffer;

  entt::entity createTile(std::uint32_t zoom, std::uint32_t x, std::uint32_t y)
  {
    TileCoords tileCoords{zoom, x, y};

    const auto tilePath = getTilePath("content/tiles/eskisehir", tileCoords);
    auto rawFile = loadFileBinary(tilePath);
    if (rawFile.empty())
      return entt::null;

    const auto texture = allocator->createTexture(256, 256);
    std::span<std::byte> memory = allocator->allocateTexture(texture);
    loadJPG(rawFile, memory);

    const auto vertexBuffer = allocator->createVertexBuffer(VERTEX_BUFFER_SIZE_PER_TILE);
    std::span<std::byte> vertexBufferMemory = allocator->allocateBuffer(vertexBuffer);
    std::span<gpu::Vertex> vertices(reinterpret_cast<gpu::Vertex*>(vertexBufferMemory.data()), NUM_VERTICES_PER_TILE);
    auto tileCenter = tileToECEF(zoom, x + 0.5, y + 0.5);
    generateTileVertices(tileCoords, tileCenter, vertices);

    auto entity = registry->create();
    registry->emplace<component::Position>(entity, tileCenter);
    registry->emplace<component::VertexBuffer>(entity, vertexBuffer.buffer);
    registry->emplace<component::IndexBuffer>(entity, tileIndexBuffer);
    registry->emplace<component::Texture>(entity, texture.texture);
    return entity;
  };

  void destroyTile(entt::entity entity)
  {
    auto vertexBufferComp = registry->get<component::VertexBuffer>(entity).value;
    allocator->releaseBuffer(vertexBufferComp);

    auto textureComp = registry->get<component::Texture>(entity).value;
    allocator->releaseTexture(textureComp);

    registry->destroy(entity);
  }
};
} // namespace flb
