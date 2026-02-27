#pragma once

#include "gpu/pipeline.hpp"
#include "math.hpp"

#include <filesystem>
#include <vector>

namespace flb
{

static std::vector<TileCoords> getIntersectingTileCoords(const TilesetDescription& description)
{
  std::vector<TileCoords> coords;
  BBOX bounds = description.zoomRegion;
  for (std::uint32_t zoom = description.zoomMin; zoom <= description.zoomMax; ++zoom)
  {
    TileCoords sw = geoToTileCoords(bounds.min, zoom);
    TileCoords ne = geoToTileCoords(bounds.max, zoom);

    auto minx = sw.x;
    auto miny = ne.y;

    auto maxx = ne.x;
    auto maxy = sw.y;

    for (std::uint32_t x = minx; x <= maxx; ++x)
    {
      for (std::uint32_t y = miny; y <= maxy; ++y)
      {
        coords.emplace_back(zoom, x, y);
      }
    }
  }
  return coords;
}

static std::filesystem::path getTilePath(const std::filesystem::path& root, const TileCoords& coords)
{
  return root / std::to_string(coords.zoom) / std::to_string(coords.x) / (std::to_string(coords.y) + ".png");
}

/**
 * Generates a vertices for a given tile coordinate.
 * Calculates a grid of vertices with positions calculated based on the tile's location on the globe.
 * Writes the vertices to the provided span, which should have a size of (GRID_RESOLUTION + 1) * (GRID_RESOLUTION + 1)
 * to accommodate the grid.
 */
constexpr gpu::Index GRID_RESOLUTION = 16;
constexpr std::size_t NUM_VERTICES_PER_TILE = (GRID_RESOLUTION + 1) * (GRID_RESOLUTION + 1);
constexpr std::size_t VERTEX_BUFFER_SIZE_PER_TILE = NUM_VERTICES_PER_TILE * sizeof(gpu::Vertex);
static void generateTileVertices(
  const TileCoords& coords, const glm::dvec3& tileCenter, std::span<gpu::Vertex>& vertices)
{
  double coordx = static_cast<double>(coords.x);
  double coordy = static_cast<double>(coords.y);

  double n = glm::pow(2.0, coords.zoom);
  constexpr double a = SEMI_MAJOR;
  constexpr double b = SEMI_MINOR;
  constexpr double e2 = 1.0 - (b * b) / (a * a);

  for (gpu::Index i = 0; i <= GRID_RESOLUTION; ++i)
  {
    double v = static_cast<double>(i) / GRID_RESOLUTION;
    double tile_y = coordy + v;

    double lat = glm::atan(glm::sinh(PI * (1.0 - 2.0 * tile_y / n)));
    double sin_lat = glm::sin(lat);
    double cos_lat = glm::cos(lat);
    double N = a / glm::sqrt(1.0 - e2 * sin_lat * sin_lat);
    double z = N * (1.0 - e2) * sin_lat;

    for (gpu::Index j = 0; j <= GRID_RESOLUTION; ++j)
    {
      double u = static_cast<double>(j) / GRID_RESOLUTION;
      double tile_x = coordx + u;

      double lon = (tile_x / n) * 2.0 * PI - PI;
      double x = N * cos_lat * glm::cos(lon);
      double y = N * cos_lat * glm::sin(lon);

      glm::dvec3 posDouble{x, y, z};
      glm::dvec3 localPosDouble = posDouble - tileCenter;
      glm::vec3 position = glm::vec3(localPosDouble);

      glm::vec3 normal = glm::normalize(glm::dvec3(
        posDouble.x / SEMI_MAJOR_SQUARED, posDouble.y / SEMI_MAJOR_SQUARED, posDouble.z / SEMI_MINOR_SQUARED));

      gpu::Vertex vertex{
        .position = position,
        .normal = normal,
        .color = glm::vec3{1.0f},
        .uv = glm::vec2{u, v},
      };
      vertices[i * (GRID_RESOLUTION + 1) + j] = vertex;
    }
  }
}

/**
 * Generates indices for a tile grid based on the specified resolution.
 * The indices define two triangles for each quad in the grid, which will be used for rendering the tile.
 * The provided span should have a size of GRID_RESOLUTION * GRID_RESOLUTION * 6 to accommodate all the indices.
 */
constexpr std::size_t TILE_NUM_INDICES = GRID_RESOLUTION * GRID_RESOLUTION * 6;
constexpr std::size_t TILE_INDEX_BUFFER_SIZE = TILE_NUM_INDICES * sizeof(gpu::Index);
static void generateTileIndices(std::span<gpu::Index>& indices)
{
  std::size_t index = 0;
  for (gpu::Index i = 0; i < GRID_RESOLUTION; ++i)
  {
    for (gpu::Index j = 0; j < GRID_RESOLUTION; ++j)
    {
      gpu::Index topLeft = i * (GRID_RESOLUTION + 1) + j;
      gpu::Index topRight = topLeft + 1;
      gpu::Index bottomLeft = (i + 1) * (GRID_RESOLUTION + 1) + j;
      gpu::Index bottomRight = bottomLeft + 1;

      indices[index++] = topLeft;
      indices[index++] = bottomLeft;
      indices[index++] = topRight;

      indices[index++] = topRight;
      indices[index++] = bottomLeft;
      indices[index++] = bottomRight;
    }
  }
}

static std::vector<glm::dvec3> getTileOrigins(const std::vector<TileCoords>& tileCoords)
{
  std::vector<glm::dvec3> origins;
  origins.reserve(tileCoords.size());
  for (const auto& coords : tileCoords)
  {
    double coordx = static_cast<double>(coords.x);
    double coordy = static_cast<double>(coords.y);
    glm::dvec3 tileCenter = tileToECEF(coords.zoom, coordx + 0.5, coordy + 0.5);
    origins.push_back(tileCenter);
  }
  return origins;
}
} // namespace flb
