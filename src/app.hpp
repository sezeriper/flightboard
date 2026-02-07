#pragma once

#include "camera.hpp"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <SDL3/SDL.h>

#include <chrono>
#include <ranges>
#include <type_traits>

namespace flb
{
  struct Vertex {
    glm::vec4 position;
    glm::vec4 color;
  };

  using Index = Uint16;

  template<typename T>
  concept VertexOrIndex = std::is_same_v<T, Vertex> || std::is_same_v<T, Index>;

  class App {
  public:
    App() = default;
    ~App() = default;

    SDL_AppResult init();
    void cleanup();
    SDL_AppResult handleEvent(SDL_Event* event);
    SDL_AppResult update(float dt);
    SDL_AppResult draw() const;

    std::chrono::steady_clock::time_point lastFrameTime{};

  private:
    struct Uniforms {
      glm::mat4 modelViewProjection{1.0f};
    };

    struct MeshData {
      const std::vector<Vertex> vertices;
      const std::vector<Index> indices;
    };

    struct MeshGPUBuffers {
      SDL_GPUBuffer* vertexBuffer;
      SDL_GPUBuffer* indexBuffer;
      Uint32 numOfIndices;
    };

    using Transform = glm::mat4;

    Camera camera;
    entt::registry registry;

    SDL_AppResult createPipeline();
    SDL_AppResult createDepthTexture(Uint32 width, Uint32 height);

    template<typename R>
    requires std::ranges::contiguous_range<R> &&
             std::ranges::sized_range<R> &&
             VertexOrIndex<std::ranges::range_value_t<R>>
    SDL_AppResult uploadDataToGPUBuffer(const R& data, SDL_GPUBuffer** outBuffer) const;
    SDL_AppResult uploadMeshToGPUBuffers(const MeshData& meshData, MeshGPUBuffers& outBuffers) const;
    SDL_AppResult createModel(const MeshData& meshData, const Transform& transform);

    SDL_Window* window = NULL;
    SDL_GPUDevice* device = NULL;
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    SDL_GPUTexture* depthTexture = NULL;
  };
} // namespace flb
