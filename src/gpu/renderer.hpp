#pragma once

#include "window.hpp"
#include "camera.hpp"
#include "gpu/device.hpp"
#include "gpu/pipeline.hpp"
#include "gpu/sampler.hpp"

#include <entt/entt.hpp>
#include <SDL3/SDL.h>

namespace flb {

class Renderer {
public:
    SDL_AppResult init(SDL_Window* window);
    void cleanup(SDL_Window* window);

    SDL_AppResult draw(entt::registry& registry, const FPSCamera& camera, const Window& window) const;

    gpu::Device& getDevice() { return device; }
    const gpu::Device& getDevice() const { return device; }

private:
   gpu::Device device;
   gpu::Pipeline pipeline;
   gpu::Sampler sampler;
};

} // namespace flb
