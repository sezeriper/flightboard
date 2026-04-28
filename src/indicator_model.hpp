#pragma once

#include "mesh_manager.hpp"

#include <glm/glm.hpp>

namespace flb
{

class IndicatorModel
{
public:
  IndicatorModel() = default;

  IndicatorModel(MeshManager* meshManager_, MeshHandle meshHandle_, glm::vec4 color_)
      : meshManager(meshManager_), meshHandle(meshHandle_), color(color_)
  {
  }

  IndicatorModel(const IndicatorModel& other)
      : meshManager(other.meshManager), meshHandle(other.meshHandle), color(other.color)
  {
    addRef();
  }

  IndicatorModel& operator=(const IndicatorModel& other)
  {
    if (this == &other)
      return *this;

    releaseRef();

    meshManager = other.meshManager;
    meshHandle = other.meshHandle;
    color = other.color;

    addRef();

    return *this;
  }

  IndicatorModel(IndicatorModel&& other) noexcept
      : meshManager(other.meshManager), meshHandle(other.meshHandle), color(other.color)
  {
    other.meshManager = nullptr;
    other.meshHandle = {};
    other.color = glm::vec4{1.0f};
  }

  IndicatorModel& operator=(IndicatorModel&& other) noexcept
  {
    if (this == &other)
      return *this;

    releaseRef();

    meshManager = other.meshManager;
    meshHandle = other.meshHandle;
    color = other.color;

    other.meshManager = nullptr;
    other.meshHandle = {};
    other.color = glm::vec4{1.0f};

    return *this;
  }

  ~IndicatorModel() { releaseRef(); }

  bool isValid() const noexcept { return meshHandle.isValid(); }

  MeshHandle getMeshHandle() const noexcept { return meshHandle; }
  glm::vec4 getColor() const noexcept { return color; }
  void setColor(glm::vec4 value) noexcept { color = value; }

  Mesh getMesh() const
  {
    if (meshManager == nullptr)
      return {};

    return meshManager->get(meshHandle);
  }

private:
  MeshManager* meshManager = nullptr;
  MeshHandle meshHandle{};
  glm::vec4 color{1.0f};

  void addRef()
  {
    if (meshManager != nullptr)
      meshManager->addRef(meshHandle);
  }

  void releaseRef()
  {
    if (meshManager != nullptr)
      meshManager->release(meshHandle);
  }
};

} // namespace flb
