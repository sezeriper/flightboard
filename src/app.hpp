#pragma once

#include "camera.hpp"
#include "gpu/allocator.hpp"
#include "gpu/renderer.hpp"
#include "imgui_layer.hpp"
#include "ros.hpp"
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
  Window window;
  Renderer renderer;
  gpu::Allocator allocator;
  ImGuiLayer imGuiLayer;

  float mouseSensitivity = 0.006f;
  float keyboardSensitivity = 4.0f;
  float scrollSensitivity = 0.6f;
  FPSCamera camera;
  bool cameraMouseLook = false;
  entt::registry registry;

  ROS ros;

  TileManager tileManager;
};
} // namespace flb
