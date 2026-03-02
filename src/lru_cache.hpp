#pragma once

#include "time.hpp"

#include <array>
#include <functional>
#include <optional>
#include <utility>

namespace flb
{

// A fixed-capacity cache with bounded probe LRU eviction using open addressing and linear probing.
// Note: This relies on types being DefaultConstructible due to the underlying std::array usage.
template <
  typename Key,
  typename Value,
  std::size_t Capacity,
  typename Hasher = std::hash<Key>,
  std::size_t ProbeLimit = 16>
class LRUCache
{
  static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");
  static_assert(ProbeLimit <= Capacity, "ProbeLimit cannot exceed Capacity");

public:
  LRUCache() = default;

  // Retrieves the value for the given key. Updates the LRU timestamp on a hit.
  std::optional<Value> get(const Key& key, TimePoint currentTime)
  {
    std::size_t startIndex = hasher(key) & MASK;

    for (std::size_t i = 0; i < ProbeLimit; ++i)
    {
      std::size_t probeIndex = (startIndex + i) & MASK;
      Slot& slot = slots[probeIndex];

      if (slot.occupied && slot.key == key)
      {
        slot.lastUsed = currentTime;
        return slot.value;
      }

      if (!slot.occupied)
      {
        break;
      }
    }

    return std::nullopt;
  }

  // Inserts a key-value pair. If an eviction occurs, returns the evicted key-value pair.
  std::optional<std::pair<Key, Value>> insert(Key key, Value value, TimePoint currentTime)
  {
    std::size_t startIndex = hasher(key) & MASK;
    std::size_t insertIndex = startIndex;
    TimePoint oldestUsage = currentTime;

    for (std::size_t i = 0; i < ProbeLimit; ++i)
    {
      std::size_t probeIndex = (startIndex + i) & MASK;
      Slot& slot = slots[probeIndex];

      // If key already exists, update value and return the old value as evicted
      if (slot.occupied && slot.key == key)
      {
        std::optional<std::pair<Key, Value>> evicted = std::make_pair(std::move(slot.key), std::move(slot.value));
        slot.value = std::move(value);
        slot.lastUsed = currentTime;
        return evicted;
      }

      if (!slot.occupied)
      {
        insertIndex = probeIndex;
        break;
      }

      if (slot.lastUsed < oldestUsage)
      {
        oldestUsage = slot.lastUsed;
        insertIndex = probeIndex;
      }
    }

    Slot& targetSlot = slots[insertIndex];
    std::optional<std::pair<Key, Value>> evicted = std::nullopt;

    if (targetSlot.occupied)
    {
      evicted = std::make_pair(std::move(targetSlot.key), std::move(targetSlot.value));
    }

    targetSlot.key = std::move(key);
    targetSlot.value = std::move(value);
    targetSlot.lastUsed = currentTime;
    targetSlot.occupied = true;

    return evicted;
  }

  // Iterates over all occupied slots, calls onEvict, and clears the map.
  template <typename Func>
  void clear(Func onEvict)
  {
    for (auto& slot : slots)
    {
      if (slot.occupied)
      {
        onEvict(std::move(slot.key), std::move(slot.value));
        slot.occupied = false;
      }
    }
  }

  // Clears the map without triggering callbacks.
  void clear()
  {
    for (auto& slot : slots)
    {
      slot.occupied = false;
    }
  }

private:
  static constexpr std::size_t MASK = Capacity - 1;

  struct Slot
  {
    TimePoint lastUsed = 0;
    Key key{};
    Value value{};
    bool occupied = false;
  };

  std::array<Slot, Capacity> slots;
  Hasher hasher;
};

} // namespace flb
