#pragma once

#include "indicator_model.hpp"
#include "obj_loader.hpp"

#include <SDL3/SDL.h>

#include <filesystem>

namespace flb
{

inline IndicatorModel loadIndicatorModel(
  MeshManager& meshManager, const std::filesystem::path& objPath, glm::vec4 color, const char* modelName)
{
  const auto [vertexData, indexData] = loadOBJ(objPath);
  const MeshHandle meshHandle = meshManager.allocate(vertexData, indexData);
  const Mesh mesh = meshManager.get(meshHandle);
  if (!meshHandle.isValid() || mesh.vertexBuffer.buffer == nullptr || mesh.indexBuffer.buffer == nullptr)
  {
    SDL_Log("Failed to create %s indicator mesh", modelName);
    meshManager.release(meshHandle);
    return {};
  }

  return {&meshManager, meshHandle, color};
}

} // namespace flb
