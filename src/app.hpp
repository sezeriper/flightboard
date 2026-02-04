#pragma once

#include "camera.hpp"

#include <glm/glm.hpp>
#include <SDL3/SDL.h>

#include <array>
#include <chrono>

namespace flb
{
namespace app
{
  struct Uniforms {
    glm::mat4 modelViewProjection{1.0f};
  };

  struct State {
    SDL_Window* window = NULL;
    SDL_GPUDevice* device = NULL;
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    SDL_GPUBuffer* vertexBuffer = NULL;
    SDL_GPUBuffer* indexBuffer = NULL;
    SDL_GPUTexture* depthTexture = NULL;

    Uniforms uniforms;
    Camera camera;
    float cubeRotation = 0.0f;
    std::chrono::steady_clock::time_point lastFrameTime{};
  };

  struct Vertex {
    glm::vec4 position;
    glm::vec4 color;
  };

  // Cube vertices
  static const std::array VERTICES {
    Vertex{{-0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}}, // 0
    Vertex{{ 0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}}, // 1
    Vertex{{ 0.5f,  0.5f, -0.5f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}, // 2
    Vertex{{-0.5f,  0.5f, -0.5f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}}, // 3
    Vertex{{-0.5f, -0.5f,  0.5f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}}, // 4
    Vertex{{ 0.5f, -0.5f,  0.5f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}}, // 5
    Vertex{{ 0.5f,  0.5f,  0.5f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 6
    Vertex{{-0.5f,  0.5f,  0.5f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}  // 7
  };

  // Cube indices
  static const std::array<std::uint16_t, 36> INDICES {
    0, 3, 2, 2, 1, 0, // back face
    4, 5, 6, 6, 7, 4, // front face
    0, 4, 7, 7, 3, 0, // left face
    1, 2, 6, 6, 5, 1, // right face
    3, 7, 6, 6, 2, 3, // top face
    0, 1, 5, 5, 4, 0  // bottom face
  };

  SDL_AppResult init(State& state);
  void cleanup(const State& state);
  SDL_AppResult update(State& state, float dt);
  SDL_AppResult draw(const State& state);
  SDL_AppResult handleEvent(State& state, SDL_Event* event);
} // namespace app
} // namespace flb
