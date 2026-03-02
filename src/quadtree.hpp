/**
 * A quadtree class specialized for generating lod for tile maps.
 */

#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace flb
{
struct NodeCoords
{
  std::uint32_t level;
  std::uint32_t x;
  std::uint32_t y;

  constexpr bool operator==(const NodeCoords& other) const = default;
};

using NodeID = std::uint32_t;
constexpr NodeID NULL_NODE = std::numeric_limits<NodeID>::max();
struct Node
{
  std::array<NodeID, 4> children{NULL_NODE, NULL_NODE, NULL_NODE, NULL_NODE};
};

template <typename F>
concept ShouldSplitFunction = std::invocable<F, NodeCoords> && std::same_as<std::invoke_result_t<F, NodeCoords>, bool>;

template <typename F>
concept ProcessLeafeFunction = std::invocable<F, NodeCoords> && std::same_as<std::invoke_result_t<F, NodeCoords>, void>;

class QuadTree
{
public:
  std::size_t size() const { return nodes.size(); }
  void reserve(std::size_t size) { nodes.reserve(size); }

  void clear()
  {
    root = NULL_NODE;
    nodes.clear();
  }

  /**
   * Builds the quadtree using the provided shouldSplit function. The shouldSplit function takes
   * the associated NodeCoords for each node as an argument and returns a boolean indicating whether the node should be
   * split.
   */
  template <ShouldSplitFunction F>
  void build(F shouldSplit)
  {
    nodes.clear();
    root = createNode();
    NodeCoords rootCoords{0, 0, 0};
    buildRecursive(root, rootCoords, shouldSplit);
  }

  template <ProcessLeafeFunction F>
  void traverseLeaves(F callback)
  {
    traverseLeavesRecursive(root, {0, 0, 0}, callback);
  }

  static constexpr std::array<NodeCoords, 4> getChildren(const NodeCoords& coords)
  {
    std::array<NodeCoords, 4> children;
    for (std::uint32_t y = 0; y < 2; ++y)
    {
      for (std::uint32_t x = 0; x < 2; ++x)
      {
        NodeCoords childCoords{coords.level + 1, coords.x * 2 + x, coords.y * 2 + y};
        children[y * 2 + x] = childCoords;
      }
    }
    return children;
  }

private:
  std::vector<Node> nodes;
  NodeID root = NULL_NODE;

  NodeID createNode()
  {
    nodes.emplace_back();
    return static_cast<NodeID>(nodes.size() - 1);
  }

  void buildRecursive(NodeID id, NodeCoords coords, auto&& shouldSplit)
  {
    if (!shouldSplit(coords))
      return;

    for (std::uint32_t y = 0; y < 2; ++y)
    {
      for (std::uint32_t x = 0; x < 2; ++x)
      {
        NodeID childId = createNode();
        nodes[id].children[y * 2 + x] = childId;
        NodeCoords childCoords{coords.level + 1, coords.x * 2 + x, coords.y * 2 + y};
        buildRecursive(childId, childCoords, shouldSplit);
      }
    }
  }

  void traverseLeavesRecursive(NodeID id, NodeCoords coords, auto&& callback)
  {
    // checking first child is sufficient to determine if it's a leaf node, since if the first child is null, all
    // children will be null.
    if (nodes[id].children[0] == NULL_NODE)
    {
      callback(coords);
    }
    else
    {
      for (std::uint32_t y = 0; y < 2; ++y)
      {
        for (std::uint32_t x = 0; x < 2; ++x)
        {
          NodeID childId = nodes[id].children[y * 2 + x];
          NodeCoords childCoords{coords.level + 1, coords.x * 2 + x, coords.y * 2 + y};
          traverseLeavesRecursive(childId, childCoords, callback);
        }
      }
    }
  }
};
} // namespace flb
