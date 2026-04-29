#pragma once

#include "SDL3/SDL_init.h"
#include "camera.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/sampler.hpp"
#include "window.hpp"

#include <SDL3/SDL.h>
#include <entt/entt.hpp>

namespace flb
{
class ImGuiLayer;
struct ViewportRect;

namespace gpu
{
class Allocator;
};

class Renderer
{
public:
  SDL_AppResult init(SDL_Window* window);
  void cleanup(SDL_Window* window);
  SDL_AppResult initDebugSphere(gpu::Allocator& allocator);
  void initTileIndexBuffer(gpu::Allocator& allocator);
  SDL_AppResult ensureSceneTarget(const ViewportRect& rect);
  SDL_GPUTexture* getSceneTexture() const { return sceneTarget.colorTexture; }

  SDL_AppResult draw(
    entt::registry& registry, const Camera& camera, const Window& window, const ImGuiLayer* imGuiLayer = nullptr) const;

  gpu::Device& getDevice() { return device; }
  const gpu::Device& getDevice() const { return device; }

private:
  void releaseSceneTarget();

  gpu::Device device;
  gpu::Pipeline mainPipeline;
  gpu::Pipeline debugPipeline;
  gpu::Pipeline indicatorDepthPipeline;
  gpu::Pipeline indicatorPipeline;
  gpu::Sampler sampler;
  SDL_GPUTextureFormat sceneColorFormat = SDL_GPU_TEXTUREFORMAT_INVALID;

  SDL_GPUBuffer* debugSphereVertexBuffer = nullptr;
  SDL_GPUBuffer* debugSphereIndexBuffer = nullptr;
  Uint32 debugSphereIndexCount = 0;
  SDL_GPUBuffer* tileIndexBuffer = nullptr;

  struct SceneTarget
  {
    SDL_GPUTexture* colorTexture = nullptr;
    SDL_GPUTexture* depthTexture = nullptr;
    Uint32 width = 0;
    Uint32 height = 0;
  };
  SceneTarget sceneTarget;
};

} // namespace flb
