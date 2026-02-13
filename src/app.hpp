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
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uv;
  };

  using Index = Uint16;

  template<typename T>
  concept VertexOrIndex = std::is_same_v<T, Vertex> || std::is_same_v<T, Index>;

  struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<Index> indices;
  };

  /**
   * The image loaded using SDL_LoadSurface. The data is owned by the caller
   * and must remain valid until the texture is uploaded to the GPU.
   */
  struct ImageData {
    std::size_t width;
    std::size_t height;
    std::vector<std::byte> data;
  };

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
      glm::mat4 viewProjection;
      glm::mat4 model;
    };

    struct ModelBuffers {
      SDL_GPUBuffer* vertex;
      SDL_GPUBuffer* index;
      Uint32 numOfIndices;
    };
    using ModelTexture = SDL_GPUTexture*;
    using Transform = glm::mat4;

    static constexpr float mouseSensitivity = 0.006f;
    static constexpr float keyboardSensitivity = 4.0f;
    static constexpr float scrollSensitivity = 0.6f;
    OrbitalCamera camera;
    entt::registry registry;

    SDL_AppResult createPipeline();
    SDL_AppResult createDepthTexture(Uint32 width, Uint32 height);

    template<typename R>
    requires std::ranges::contiguous_range<R> &&
             std::ranges::sized_range<R> &&
             VertexOrIndex<std::ranges::range_value_t<R>>
    SDL_AppResult uploadDataToGPUBuffer(const R& data, SDL_GPUBuffer** outBuffer) const;
    SDL_AppResult uploadMeshToGPUBuffers(const MeshData& meshData, ModelBuffers& outBuffers) const;
    SDL_AppResult uploadImageToGPUTexture(const ImageData& imageData, ModelTexture& outTexture) const;
    SDL_AppResult createModel(const MeshData& meshData, const ImageData& imageData, const Transform& transform);

    SDL_Window* window = NULL;
    SDL_GPUDevice* device = NULL;
    SDL_GPUGraphicsPipeline* pipeline = NULL;
    SDL_GPUTexture* depthTexture = NULL;
    SDL_GPUSampler* sampler = NULL;
  };
} // namespace flb
