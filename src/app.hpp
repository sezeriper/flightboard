#pragma once

#include <glm/glm.hpp>
#include <SDL3/SDL.h>

#include <array>
#include <chrono>

namespace flb
{
namespace app
{
  struct State {
    SDL_Window* window = NULL;
    SDL_GPUDevice* device = NULL;
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    SDL_GPUBuffer* vertexBuffer = NULL;

    // for mesuring fps
    std::chrono::steady_clock::time_point startTime{};
    std::uint32_t frameCount = 0;
  };

  struct Vertex {
    glm::vec2 position;
    glm::vec4 color;
  };

  constexpr std::array<Vertex, 3> VERTICES {
    Vertex {{  0.0f,  0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    Vertex {{ -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f, 1.0f}},
    Vertex {{  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f, 1.0f}},
  };

  SDL_AppResult init(State& state);
  void cleanup(const State& state);
  SDL_AppResult draw(const State& state);
} // namespace app
} // namespace flb
