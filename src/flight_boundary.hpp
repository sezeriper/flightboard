#pragma once

#include "math.hpp"
#include "mesh_manager.hpp"

#include <SDL3/SDL_init.h>
#include <entt/entt.hpp>

#include <filesystem>
#include <vector>

namespace flb
{

class FlightBoundary
{
public:
  SDL_AppResult init(
    entt::registry& registry,
    MeshManager& meshManager,
    const std::filesystem::path& boundaryPath = "content/flight_boundary.txt",
    const std::filesystem::path& cubePath = "content/models/cube/cube.obj");

  void clear(entt::registry& registry);

private:
  static std::vector<GeoCoords> parseBoundaryFile(const std::filesystem::path& boundaryPath);

  std::vector<entt::entity> wallEntities;
};

} // namespace flb
