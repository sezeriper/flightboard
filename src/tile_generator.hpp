#pragma once

#include "culling.hpp"
#include "gpu/pipeline.hpp"
#include "math.hpp"

#include <filesystem>
#include <vector>

namespace flb
{

static std::vector<NodeCoords> getIntersectingTileCoords(const TilesetDescription& description)
{
  std::vector<NodeCoords> coords;
  BBOX bounds = description.zoomRegion;
  for (std::uint32_t zoom = description.zoomMin; zoom <= description.zoomMax; ++zoom)
  {
    NodeCoords sw = geoToTileCoords(bounds.min, zoom);
    NodeCoords ne = geoToTileCoords(bounds.max, zoom);

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

static std::filesystem::path getTilePath(const std::filesystem::path& root, const NodeCoords& coords)
{
  return root / std::to_string(coords.level) / std::to_string(coords.x) / (std::to_string(coords.y) + ".png");
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
  const NodeCoords childTile, const NodeCoords parentTile, const ECEFCoords tileCenter, std::span<gpu::Vertex> vertices)
{
  const double coordx = static_cast<double>(childTile.x);
  const double coordy = static_cast<double>(childTile.y);

  const double n = glm::pow(2.0, childTile.level);
  constexpr double a = SEMI_MAJOR;
  constexpr double b = SEMI_MINOR;
  constexpr double e2 = 1.0 - (b * b) / (a * a);

  // If no fallback occurred, levelDiff is 0, scale is 1.0, and offsets are 0.0
  const std::uint32_t levelDiff = childTile.level - parentTile.level;
  const double uvScale = 1.0 / static_cast<double>(1 << levelDiff);
  const double uvOffsetX = static_cast<double>(childTile.x - (parentTile.x << levelDiff)) * uvScale;
  const double uvOffsetY = static_cast<double>(childTile.y - (parentTile.y << levelDiff)) * uvScale;

  std::uint32_t h = childTile.x * 374761393U + childTile.y * 668265263U + childTile.level * 314159265U;
  h = (h ^ (h >> 13)) * 1274126177U;
  glm::vec3 tileColor{((h >> 16) & 0xFF) / 255.0f, ((h >> 8) & 0xFF) / 255.0f, (h & 0xFF) / 255.0f};

  for (gpu::Index i = 0; i <= GRID_RESOLUTION; ++i)
  {
    const double v = static_cast<double>(i) / GRID_RESOLUTION;
    const double tile_y = coordy + v;

    const double lat = glm::atan(glm::sinh(PI * (1.0 - 2.0 * tile_y / n)));
    const double sin_lat = glm::sin(lat);
    const double cos_lat = glm::cos(lat);
    const double N = a / glm::sqrt(1.0 - e2 * sin_lat * sin_lat);
    const double z = N * (1.0 - e2) * sin_lat;

    for (gpu::Index j = 0; j <= GRID_RESOLUTION; ++j)
    {
      const double u = static_cast<double>(j) / GRID_RESOLUTION;
      const double tile_x = coordx + u;

      const double lon = (tile_x / n) * 2.0 * PI - PI;
      const double x = N * cos_lat * glm::cos(lon);
      const double y = N * cos_lat * glm::sin(lon);

      const glm::dvec3 posDouble{x, y, z};
      const glm::dvec3 localPosDouble = posDouble - tileCenter;
      const glm::vec3 position = glm::vec3(localPosDouble);

      const glm::vec3 normal = glm::normalize(glm::dvec3(
        posDouble.x / SEMI_MAJOR_SQUARED, posDouble.y / SEMI_MAJOR_SQUARED, posDouble.z / SEMI_MINOR_SQUARED));

      const float finalU = u * uvScale + uvOffsetX;
      const float finalV = v * uvScale + uvOffsetY;

      const gpu::Vertex vertex{
        .position = position,
        .normal = normal,
        .color = tileColor,
        .uv = glm::vec2{finalU, finalV},
      };
      vertices[i * (GRID_RESOLUTION + 1) + j] = vertex;
    }
  }
}

static BoundingSphere generateTileBoundingSphere(const std::span<gpu::Vertex>& vertices, const glm::dvec3& tileCenter)
{
  if (vertices.empty())
    return {tileCenter, 0.0};

  glm::vec3 minPos = vertices[0].position;
  glm::vec3 maxPos = vertices[0].position;

  for (const auto& v : vertices)
  {
    minPos = glm::min(minPos, v.position);
    maxPos = glm::max(maxPos, v.position);
  }

  glm::vec3 localCenter = (minPos + maxPos) * 0.5f;

  float maxDist = 0.0f;
  for (const auto& v : vertices)
  {
    float dist = glm::distance(v.position, localCenter);
    if (dist > maxDist)
    {
      maxDist = dist;
    }
  }

  return {tileCenter + glm::dvec3(localCenter), static_cast<double>(maxDist)};
}

static BoundingSphere generateBoundingSphereLoose(std::uint32_t level, std::uint32_t x, std::uint32_t y)
{
  // 1. Get the mathematical center of the tile
  const glm::dvec3 center = tileToECEF(level, x + 0.5, y + 0.5);

  // 2. Get the 4 mathematical corners
  const glm::dvec3 c0 = tileToECEF(level, x, y);
  const glm::dvec3 c1 = tileToECEF(level, x + 1, y);
  const glm::dvec3 c2 = tileToECEF(level, x, y + 1);
  const glm::dvec3 c3 = tileToECEF(level, x + 1, y + 1);

  // 3. Find the maximum distance from the center to any corner
  const double r0 = glm::distance(center, c0);
  const double r1 = glm::distance(center, c1);
  const double r2 = glm::distance(center, c2);
  const double r3 = glm::distance(center, c3);

  const double maxRadius = std::max({r0, r1, r2, r3});

  return {center, maxRadius};
}

// Assuming Ellipsoid::scaleToUnitSphere is defined in your namespace as before.

static glm::dvec3 generateHorizonCullingPoint(
  const std::span<gpu::Vertex>& vertices, const glm::dvec3& tileCenter, const BoundingSphere& boundingSphere)
{
  if (vertices.empty())
  {
    return {};
  }

  // 1. Compute the scaled-space direction to the point.
  // Cesium recommends using the direction from the ellipsoid center to the bounding sphere center.
  const glm::dvec3 directionToPoint = boundingSphere.position;

  if (glm::length(directionToPoint) < 1e-10) // Guard against ZERO vector
  {
    return {};
  }

  glm::dvec3 scaledSpaceDirectionToPoint = glm::normalize(Ellipsoid::scaleToUnitSphere(directionToPoint));
  double resultMagnitude = 0.0;

  // 2. Iterate over all vertices to find the maximum scaled magnitude
  for (const auto& v : vertices)
  {
    // Convert local vertex position to ECEF by adding the tile center
    glm::dvec3 positionEcef = tileCenter + glm::dvec3(v.position);

    // --- Start of computeMagnitude equivalent ---
    glm::dvec3 scaledSpacePos = Ellipsoid::scaleToUnitSphere(positionEcef);
    double magnitudeSquared = glm::dot(scaledSpacePos, scaledSpacePos);
    double magnitude = std::sqrt(magnitudeSquared);
    glm::dvec3 direction = scaledSpacePos / magnitude;

    // For the purpose of this computation, points below the ellipsoid are considered to be on it instead.
    magnitudeSquared = std::max(1.0, magnitudeSquared);
    magnitude = std::max(1.0, magnitude);

    double cosAlpha = glm::dot(direction, scaledSpaceDirectionToPoint);
    double sinAlpha = glm::length(glm::cross(direction, scaledSpaceDirectionToPoint));
    double cosBeta = 1.0 / magnitude;
    double sinBeta = std::sqrt(magnitudeSquared - 1.0) * cosBeta;

    double denominator = (cosAlpha * cosBeta) - (sinAlpha * sinBeta);
    double candidateMagnitude = 1.0 / denominator;
    // --- End of computeMagnitude equivalent ---

    if (candidateMagnitude < 0.0)
    {
      // All points should face the same direction, but this one doesn't, so return nullopt (undefined).
      return {};
    }

    resultMagnitude = std::max(resultMagnitude, candidateMagnitude);
  }

  // 3. Convert the magnitude back to a point
  if (resultMagnitude <= 0.0 || std::isinf(resultMagnitude) || std::isnan(resultMagnitude))
  {
    return {};
  }

  return scaledSpaceDirectionToPoint * resultMagnitude;
}

static glm::dvec3 generateHorizonCullingPointLoose(const BoundingSphere& boundingSphere)
{
  const glm::dvec3 directionToPoint = boundingSphere.position;

  if (glm::length(directionToPoint) < 1e-10) // Guard against ZERO vector
  {
    return {};
  }

  glm::dvec3 scaledSpaceDirectionToPoint = glm::normalize(Ellipsoid::scaleToUnitSphere(directionToPoint));
  double resultMagnitude = 0.0;

  const glm::dvec3& c = boundingSphere.position;
  const double r = boundingSphere.radius;

  // Generate 8 proxy points representing the AABB of the bounding sphere.
  // This perfectly encapsulates the sphere, ensuring conservative culling.
  const std::array<glm::dvec3, 8> proxyVertices = {
    c + glm::dvec3(r, r, r),
    c + glm::dvec3(r, r, -r),
    c + glm::dvec3(r, -r, r),
    c + glm::dvec3(r, -r, -r),
    c + glm::dvec3(-r, r, r),
    c + glm::dvec3(-r, r, -r),
    c + glm::dvec3(-r, -r, r),
    c + glm::dvec3(-r, -r, -r)};

  // Iterate over the 8 proxy corners to find the maximum scaled magnitude
  for (const auto& positionEcef : proxyVertices)
  {
    glm::dvec3 scaledSpacePos = Ellipsoid::scaleToUnitSphere(positionEcef);
    double magnitudeSquared = glm::dot(scaledSpacePos, scaledSpacePos);
    double magnitude = std::sqrt(magnitudeSquared);
    glm::dvec3 direction = scaledSpacePos / magnitude;

    // Points below the ellipsoid are considered to be on it.
    magnitudeSquared = std::max(1.0, magnitudeSquared);
    magnitude = std::max(1.0, magnitude);

    double cosAlpha = glm::dot(direction, scaledSpaceDirectionToPoint);
    double sinAlpha = glm::length(glm::cross(direction, scaledSpaceDirectionToPoint));
    double cosBeta = 1.0 / magnitude;
    double sinBeta = std::sqrt(magnitudeSquared - 1.0) * cosBeta;

    double denominator = (cosAlpha * cosBeta) - (sinAlpha * sinBeta);
    double candidateMagnitude = 1.0 / denominator;

    if (candidateMagnitude < 0.0)
    {
      return {};
    }

    resultMagnitude = std::max(resultMagnitude, candidateMagnitude);
  }

  if (resultMagnitude <= 0.0 || std::isinf(resultMagnitude) || std::isnan(resultMagnitude))
  {
    return {}; // Undefined if we computed NaN or infinity
  }

  return scaledSpaceDirectionToPoint * resultMagnitude;
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

static std::vector<glm::dvec3> getTileOrigins(const std::vector<NodeCoords>& tileCoords)
{
  std::vector<glm::dvec3> origins;
  origins.reserve(tileCoords.size());
  for (const auto& coords : tileCoords)
  {
    double coordx = static_cast<double>(coords.x);
    double coordy = static_cast<double>(coords.y);
    glm::dvec3 tileCenter = tileToECEF(coords.level, coordx + 0.5, coordy + 0.5);
    origins.push_back(tileCenter);
  }
  return origins;
}
} // namespace flb
