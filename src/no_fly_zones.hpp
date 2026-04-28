#pragma once

#include "math.hpp"
#include "mesh_manager.hpp"

#include <SDL3/SDL_init.h>
#include <entt/entt.hpp>

#include <filesystem>
#include <vector>

namespace flb
{

class NoFlyZones
{
public:
  SDL_AppResult init(
    entt::registry& registry,
    MeshManager& meshManager,
    const std::filesystem::path& zonesPath = "content/no_fly_zones.txt",
    const std::filesystem::path& cylinderPath = "content/models/cylinder/cylinder.obj");

  void clear(entt::registry& registry);

private:
  struct ZoneDefinition
  {
    GeoCoords center;
    float radiusMeters = 0.0f;
  };

  static bool parseZonesFile(const std::filesystem::path& zonesPath, std::vector<ZoneDefinition>& outZones);
  static glm::mat3 makeZoneTransform(const ZoneDefinition& zone);

  std::vector<entt::entity> zoneEntities;
};

} // namespace flb
