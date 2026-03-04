#pragma once

#include "quadtree.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <numbers>

namespace flb
{
constexpr double SQRT_2 = 1.4142135623730951;
constexpr double PI = std::numbers::pi_v<double>;
constexpr double MIN_LATITUDE = -85.05112878;
constexpr double MAX_LATITUDE = 85.05112878;
constexpr double MIN_LONGITUDE = -180.0;
constexpr double MAX_LONGITUDE = 180.0;
constexpr double SEMI_MAJOR = 6378137.0;
constexpr double SEMI_MINOR = 6356752.3142451793;
constexpr double SEMI_MAJOR_SQUARED = SEMI_MAJOR * SEMI_MAJOR;
constexpr double SEMI_MINOR_SQUARED = SEMI_MINOR * SEMI_MINOR;

struct GeoCoords
{
  double latitude;
  double longitude;
};

struct BBOX
{
  GeoCoords min;
  GeoCoords max;
};

/**
 * The loading algorithm will load all tiles that intersect with the zoom region for each zoom level in the range
 * [zoomMin, zoomMax].
 */
struct TilesetDescription
{
  std::uint32_t tileImageSize;
  std::uint32_t zoomMin;
  std::uint32_t zoomMax;
  BBOX zoomRegion;
};

using ECEFCoords = glm::dvec3;

/**
 * Returns the surface normal vector for the given geographic coordinates (latitude and longitude) on the WGS84
 * ellipsoid.
 */
static glm::vec3 getSurfaceNormal(const GeoCoords& geo)
{
  const double latRad = glm::radians(geo.latitude);
  const double lonRad = glm::radians(geo.longitude);

  const double sinLat = glm::sin(latRad);
  const double cosLat = glm::cos(latRad);
  const double sinLon = glm::sin(lonRad);
  const double cosLon = glm::cos(lonRad);

  const double n = SEMI_MAJOR_SQUARED * cosLat * cosLat + SEMI_MINOR_SQUARED * sinLat * sinLat;
  const double x = (SEMI_MAJOR_SQUARED * cosLat * cosLon) / n;
  const double y = (SEMI_MAJOR_SQUARED * cosLat * sinLon) / n;
  const double z = (SEMI_MINOR_SQUARED * sinLat) / n;

  return glm::normalize(glm::dvec3(x, y, z));
}

static NodeCoords geoToTileCoords(const GeoCoords& from, const std::uint32_t& zoom)
{
  const double latitude = glm::clamp(from.latitude, MIN_LATITUDE, MAX_LATITUDE);
  const double longitude = glm::clamp(from.longitude, MIN_LONGITUDE, MAX_LONGITUDE);

  const double x = (longitude + 180.0) / 360.0;
  const double sinLat = glm::sin(latitude * PI / 180.0);
  const double y = 0.5 - glm::log((1.0 + sinLat) / (1.0 - sinLat)) / (4.0 * PI);

  const double numTiles = glm::pow(2.0, zoom);
  const std::uint32_t tileX = static_cast<std::uint32_t>(glm::floor(x * numTiles));
  const std::uint32_t tileY = static_cast<std::uint32_t>(glm::floor(y * numTiles));
  return {zoom, tileX, tileY};
}

/**
 * Converts geographic coordinates (latitude and longitude) to ECEF (Earth-Centered, Earth-Fixed) coordinates.
 */
static ECEFCoords geoToECEF(const GeoCoords& geo)
{
  const double latRad = glm::radians(geo.latitude);
  const double lonRad = glm::radians(geo.longitude);

  constexpr double a = SEMI_MAJOR;
  constexpr double b = SEMI_MINOR;
  constexpr double e2 = 1.0 - (b * b) / (a * a);

  const double sinLat = glm::sin(latRad);
  const double cosLat = glm::cos(latRad);
  const double N = a / glm::sqrt(1.0 - e2 * sinLat * sinLat);

  const double x = N * cosLat * glm::cos(lonRad);
  const double y = N * cosLat * glm::sin(lonRad);
  const double z = N * (1.0 - e2) * sinLat;

  return {x, y, z};
}

/**
 * Converts geographic coordinates (latitude and longitude) and height above ellipsoid to
 * ECEF (Earth-Centered, Earth-Fixed) coordinates.
 */
static ECEFCoords geoToECEF(const GeoCoords& geo, double height)
{
  const double latRad = glm::radians(geo.latitude);
  const double lonRad = glm::radians(geo.longitude);

  constexpr double a = SEMI_MAJOR;
  constexpr double b = SEMI_MINOR;
  constexpr double e2 = 1.0 - (b * b) / (a * a);

  const double sinLat = glm::sin(latRad);
  const double cosLat = glm::cos(latRad);
  const double N = a / glm::sqrt(1.0 - e2 * sinLat * sinLat);

  const double x = (N + height) * cosLat * glm::cos(lonRad);
  const double y = (N + height) * cosLat * glm::sin(lonRad);
  const double z = (N * (1.0 - e2) + height) * sinLat;

  return {x, y, z};
}

/**
 * Converts fractional tile coordinates to geographic coordinates (latitude and longitude) in radians.
 */
static ECEFCoords tileToECEF(std::uint32_t tile_zoom, double tile_x, double tile_y)
{
  const double n = glm::pow(2.0, tile_zoom);
  const double lon = (tile_x / n) * 2.0 * PI - PI;
  const double lat = glm::atan(glm::sinh(PI * (1.0 - 2.0 * tile_y / n)));

  constexpr double a = SEMI_MAJOR;
  constexpr double b = SEMI_MINOR;
  constexpr double e2 = 1.0 - (b * b) / (a * a);

  const double sin_lat = glm::sin(lat);
  const double cos_lat = glm::cos(lat);
  const double N = a / glm::sqrt(1.0 - e2 * sin_lat * sin_lat);

  const double x = N * cos_lat * glm::cos(lon);
  const double y = N * cos_lat * glm::sin(lon);
  const double z = N * (1.0 - e2) * sin_lat;

  return {x, y, z};
}
} // namespace flb
