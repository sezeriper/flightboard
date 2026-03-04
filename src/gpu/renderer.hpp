#pragma once

#include "camera.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/sampler.hpp"
#include "window.hpp"

#include <SDL3/SDL.h>
#include <entt/entt.hpp>

namespace flb
{
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

  SDL_AppResult draw(entt::registry& registry, const FPSCamera& camera, const Window& window) const;

  gpu::Device& getDevice() { return device; }
  const gpu::Device& getDevice() const { return device; }

private:
  gpu::Device device;
  gpu::Pipeline pipeline;
  gpu::Sampler sampler;

  SDL_GPUBuffer* debugSphereVertexBuffer = nullptr;
  SDL_GPUBuffer* debugSphereIndexBuffer = nullptr;
  Uint32 debugSphereIndexCount = 0;
};

} // namespace flb
