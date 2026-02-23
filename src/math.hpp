#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <numbers>

namespace flb
{
constexpr double PI = std::numbers::pi_v<double>;
constexpr double MIN_LATITUDE = -85.05112878;
constexpr double MAX_LATITUDE = 85.05112878;
constexpr double MIN_LONGITUDE = -180.0;
constexpr double MAX_LONGITUDE = 180.0;
constexpr double SEMI_MAJOR = 6378137.0;
constexpr double SEMI_MINOR = 6356752.3142451793;
constexpr double SEMI_MAJOR_SQUARED = SEMI_MAJOR * SEMI_MAJOR;
constexpr double SEMI_MINOR_SQUARED = SEMI_MINOR * SEMI_MINOR;

struct TileCoords
{
  std::uint32_t zoom;
  std::uint32_t x;
  std::uint32_t y;
};

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
  double latRad = glm::radians(geo.latitude);
  double lonRad = glm::radians(geo.longitude);

  double sinLat = glm::sin(latRad);
  double cosLat = glm::cos(latRad);
  double sinLon = glm::sin(lonRad);
  double cosLon = glm::cos(lonRad);

  double n = SEMI_MAJOR_SQUARED * cosLat * cosLat + SEMI_MINOR_SQUARED * sinLat * sinLat;
  double x = (SEMI_MAJOR_SQUARED * cosLat * cosLon) / n;
  double y = (SEMI_MAJOR_SQUARED * cosLat * sinLon) / n;
  double z = (SEMI_MINOR_SQUARED * sinLat) / n;

  return glm::normalize(glm::dvec3(x, y, z));
}

static TileCoords geoToTileCoords(const GeoCoords& from, const std::uint32_t& zoom)
{
  double latitude = glm::clamp(from.latitude, MIN_LATITUDE, MAX_LATITUDE);
  double longitude = glm::clamp(from.longitude, MIN_LONGITUDE, MAX_LONGITUDE);

  double x = (longitude + 180.0) / 360.0;
  double sinLat = glm::sin(latitude * PI / 180.0);
  double y = 0.5 - glm::log((1.0 + sinLat) / (1.0 - sinLat)) / (4.0 * PI);

  double numTiles = glm::pow(2.0, zoom);
  std::uint32_t tileX = static_cast<std::uint32_t>(glm::floor(x * numTiles));
  std::uint32_t tileY = static_cast<std::uint32_t>(glm::floor(y * numTiles));
  return {zoom, tileX, tileY};
}

/**
 * Converts geographic coordinates (latitude and longitude) to ECEF (Earth-Centered, Earth-Fixed) coordinates.
 */
static ECEFCoords geoToECEF(const GeoCoords& geo)
{
  double latRad = glm::radians(geo.latitude);
  double lonRad = glm::radians(geo.longitude);

  constexpr double a = SEMI_MAJOR;
  constexpr double b = SEMI_MINOR;
  constexpr double e2 = 1.0 - (b * b) / (a * a);

  double sinLat = glm::sin(latRad);
  double cosLat = glm::cos(latRad);
  double N = a / glm::sqrt(1.0 - e2 * sinLat * sinLat);

  double x = N * cosLat * glm::cos(lonRad);
  double y = N * cosLat * glm::sin(lonRad);
  double z = N * (1.0 - e2) * sinLat;

  return {x, y, z};
}

/**
 * Converts geographic coordinates (latitude and longitude) and height above ellipsoid to
 * ECEF (Earth-Centered, Earth-Fixed) coordinates.
 */
static ECEFCoords geoToECEF(const GeoCoords& geo, double height)
{
  double latRad = glm::radians(geo.latitude);
  double lonRad = glm::radians(geo.longitude);

  constexpr double a = SEMI_MAJOR;
  constexpr double b = SEMI_MINOR;
  constexpr double e2 = 1.0 - (b * b) / (a * a);

  double sinLat = glm::sin(latRad);
  double cosLat = glm::cos(latRad);
  double N = a / glm::sqrt(1.0 - e2 * sinLat * sinLat);

  double x = (N + height) * cosLat * glm::cos(lonRad);
  double y = (N + height) * cosLat * glm::sin(lonRad);
  double z = (N * (1.0 - e2) + height) * sinLat;

  return {x, y, z};
}

/**
 * Converts fractional tile coordinates to geographic coordinates (latitude and longitude) in radians.
 */
static ECEFCoords tileToECEF(double tile_x, double tile_y, std::uint32_t tile_zoom)
{
  double n = glm::pow(2.0, tile_zoom);
  double lon = (tile_x / n) * 2.0 * PI - PI;
  double lat = glm::atan(glm::sinh(PI * (1.0 - 2.0 * tile_y / n)));

  constexpr double a = SEMI_MAJOR;
  constexpr double b = SEMI_MINOR;
  constexpr double e2 = 1.0 - (b * b) / (a * a);

  double sin_lat = glm::sin(lat);
  double cos_lat = glm::cos(lat);
  double N = a / glm::sqrt(1.0 - e2 * sin_lat * sin_lat);

  double x = N * cos_lat * glm::cos(lon);
  double y = N * cos_lat * glm::sin(lon);
  double z = N * (1.0 - e2) * sin_lat;

  return {x, y, z};
}
} // namespace flb
