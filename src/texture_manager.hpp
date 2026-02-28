#pragma once

#include "gpu/allocator.hpp"

#include <cstdint>
#include <limits>
#include <vector>

namespace flb
{

struct TextureHandle
{
  std::uint32_t index = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t version = 0;

  bool isValid() const { return index != std::numeric_limits<std::uint32_t>::max(); }
  bool operator==(const TextureHandle& other) const = default;
};

class TextureManager
{
public:
  TextureHandle allocate(gpu::Allocator* allocator, int width, int height)
  {
    std::uint32_t index = getFreeSlot();
    std::uint32_t version = ++globalVersionCounter;

    auto texture = allocator->createTexture(width, height);

    Slot& slot = pool[index];
    slot.textureHandle = texture;
    slot.version = version;
    slot.refCount = 1;

    return {index, version};
  }

  void addRef(TextureHandle handle)
  {
    if (!handle.isValid() || handle.index >= pool.size())
    {
      return;
    }

    Slot& slot = pool[handle.index];
    if (slot.version == handle.version)
    {
      ++slot.refCount;
    }
  }

  gpu::TextureHandle get(TextureHandle handle) const
  {
    if (!handle.isValid() || handle.index >= pool.size())
      return {};

    const Slot& slot = pool[handle.index];

    // If the slot was freed and recycled, the version numbers won't match
    if (slot.version != handle.version)
      return {};

    return slot.textureHandle;
  }

  void destroy(TextureHandle handle, gpu::Allocator* allocator)
  {
    // If the magic number doesn't match, it was already destroyed. Safe exit!
    if (!handle.isValid() || handle.index >= pool.size())
      return;

    Slot& slot = pool[handle.index];

    if (slot.version != handle.version)
      return;

    --slot.refCount;

    if (slot.refCount != 0)
      return;

    allocator->releaseTexture(slot.textureHandle.texture);
    slot = {};
  }

private:
  struct Slot
  {
    gpu::TextureHandle textureHandle{NULL, 0, 0, 0};
    std::uint32_t version = 0;
    std::uint32_t refCount = 0;
  };

  std::vector<Slot> pool;
  std::vector<std::uint32_t> freeSlots;
  std::uint32_t globalVersionCounter = 0; // Increments on every allocation

  std::uint32_t getFreeSlot()
  {
    if (!freeSlots.empty())
    {
      auto idx = freeSlots.back();
      freeSlots.pop_back();
      return idx;
    }
    pool.emplace_back();
    return static_cast<std::uint32_t>(pool.size() - 1);
  }
};

} // namespace flb
