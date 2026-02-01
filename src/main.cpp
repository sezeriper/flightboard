#include "SDL3/SDL_init.h"
#include "utils.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>


SDL_Window* window = NULL;
SDL_GPUDevice* device = NULL;
SDL_GPUGraphicsPipeline* pipeline = NULL;

SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
  SDL_InitSubSystem(SDL_INIT_VIDEO);

  device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, true, NULL);
  if (device == NULL)
  {
    SDL_Log("CreateGPUDevice failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  window = SDL_CreateWindow("flightboard v0.0.1", 1280, 720, SDL_WINDOW_RESIZABLE);
  if (window == NULL)
  {
    SDL_Log("CreateWindow failed %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (!SDL_ClaimWindowForGPUDevice(device, window))
  {
    SDL_Log("ClaimWindowForGPUDevice failed");
  }


  SDL_GPUShader* vertexShader = loadShader(device, "raw_triangle.vert", 0, 0, 0, 0);
  if (vertexShader == NULL)
  {
    SDL_Log("Failed to load vertex shader");
    return SDL_APP_FAILURE;
  }

  SDL_GPUShader* fragmentShader = loadShader(device, "raw_triangle.frag", 0, 0, 0, 0);
  if (fragmentShader == NULL)
  {
    SDL_Log("Failed to load fragment shader");
    return SDL_APP_FAILURE;
  }

  // create the pipeline
  SDL_GPUColorTargetDescription colorTargetDesc {
    .format = SDL_GetGPUSwapchainTextureFormat(device, window),
  };

  SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo {
    .vertex_shader = vertexShader,
    .fragment_shader = fragmentShader,
    .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
    .rasterizer_state {
      .fill_mode = SDL_GPU_FILLMODE_FILL,
    },
    .target_info {
      .num_color_targets = 1,
      .color_target_descriptions = &colorTargetDesc,
    }
  };

  pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineCreateInfo);
  if (pipeline == NULL)
  {
    SDL_Log("Failed to create graphics pipeline");
    return SDL_APP_FAILURE;
  }

  // clean up shaders after pipeline creation
  SDL_ReleaseGPUShader(device, vertexShader);
  SDL_ReleaseGPUShader(device, fragmentShader);
  
  SDL_Log("Initialization complete");

  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
  if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
  {
    return SDL_APP_SUCCESS;
  }
  return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
  return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
  SDL_ReleaseWindowFromGPUDevice(device, window);
  SDL_DestroyWindow(window);
  SDL_DestroyGPUDevice(device);
}
