#pragma once

#include "gpu/allocator.hpp"
#include "gpu/pipeline.hpp"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <vector>

namespace flb
{

struct MeshHandle
{
  static constexpr std::uint32_t InvalidIndex = std::numeric_limits<std::uint32_t>::max();

  std::uint32_t index = InvalidIndex;
  std::uint32_t generation = 0;

  bool isValid() const noexcept { return index != InvalidIndex; }

  bool operator==(const MeshHandle& other) const = default;
};

struct Mesh
{
  gpu::BufferHandle vertexBuffer{};
  gpu::BufferHandle indexBuffer{};
  Uint32 indexCount = 0;
};

class MeshManager
{
public:
  void init(gpu::Allocator* allocator_) { allocator = allocator_; }

  MeshHandle allocate(std::span<const gpu::Vertex> vertices, std::span<const gpu::Index> indices)
  {
    if (vertices.empty() || indices.empty())
      return {};

    if (
      vertices.size_bytes() > std::numeric_limits<Uint32>::max() ||
      indices.size_bytes() > std::numeric_limits<Uint32>::max())
    {
      return {};
    }

    Mesh mesh{
      .vertexBuffer = allocator->createVertexBuffer(static_cast<Uint32>(vertices.size_bytes())),
      .indexBuffer = allocator->createIndexBuffer(static_cast<Uint32>(indices.size_bytes())),
      .indexCount = static_cast<Uint32>(indices.size()),
    };

    if (mesh.vertexBuffer.buffer == nullptr || mesh.indexBuffer.buffer == nullptr)
    {
      releaseMesh(mesh);
      return {};
    }

    const std::span<std::byte> vertexMemory = allocator->allocateBuffer(mesh.vertexBuffer);
    const std::span<std::byte> indexMemory = allocator->allocateBuffer(mesh.indexBuffer);
    if (vertexMemory.empty() || indexMemory.empty())
    {
      releaseMesh(mesh);
      return {};
    }

    std::memcpy(vertexMemory.data(), vertices.data(), mesh.vertexBuffer.size);
    std::memcpy(indexMemory.data(), indices.data(), mesh.indexBuffer.size);

    const std::uint32_t index = getFreeSlot();

    Slot& slot = pool[index];
    slot.mesh = mesh;
    slot.refCount = 1;

    return {index, slot.generation};
  }

  void addRef(MeshHandle handle)
  {
    Slot* slot = getValidSlot(handle);
    if (!slot)
      return;

    assert(slot->refCount != std::numeric_limits<std::uint32_t>::max());
    ++slot->refCount;
  }

  Mesh get(MeshHandle handle) const
  {
    const Slot* slot = getValidSlot(handle);
    if (!slot)
      return {};

    return slot->mesh;
  }

  void release(MeshHandle handle)
  {
    Slot* slot = getValidSlot(handle);
    if (!slot)
      return;

    --slot->refCount;

    if (slot->refCount != 0)
      return;

    releaseMesh(slot->mesh);

    slot->mesh = {};
    bumpGeneration(*slot);

    freeSlots.push_back(handle.index);
  }

private:
  struct Slot
  {
    Mesh mesh{};
    std::uint32_t generation = 1;
    std::uint32_t refCount = 0;
  };

  gpu::Allocator* allocator = nullptr;

  std::vector<Slot> pool;
  std::vector<std::uint32_t> freeSlots;

  void releaseMesh(const Mesh& mesh)
  {
    allocator->releaseBuffer(mesh.vertexBuffer.buffer);
    allocator->releaseBuffer(mesh.indexBuffer.buffer);
  }

  std::uint32_t getFreeSlot()
  {
    if (!freeSlots.empty())
    {
      const std::uint32_t index = freeSlots.back();
      freeSlots.pop_back();
      return index;
    }

    pool.emplace_back();
    return static_cast<std::uint32_t>(pool.size() - 1);
  }

  Slot* getValidSlot(MeshHandle handle)
  {
    if (!handle.isValid() || handle.index >= pool.size())
      return nullptr;

    Slot& slot = pool[handle.index];

    if (slot.refCount == 0)
      return nullptr;

    if (slot.generation != handle.generation)
      return nullptr;

    return &slot;
  }

  const Slot* getValidSlot(MeshHandle handle) const
  {
    if (!handle.isValid() || handle.index >= pool.size())
      return nullptr;

    const Slot& slot = pool[handle.index];

    if (slot.refCount == 0)
      return nullptr;

    if (slot.generation != handle.generation)
      return nullptr;

    return &slot;
  }

  static void bumpGeneration(Slot& slot)
  {
    ++slot.generation;

    // Keep generation 0 reserved for invalid/default handles.
    if (slot.generation == 0)
      ++slot.generation;
  }
};

} // namespace flb
