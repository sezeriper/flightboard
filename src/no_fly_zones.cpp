#include "no_fly_zones.hpp"

#include "components.hpp"
#include "indicator_model_loader.hpp"

#include <SDL3/SDL.h>

#include <fstream>
#include <sstream>
#include <string>

namespace flb
{
namespace
{
constexpr float ZoneHeightMeters = 800.0f;
const glm::vec4 ZoneColor{1.0f, 0.0f, 0.0f, 0.25f};

} // namespace

SDL_AppResult NoFlyZones::init(
  entt::registry& registry,
  MeshManager& meshManager,
  const std::filesystem::path& zonesPath,
  const std::filesystem::path& cylinderPath)
{
  clear(registry);

  std::vector<ZoneDefinition> zones;
  if (!parseZonesFile(zonesPath, zones))
  {
    return SDL_APP_FAILURE;
  }

  if (zones.empty())
  {
    SDL_Log("No no-fly zones loaded from %s", zonesPath.string().c_str());
    return SDL_APP_CONTINUE;
  }

  const IndicatorModel zoneModel = loadIndicatorModel(meshManager, cylinderPath, ZoneColor, "no-fly zone");
  if (!zoneModel.isValid())
  {
    return SDL_APP_FAILURE;
  }

  zoneEntities.reserve(zones.size());
  for (const ZoneDefinition& zone : zones)
  {
    const entt::entity zoneEntity = registry.create();
    registry.emplace<component::Position>(zoneEntity, geoToECEF(zone.center));
    registry.emplace<component::Transform>(zoneEntity, NoFlyZones::makeZoneTransform(zone));
    registry.emplace<component::IndicatorModel>(zoneEntity, zoneModel);
    zoneEntities.push_back(zoneEntity);
  }

  return SDL_APP_CONTINUE;
}

void NoFlyZones::clear(entt::registry& registry)
{
  for (const entt::entity zone : zoneEntities)
  {
    if (registry.valid(zone))
    {
      registry.destroy(zone);
    }
  }
  zoneEntities.clear();
}

glm::mat3 NoFlyZones::makeZoneTransform(const ZoneDefinition& zone)
{
  glm::mat3 scale{1.0f};
  scale[0][0] = zone.radiusMeters;
  scale[1][1] = zone.radiusMeters;
  scale[2][2] = ZoneHeightMeters;

  return getSurfaceAlignedTransform(zone.center) * scale;
}

bool NoFlyZones::parseZonesFile(const std::filesystem::path& zonesPath, std::vector<ZoneDefinition>& outZones)
{
  std::ifstream file(zonesPath);
  if (!file.is_open())
  {
    SDL_Log("Failed to open no-fly-zones file: %s", zonesPath.string().c_str());
    return false;
  }

  std::string line;
  std::size_t lineNumber = 0;
  while (std::getline(file, line))
  {
    ++lineNumber;
    if (line.find_first_not_of(" \t\r\n") == std::string::npos)
    {
      continue;
    }

    ZoneDefinition zone{};
    std::istringstream lineStream(line);
    if (!(lineStream >> zone.center.latitude >> zone.center.longitude >> zone.radiusMeters))
    {
      SDL_Log("Invalid no-fly-zone entry at line %zu", lineNumber);
      return false;
    }
    if (zone.radiusMeters <= 0.0f)
    {
      SDL_Log("Invalid no-fly-zone radius at line %zu", lineNumber);
      return false;
    }

    outZones.push_back(zone);
  }

  return true;
}

} // namespace flb
