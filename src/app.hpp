#pragma once

#include "camera.hpp"
#include "flight_boundary.hpp"
#include "gpu/allocator.hpp"
#include "gpu/renderer.hpp"
#include "imgui_layer.hpp"
#include "mesh_manager.hpp"
#include "model.hpp"
#include "no_fly_zones.hpp"
#include "ros.hpp"
#include "texture_manager.hpp"
#include "tile_manager.hpp"
#include "time.hpp"
#include "window.hpp"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace flb
{
class App
{
public:
  SDL_AppResult init();
  void cleanup();
  SDL_AppResult handleEvent(SDL_Event* event);
  SDL_AppResult update(float dt);
  SDL_AppResult draw();

  TimePoint lastFrame{};

private:
  Camera& activeCamera();
  const Camera& activeCamera() const;
  bool isPerspectiveCameraActive() const;
  void setCameraAspect(float aspect);
  void toggleCameraMode();

  Window window;
  Renderer renderer;
  gpu::Allocator allocator;
  ImGuiLayer imGuiLayer;

  float mouseSensitivity = 0.006f;
  float keyboardSensitivity = 4.0f;
  float scrollSensitivity = 0.6f;
  CameraMode activeCameraMode = CameraMode::Perspective;
  PerspectiveCamera perspectiveCamera;
  OrthographicCamera orthographicCamera;
  bool cameraMouseLook = false;
  entt::registry registry;

  ROS ros;

  TextureManager textureManager;
  MeshManager meshManager;
  TileManager tileManager;
  FlightBoundary flightBoundary;
  NoFlyZones noFlyZones;
};
} // namespace flb
