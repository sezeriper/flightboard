#pragma once

#include "mesh_manager.hpp"
#include "texture_manager.hpp"

namespace flb
{

class Model
{
public:
  Model() = default;

  Model(
    MeshManager* meshManager_, TextureManager* textureManager_, MeshHandle meshHandle_, TextureHandle textureHandle_)
      : meshManager(meshManager_), textureManager(textureManager_), meshHandle(meshHandle_),
        textureHandle(textureHandle_)
  {
  }

  Model(const Model& other)
      : meshManager(other.meshManager), textureManager(other.textureManager), meshHandle(other.meshHandle),
        textureHandle(other.textureHandle)
  {
    addRefs();
  }

  Model& operator=(const Model& other)
  {
    if (this == &other)
      return *this;

    releaseRefs();

    meshManager = other.meshManager;
    textureManager = other.textureManager;
    meshHandle = other.meshHandle;
    textureHandle = other.textureHandle;

    addRefs();

    return *this;
  }

  Model(Model&& other) noexcept
      : meshManager(other.meshManager), textureManager(other.textureManager), meshHandle(other.meshHandle),
        textureHandle(other.textureHandle)
  {
    other.meshManager = nullptr;
    other.textureManager = nullptr;
    other.meshHandle = {};
    other.textureHandle = {};
  }

  Model& operator=(Model&& other) noexcept
  {
    if (this == &other)
      return *this;

    releaseRefs();

    meshManager = other.meshManager;
    textureManager = other.textureManager;
    meshHandle = other.meshHandle;
    textureHandle = other.textureHandle;

    other.meshManager = nullptr;
    other.textureManager = nullptr;
    other.meshHandle = {};
    other.textureHandle = {};

    return *this;
  }

  ~Model() { releaseRefs(); }

  bool isValid() const noexcept { return meshHandle.isValid() && textureHandle.isValid(); }

  MeshHandle getMeshHandle() const noexcept { return meshHandle; }
  TextureHandle getTextureHandle() const noexcept { return textureHandle; }

  Mesh getMesh() const
  {
    if (meshManager == nullptr)
      return {};

    return meshManager->get(meshHandle);
  }

  gpu::TextureHandle getTexture() const
  {
    if (textureManager == nullptr)
      return {};

    return textureManager->get(textureHandle);
  }

private:
  MeshManager* meshManager = nullptr;
  TextureManager* textureManager = nullptr;
  MeshHandle meshHandle{};
  TextureHandle textureHandle{};

  void addRefs()
  {
    if (meshManager != nullptr)
      meshManager->addRef(meshHandle);
    if (textureManager != nullptr)
      textureManager->addRef(textureHandle);
  }

  void releaseRefs()
  {
    if (meshManager != nullptr)
      meshManager->release(meshHandle);
    if (textureManager != nullptr)
      textureManager->release(textureHandle);
  }
};

} // namespace flb
