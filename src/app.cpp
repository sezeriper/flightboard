#include "app.hpp"
#include "utils.hpp"


// #include <imgui.h>
// #include <imgui_impl_sdl3.h>
// #include <imgui_impl_sdlgpu3.h>

namespace flb
{
namespace app
{

SDL_AppResult init(State& state)
{
  SDL_InitSubSystem(SDL_INIT_VIDEO);

  SDL_GPUDevice* device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, true, NULL);
  if (device == NULL)
  {
    SDL_Log("CreateGPUDevice failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_Window* window = SDL_CreateWindow("flightboard v0.0.1", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_METAL | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (window == NULL)
  {
    SDL_Log("CreateWindow failed %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (!SDL_ClaimWindowForGPUDevice(device, window))
  {
    SDL_Log("ClaimWindowForGPUDevice failed");
  }

  // create shaders
  SDL_GPUShader* vertexShader = NULL;
  SDL_GPUShader* fragmentShader = NULL;
  SDL_AppResult shaderResult = createShaders(device, &vertexShader, &fragmentShader);
  if (shaderResult != SDL_APP_CONTINUE)
  {
    return shaderResult;
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

  SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineCreateInfo);
  if (pipeline == NULL)
  {
    SDL_Log("Failed to create graphics pipeline");
    return SDL_APP_FAILURE;
  }

  // clean up shaders after pipeline creation
  SDL_ReleaseGPUShader(device, vertexShader);
  SDL_ReleaseGPUShader(device, fragmentShader);

  state.window = window;
  state.device = device;
  state.pipeline = pipeline;
  return SDL_APP_CONTINUE;
}

void cleanup(const State& state)
{
  auto window = state.window;
  auto device = state.device;
  auto pipeline = state.pipeline;
  SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
  SDL_ReleaseWindowFromGPUDevice(device, window);
  SDL_DestroyWindow(window);
  SDL_DestroyGPUDevice(device);
}

SDL_AppResult draw(const State& state)
{
  SDL_GPUDevice* device = state.device;
  SDL_Window* window = state.window;
  SDL_GPUGraphicsPipeline* pipeline = state.pipeline;

  SDL_GPUCommandBuffer* commandBuffer = SDL_AcquireGPUCommandBuffer(device);
  if (commandBuffer == NULL)
  {
    SDL_Log("AcquireGPUCommandBuffer failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_GPUTexture* swapchainTexture = NULL;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(
    commandBuffer, window, &swapchainTexture, NULL, NULL))
  {
    SDL_Log("WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (swapchainTexture == NULL)
  {
    SDL_SubmitGPUCommandBuffer(commandBuffer);
    return SDL_APP_CONTINUE;
  }

  SDL_GPUColorTargetInfo colorTargetInfo {
    .texture = swapchainTexture,
    .clear_color {0.1f, 0.1f, 0.1f, 1.0f},
    .load_op = SDL_GPU_LOADOP_CLEAR,
    .store_op = SDL_GPU_STOREOP_STORE,
  };

  SDL_GPURenderPass* renderPass =
    SDL_BeginGPURenderPass(commandBuffer, &colorTargetInfo, 1, NULL);
  SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

  SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);
  SDL_EndGPURenderPass(renderPass);

  SDL_SubmitGPUCommandBuffer(commandBuffer);

  return SDL_APP_CONTINUE;
}
} // namespace app
} // namespace flb