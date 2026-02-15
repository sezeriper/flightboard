#pragma once

#include "camera.hpp"
#include "window.hpp"
#include "device.hpp"
#include "time.hpp"

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <SDL3/SDL.h>


namespace flb
{

  class App {
  public:
    SDL_AppResult init();
    void cleanup();
    SDL_AppResult handleEvent(SDL_Event* event);
    SDL_AppResult update(float dt);
    SDL_AppResult draw() const;

    Time::TimePoint lastFrame{};

  private:

    static constexpr float mouseSensitivity = 0.006f;
    static constexpr float keyboardSensitivity = 4.0f;
    static constexpr float scrollSensitivity = 0.6f;
    OrbitalCamera camera;
    entt::registry registry;

    SDL_AppResult createPipeline();
    SDL_AppResult createDepthTexture(Uint32 width, Uint32 height);

    SDL_AppResult createModel(const Mesh& mesh, const Texture& texture, const Transform& transform);

    Device device;
    Window window;

    SDL_GPUGraphicsPipeline* pipeline = NULL;
    SDL_GPUTexture* depthTexture = NULL;
    SDL_GPUSampler* sampler = NULL;
  };
} // namespace flb
