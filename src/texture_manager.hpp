#pragma once

#include "gpu/allocator.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <vector>

namespace flb
{

struct TextureHandle
{
  static constexpr std::uint32_t InvalidIndex = std::numeric_limits<std::uint32_t>::max();

  std::uint32_t index = InvalidIndex;
  std::uint32_t generation = 0;

  bool isValid() const noexcept { return index != InvalidIndex; }

  bool operator==(const TextureHandle& other) const = default;
};

class TextureManager
{
public:
  void init(gpu::Allocator* allocator_) { allocator = allocator_; }

  TextureHandle allocate(int width, int height)
  {
    auto texture = allocator->createTexture(width, height);

    const std::uint32_t index = getFreeSlot();

    Slot& slot = pool[index];
    slot.textureHandle = texture;
    slot.refCount = 1;

    return {index, slot.generation};
  }

  void addRef(TextureHandle handle)
  {
    Slot* slot = getValidSlot(handle);
    if (!slot)
      return;

    assert(slot->refCount != std::numeric_limits<std::uint32_t>::max());
    ++slot->refCount;
  }

  gpu::TextureHandle get(TextureHandle handle) const
  {
    const Slot* slot = getValidSlot(handle);
    if (!slot)
      return {};

    return slot->textureHandle;
  }

  void release(TextureHandle handle)
  {
    Slot* slot = getValidSlot(handle);
    if (!slot)
      return;

    --slot->refCount;

    if (slot->refCount != 0)
      return;

    allocator->releaseTexture(slot->textureHandle.texture);

    slot->textureHandle = {};
    bumpGeneration(*slot);

    freeSlots.push_back(handle.index);
  }

private:
  struct Slot
  {
    gpu::TextureHandle textureHandle{};
    std::uint32_t generation = 1;
    std::uint32_t refCount = 0;
  };

  gpu::Allocator* allocator = nullptr;

  std::vector<Slot> pool;
  std::vector<std::uint32_t> freeSlots;

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

  Slot* getValidSlot(TextureHandle handle)
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

  const Slot* getValidSlot(TextureHandle handle) const
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
