#pragma once

#include "utils.hpp"
#include "device.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <cstdint>
#include <filesystem>
#include <vector>
#include <numbers>

namespace
{
using real_t = double;
using int_t = std::uint32_t;

constexpr real_t PI = std::numbers::pi_v<real_t>;
constexpr real_t MIN_LATITUDE = -85.05112878;
constexpr real_t MAX_LATITUDE = 85.05112878;
constexpr real_t MIN_LONGITUDE = -180.0;
constexpr real_t MAX_LONGITUDE = 180.0;
constexpr real_t SEMI_MAJOR = 6378137.0;
constexpr real_t SEMI_MINOR = 6356752.3142451793;
constexpr real_t SEMI_MAJOR_SQUARED = SEMI_MAJOR * SEMI_MAJOR;
constexpr real_t SEMI_MINOR_SQUARED = SEMI_MINOR * SEMI_MINOR;
}

namespace flb
{
namespace map
{

struct TileCoords
{
  int_t zoom;
  int_t x;
  int_t y;
};

struct GeoCoords
{
  real_t latitude;
  real_t longitude;
};

struct BBOX
{
  GeoCoords min;
  GeoCoords max;
};

struct Tile
{
  TileCoords coords;
  Texture texture;
};

struct Tileset
{
  int_t tilePixelSize;
  int_t zoomMin;
  int_t zoomMax;
  std::vector<Tile> tiles;
};

using ECEFCoords = glm::dvec3;

static TileCoords geoToTileCoords(const GeoCoords& from, const int_t& zoom)
{
  const real_t latitude = glm::clamp(from.latitude, MIN_LATITUDE, MAX_LATITUDE);
  const real_t longitude = glm::clamp(from.longitude, MIN_LONGITUDE, MAX_LONGITUDE);

  const real_t x = (longitude + 180.0) / 360.0;
  const real_t sinLat = glm::sin(latitude * PI / 180.0);
  const real_t y = 0.5 - glm::log((1.0 + sinLat) / (1.0 - sinLat)) / (4.0 * PI);

  const real_t numTiles = glm::pow(2.0, zoom);
  const int_t tileX = static_cast<int_t>(glm::floor(x * numTiles));
  const int_t tileY = static_cast<int_t>(glm::floor(y * numTiles));
  return { zoom, tileX, tileY };
}

/**
 * Converts geographic coordinates (latitude and longitude) to ECEF (Earth-Centered, Earth-Fixed) coordinates.
 */
static ECEFCoords geoToECEF(const GeoCoords& geo)
{
  const real_t latRad = glm::radians(geo.latitude);
  const real_t lonRad = glm::radians(geo.longitude);

  constexpr real_t a = SEMI_MAJOR;
  constexpr real_t b = SEMI_MINOR;
  constexpr real_t e2 = 1.0 - (b * b) / (a * a);

  real_t sinLat = glm::sin(latRad);
  real_t cosLat = glm::cos(latRad);
  real_t N = a / glm::sqrt(1.0 - e2 * sinLat * sinLat);

  real_t x = N * cosLat * glm::cos(lonRad);
  real_t y = N * cosLat * glm::sin(lonRad);
  real_t z = N * (1.0 - e2) * sinLat;

  return { x, y, z };
}

/**
 * Converts geographic coordinates (latitude and longitude) and height above ellipsoid to
 * ECEF (Earth-Centered, Earth-Fixed) coordinates.
 */
static ECEFCoords geoToECEF(const GeoCoords& geo, real_t height)
{
  const real_t latRad = glm::radians(geo.latitude);
  const real_t lonRad = glm::radians(geo.longitude);

  constexpr real_t a = SEMI_MAJOR;
  constexpr real_t b = SEMI_MINOR;
  constexpr real_t e2 = 1.0 - (b * b) / (a * a);

  real_t sinLat = glm::sin(latRad);
  real_t cosLat = glm::cos(latRad);
  real_t N = a / glm::sqrt(1.0 - e2 * sinLat * sinLat);

  real_t x = (N + height) * cosLat * glm::cos(lonRad);
  real_t y = (N + height) * cosLat * glm::sin(lonRad);
  real_t z = (N * (1.0 - e2) + height) * sinLat;

  return { x, y, z };
}

/**
 * Converts fractional tile coordinates to geographic coordinates (latitude and longitude) in radians.
 */
static ECEFCoords tileToECEF(real_t tile_x, real_t tile_y, int_t tile_zoom)
{
  real_t n = glm::pow(2.0, tile_zoom);
  real_t lon = (tile_x / n) * 2.0 * PI - PI;
  double lat = glm::atan(glm::sinh(PI * (1.0 - 2.0 * tile_y / n)));

  constexpr real_t a = SEMI_MAJOR;
  constexpr real_t b = SEMI_MINOR;
  constexpr real_t e2 = 1.0 - (b * b) / (a * a);

  real_t sin_lat = glm::sin(lat);
  real_t cos_lat = glm::cos(lat);
  real_t N = a / glm::sqrt(1.0 - e2 * sin_lat * sin_lat);

  real_t x = N * cos_lat * glm::cos(lon);
  real_t y = N * cos_lat * glm::sin(lon);
  real_t z = N * (1.0 - e2) * sin_lat;

  return { x, y, z };
}

static Tileset load(const std::filesystem::path root, const BBOX bounds)
{
  Tileset tileset {
    .tilePixelSize = 256,
    .zoomMin = 1,
    .zoomMax = 19,
  };

  for (int_t zoom = tileset.zoomMin; zoom <= tileset.zoomMax; ++zoom)
  {
    const TileCoords sw = geoToTileCoords(bounds.min, zoom);
    const TileCoords ne = geoToTileCoords(bounds.max, zoom);

    auto minx = sw.x;
    auto miny = ne.y;

    auto maxx = ne.x;
    auto maxy = sw.y;

    for (int_t x = minx; x <= maxx; ++x)
    {
      for (int_t y = miny; y <= maxy; ++y)
      {
        const std::filesystem::path tilePath = root / std::to_string(zoom) / std::to_string(x) / (std::to_string(y) + ".png");
        auto image = loadJPG(tilePath);
        if (image.data == nullptr)
        {
          return tileset;
        }
        tileset.tiles.emplace_back(TileCoords{zoom, x, y}, std::move(image));
      }
    }
  }
  return tileset;
}

/**
 * Generates a mesh for a given tile coordinate.
 * The mesh is a grid of vertices with positions calculated based on the tile's location on the globe.
 * returns a tuple containing the generated mesh and the tile's center position in ECEF coordinates.
 */
constexpr int_t GRID_RESOLUTION = 16;
static std::tuple<Mesh, Position> generateTileMesh(TileCoords coords)
{
  Mesh mesh;
  const std::size_t num_vertices = (GRID_RESOLUTION + 1) * (GRID_RESOLUTION + 1);
  const std::size_t num_indices = GRID_RESOLUTION * GRID_RESOLUTION * 6;
  mesh.vertices.reserve(num_vertices);
  mesh.indices.reserve(num_indices);

  real_t coordx = static_cast<real_t>(coords.x);
  real_t coordy = static_cast<real_t>(coords.y);
  glm::dvec3 tileCenter = tileToECEF(coordx + 0.5, coordy + 0.5, coords.zoom);

  for (int_t i = 0; i <= GRID_RESOLUTION; ++i)
  {
    for (int_t j = 0; j <= GRID_RESOLUTION; ++j)
    {
      const real_t u = static_cast<real_t>(j) / GRID_RESOLUTION;
      const real_t v = static_cast<real_t>(i) / GRID_RESOLUTION;

      const real_t tile_x = coordx + u;
      const real_t tile_y = coordy + v;

      // Calculate the ECEF coordinates in double precision to maintain accuracy,
      // then convert to float for the vertex position
      glm::dvec3 posDouble = tileToECEF(tile_x, tile_y, coords.zoom);
      glm::dvec3 localPosDouble = posDouble - tileCenter;
      glm::vec3 position = glm::vec3(localPosDouble);

      glm::vec3 normal = glm::normalize(glm::dvec3(
        posDouble.x / SEMI_MAJOR_SQUARED,
        posDouble.y / SEMI_MAJOR_SQUARED,
        posDouble.z / SEMI_MINOR_SQUARED));

      Vertex vertex {
        .position = position,
        .normal = normal,
        .color = glm::vec3{1.0f},
        .uv = glm::vec2{u, v},
      };
      mesh.vertices.push_back(vertex);
    }
  }

  for (int i = 0; i < GRID_RESOLUTION; ++i)
  {
    for (int j = 0; j < GRID_RESOLUTION; ++j)
    {
      int row1 = i * (GRID_RESOLUTION + 1);
      int row2 = (i + 1) * (GRID_RESOLUTION + 1);

      mesh.indices.push_back(row1 + j);
      mesh.indices.push_back(row2 + j);
      mesh.indices.push_back(row1 + j + 1);

      mesh.indices.push_back(row1 + j + 1);
      mesh.indices.push_back(row2 + j);
      mesh.indices.push_back(row2 + j + 1);
    }
  }
  return {mesh, tileCenter};
}
}
}
