#include "flight_boundary.hpp"

#include "components.hpp"
#include "indicator_model_loader.hpp"

#include <SDL3/SDL.h>
#include <glm/geometric.hpp>

#include <fstream>
#include <sstream>
#include <string>

namespace flb
{
namespace
{
constexpr float WallHeightMeters = 200.0f;
constexpr float WallThicknessMeters = 20.0f;
const glm::vec4 BoundaryWallColor{1.0f, 0.8f, 0.0f, 0.35f};

GeoCoords midpoint(const GeoCoords& a, const GeoCoords& b)
{
  return {
    .latitude = (a.latitude + b.latitude) * 0.5,
    .longitude = (a.longitude + b.longitude) * 0.5,
  };
}

bool createWallPlacement(const GeoCoords& start, const GeoCoords& end, glm::dvec3& outPosition, glm::mat3& outTransform)
{
  const glm::dvec3 startEcef = geoToECEF(start);
  const glm::dvec3 endEcef = geoToECEF(end);
  const GeoCoords centerGeo = midpoint(start, end);
  const glm::dvec3 up = glm::normalize(glm::dvec3{getSurfaceNormal(centerGeo)});

  const glm::dvec3 edge = endEcef - startEcef;
  const glm::dvec3 projectedEdge = edge - up * glm::dot(edge, up);
  const double edgeLength = glm::length(projectedEdge);
  if (edgeLength <= 0.001)
  {
    return false;
  }

  const glm::dvec3 edgeDir = projectedEdge / edgeLength;
  const glm::dvec3 insideDir = glm::normalize(glm::cross(edgeDir, up));
  const glm::dvec3 center = (startEcef + endEcef) * 0.5;

  outPosition = center + insideDir * (static_cast<double>(WallThicknessMeters) * 0.5) +
                up * (static_cast<double>(WallHeightMeters) * 0.5);
  outTransform = glm::mat3{
    glm::vec3(edgeDir * edgeLength),
    glm::vec3(insideDir * static_cast<double>(WallThicknessMeters)),
    glm::vec3(up * static_cast<double>(WallHeightMeters)),
  };
  return true;
}

} // namespace

SDL_AppResult FlightBoundary::init(
  entt::registry& registry,
  MeshManager& meshManager,
  const std::filesystem::path& boundaryPath,
  const std::filesystem::path& cubePath)
{
  clear(registry);

  const std::vector<GeoCoords> points = parseBoundaryFile(boundaryPath);
  if (points.size() < 3)
  {
    SDL_Log("Flight boundary needs at least 3 points");
    return SDL_APP_FAILURE;
  }

  const IndicatorModel wallModel = loadIndicatorModel(meshManager, cubePath, BoundaryWallColor, "flight boundary wall");
  if (!wallModel.isValid())
  {
    return SDL_APP_FAILURE;
  }

  wallEntities.reserve(points.size());
  for (std::size_t i = 0; i < points.size(); ++i)
  {
    const GeoCoords& start = points[i];
    const GeoCoords& end = points[(i + 1) % points.size()];

    glm::dvec3 position{};
    glm::mat3 transform{1.0f};
    if (!createWallPlacement(start, end, position, transform))
    {
      SDL_Log("Skipping zero-length flight boundary edge %zu", i);
      continue;
    }

    const entt::entity wall = registry.create();
    registry.emplace<component::Position>(wall, position);
    registry.emplace<component::Transform>(wall, transform);
    registry.emplace<component::IndicatorModel>(wall, wallModel);
    wallEntities.push_back(wall);
  }

  return wallEntities.empty() ? SDL_APP_FAILURE : SDL_APP_CONTINUE;
}

void FlightBoundary::clear(entt::registry& registry)
{
  for (const entt::entity wall : wallEntities)
  {
    if (registry.valid(wall))
    {
      registry.destroy(wall);
    }
  }
  wallEntities.clear();
}

std::vector<GeoCoords> FlightBoundary::parseBoundaryFile(const std::filesystem::path& boundaryPath)
{
  std::ifstream file(boundaryPath);
  if (!file.is_open())
  {
    SDL_Log("Failed to open flight boundary file: %s", boundaryPath.string().c_str());
    return {};
  }

  std::vector<GeoCoords> points;
  std::string line;
  std::size_t lineNumber = 0;
  while (std::getline(file, line))
  {
    ++lineNumber;
    if (line.find_first_not_of(" \t\r\n") == std::string::npos)
    {
      continue;
    }

    std::istringstream lineStream(line);
    GeoCoords point{};
    if (!(lineStream >> point.latitude >> point.longitude))
    {
      SDL_Log("Invalid flight boundary coordinate at line %zu", lineNumber);
      return {};
    }

    points.push_back(point);
  }

  return points;
}

} // namespace flb
