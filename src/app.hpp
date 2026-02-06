#pragma once

#include "camera.hpp"

#include <glm/glm.hpp>
#include <SDL3/SDL.h>

#include <chrono>
#include <span>

namespace flb
{
  class App {
  public:
    App() = default;
    ~App() = default;

    SDL_AppResult init();
    void cleanup();
    SDL_AppResult update(float dt);
    SDL_AppResult draw();
    SDL_AppResult handleEvent(SDL_Event* event);

    std::chrono::steady_clock::time_point lastFrameTime{};

  private:
    struct Uniforms {
      glm::mat4 modelViewProjection{1.0f};
    };

    struct Vertex {
      glm::vec4 position;
      glm::vec4 color;
    };

    using Index = Uint16;

    SDL_AppResult createPipeline();
    SDL_AppResult createVertexBuffer(const std::span<const Vertex> vertices);
    SDL_AppResult createIndexBuffer(const std::span<const Index> indices);
    SDL_AppResult createDepthTexture(Uint32 width, Uint32 height);

    SDL_Window* window = NULL;
    SDL_GPUDevice* device = NULL;
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    SDL_GPUBuffer* vertexBuffer = NULL;
    SDL_GPUBuffer* indexBuffer = NULL;
    SDL_GPUTexture* depthTexture = NULL;

    Uniforms uniforms;
    Camera camera;
    float cubeRotation = 0.0f;
  };
} // namespace flb
