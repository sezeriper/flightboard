#include "app.hpp"
#include "utils.hpp"

#include <glm/glm.hpp>

#include <cstring>


// #include <imgui.h>
// #include <imgui_impl_sdl3.h>
// #include <imgui_impl_sdlgpu3.h>

namespace flb
{
namespace app
{

static SDL_AppResult createPipeline(State& state)
{
  // create shaders
  SDL_GPUShader* vertexShader = NULL;
  SDL_GPUShader* fragmentShader = NULL;
  SDL_AppResult shaderResult = createShaders(state.device, &vertexShader, &fragmentShader);
  if (shaderResult != SDL_APP_CONTINUE)
  {
    return shaderResult;
  }

  // create the pipeline
  SDL_GPUVertexBufferDescription vertexBufferDescriptions[1] = {
    {
      .slot = 0,
      .pitch = sizeof(Vertex),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
      .instance_step_rate = 0,
    }
  };

  SDL_GPUVertexAttribute vertexAttributes[2] = {
    {
      .location = 0,
      .buffer_slot = 0,
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
      .offset = offsetof(Vertex, position),
    },
    {
      .location = 1,
      .buffer_slot = 0,
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
      .offset = offsetof(Vertex, color),

    }
  };

  SDL_GPUColorTargetDescription colorTargetDescriptions[1] = {
    {
      .format = SDL_GetGPUSwapchainTextureFormat(state.device, state.window),
      .blend_state {
        .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
        .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .color_blend_op = SDL_GPU_BLENDOP_ADD,
        .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
        .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
        .enable_blend = true,
      },
    }
  };

  SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo {
    .vertex_shader = vertexShader,
    .fragment_shader = fragmentShader,
    .vertex_input_state {
      .vertex_buffer_descriptions = vertexBufferDescriptions,
      .num_vertex_buffers = 1,
      .vertex_attributes = vertexAttributes,
      .num_vertex_attributes = 2,
    },
    .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
    .rasterizer_state {
      .fill_mode = SDL_GPU_FILLMODE_FILL,
      .cull_mode = SDL_GPU_CULLMODE_BACK,
      .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
    },
    .depth_stencil_state {
      .compare_op = SDL_GPU_COMPAREOP_LESS,
      .enable_depth_test = true,
      .enable_depth_write = true,
    },
    .target_info {
      .color_target_descriptions = colorTargetDescriptions,
      .num_color_targets = 1,
      .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
      .has_depth_stencil_target = true,
    },
  };

  SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(
    state.device, &pipelineCreateInfo);
  if (pipeline == NULL)
  {
    SDL_Log("CreateGPUGraphicsPipeline failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  // clean up shaders after pipeline creation
  SDL_ReleaseGPUShader(state.device, vertexShader);
  SDL_ReleaseGPUShader(state.device, fragmentShader);

  state.pipeline = pipeline;
  return SDL_APP_CONTINUE;
}

static SDL_AppResult createVertexBuffer(State& state)
{
  // first create transfer buffer and copy vertex data to it
  SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo {
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    .size = sizeof(VERTICES),
    .props = 0,
  };
  SDL_GPUTransferBuffer* transferBuf =
    SDL_CreateGPUTransferBuffer(state.device, &transferBufferCreateInfo);
  if (transferBuf == NULL)
  {
    SDL_Log("CreateGPUTransferBuffer failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  Vertex* mappedBuf =
    (Vertex*)SDL_MapGPUTransferBuffer(state.device, transferBuf, false);
  if (mappedBuf == NULL)
  {
    SDL_Log("MapGPUTransferBuffer failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  std::memcpy(mappedBuf, VERTICES.data(), sizeof(VERTICES));
  SDL_UnmapGPUTransferBuffer(state.device, transferBuf);
  mappedBuf = NULL;

  // then create vertex buffer and copy data from transfer buffer to it
  SDL_GPUBufferCreateInfo bufferCreateInfo {
    .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
    .size = sizeof(VERTICES),
    .props = 0,
  };

  SDL_GPUBuffer* vertexBuffer =
    SDL_CreateGPUBuffer(state.device, &bufferCreateInfo);
  if (vertexBuffer == NULL)
  {
    SDL_Log("CreateGPUBuffer failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(state.device);
  SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);
  SDL_GPUTransferBufferLocation sourceBufferLocation {
    .transfer_buffer = transferBuf,
    .offset = 0,
  };
  SDL_GPUBufferRegion destBufferRegion {
    .buffer = vertexBuffer,
    .offset = 0,
    .size = sizeof(VERTICES),
  };

  SDL_UploadToGPUBuffer(copyPass, &sourceBufferLocation, &destBufferRegion, true);
  SDL_EndGPUCopyPass(copyPass);
  bool success = SDL_SubmitGPUCommandBuffer(cmdBuf);
  if (!success)
  {
    SDL_Log("SubmitGPUCommandBuffer failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_ReleaseGPUTransferBuffer(state.device, transferBuf);

  state.vertexBuffer = vertexBuffer;
  return SDL_APP_CONTINUE;
}

static SDL_AppResult createIndexBuffer(State& state)
{
  // first create transfer buffer and copy index data to it
  SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo {
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    .size = sizeof(INDICES),
    .props = 0,
  };
  SDL_GPUTransferBuffer* transferBuf =
    SDL_CreateGPUTransferBuffer(state.device, &transferBufferCreateInfo);
  if (transferBuf == NULL)
  {
    SDL_Log("CreateGPUTransferBuffer failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  std::uint32_t* mappedBuf =
    (std::uint32_t*)SDL_MapGPUTransferBuffer(state.device, transferBuf, false);
  if (mappedBuf == NULL)
  {
    SDL_Log("MapGPUTransferBuffer failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  std::memcpy(mappedBuf, INDICES.data(), sizeof(INDICES));
  SDL_UnmapGPUTransferBuffer(state.device, transferBuf);
  mappedBuf = NULL;

  // then create index buffer and copy data from transfer buffer to it
  SDL_GPUBufferCreateInfo bufferCreateInfo {
    .usage = SDL_GPU_BUFFERUSAGE_INDEX,
    .size = sizeof(INDICES),
    .props = 0,
  };

  SDL_GPUBuffer* indexBuffer =
    SDL_CreateGPUBuffer(state.device, &bufferCreateInfo);
  if (indexBuffer == NULL)
  {
    SDL_Log("CreateGPUBuffer failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(state.device);
  SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);
  SDL_GPUTransferBufferLocation sourceBufferLocation {
    .transfer_buffer = transferBuf,
    .offset = 0,
  };
  SDL_GPUBufferRegion destBufferRegion {
    .buffer = indexBuffer,
    .offset = 0,
    .size = sizeof(INDICES),
  };

  SDL_UploadToGPUBuffer(copyPass, &sourceBufferLocation, &destBufferRegion, true);
  SDL_EndGPUCopyPass(copyPass);
  bool success = SDL_SubmitGPUCommandBuffer(cmdBuf);
  if (!success)
  {
    SDL_Log("SubmitGPUCommandBuffer failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_ReleaseGPUTransferBuffer(state.device, transferBuf);

  state.indexBuffer = indexBuffer;
  return SDL_APP_CONTINUE;
}

static SDL_AppResult createDepthTexture(State& state, Uint32 width, Uint32 height)
{
  if (state.depthTexture != NULL)
  {
    SDL_ReleaseGPUTexture(state.device, state.depthTexture);
    state.depthTexture = NULL;
  }

  SDL_GPUTextureCreateInfo depthTextCreateInfo {
    .type = SDL_GPU_TEXTURETYPE_2D,
    .format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
    .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
    .width = width,
    .height = height,
    .layer_count_or_depth = 1,
    .num_levels = 1,
  };
  state.depthTexture =
    SDL_CreateGPUTexture(state.device, &depthTextCreateInfo);
  if (state.depthTexture == NULL)
  {
    SDL_Log("CreateGPUTexture failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  return SDL_APP_CONTINUE;
}

SDL_AppResult init(State& state)
{
  SDL_InitSubSystem(SDL_INIT_VIDEO);

  state.device = SDL_CreateGPUDevice(
    SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB,
    true, NULL);
  if (state.device == NULL)
  {
    SDL_Log("CreateGPUDevice failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  state.window = SDL_CreateWindow("flightboard v0.0.1", 1280, 720,
    SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (state.window == NULL)
  {
    SDL_Log("CreateWindow failed %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (!SDL_ClaimWindowForGPUDevice(state.device, state.window))
  {
    SDL_Log("ClaimWindowForGPUDevice failed: %s", SDL_GetError());
  }

  SDL_AppResult result = createPipeline(state);
  if (result != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }

  result = createVertexBuffer(state);
  if (result != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }

  result = createIndexBuffer(state);
  if (result != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }

  return SDL_APP_CONTINUE;
}

void cleanup(const State& state)
{
  SDL_ReleaseGPUTexture(state.device, state.depthTexture);
  SDL_ReleaseGPUGraphicsPipeline(state.device, state.pipeline);
  SDL_ReleaseWindowFromGPUDevice(state.device, state.window);
  SDL_DestroyWindow(state.window);
  SDL_DestroyGPUDevice(state.device);
}

SDL_AppResult handleEvent(State& state, SDL_Event* event)
{
  if (event->type == SDL_EVENT_QUIT ||
      event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
  {
    return SDL_APP_SUCCESS;
  }

  if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
  {
    auto width = event->window.data1;
    auto height = event->window.data2;
    state.camera.aspect =
      static_cast<float>(width) /
      static_cast<float>(height);

    SDL_AppResult result = createDepthTexture(state, width, height);
    if (result != SDL_APP_CONTINUE)
    {
      return SDL_APP_FAILURE;
    }
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult update(State& state, float dt)
{
  state.camera.pos = {0.0f, 0.0f, 3.0f};

  state.cubeRotation += glm::radians(90.0f) * dt; // rotate 90 degrees per second
  glm::mat4 model = glm::rotate(
    glm::mat4{1.0f},
    state.cubeRotation,
    UP
  );

  glm::mat4 view = state.camera.getViewProjMat();

  state.uniforms.modelViewProjection = view * model;

  return SDL_APP_CONTINUE;
}

SDL_AppResult draw(const State& state)
{
  SDL_GPUCommandBuffer* commandBuffer = SDL_AcquireGPUCommandBuffer(state.device);
  if (commandBuffer == NULL)
  {
    SDL_Log("AcquireGPUCommandBuffer failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_GPUTexture* swapchainTexture = NULL;
  if (!SDL_WaitAndAcquireGPUSwapchainTexture(
    commandBuffer, state.window, &swapchainTexture, NULL, NULL))
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

  SDL_GPUDepthStencilTargetInfo depthTargetInfo {
    .texture = state.depthTexture,
    .clear_depth = 1.0f,
    .load_op = SDL_GPU_LOADOP_CLEAR,
    .store_op = SDL_GPU_STOREOP_STORE,
  };

  // begin render pass
  SDL_GPURenderPass* renderPass =
    SDL_BeginGPURenderPass(commandBuffer, &colorTargetInfo, 1, &depthTargetInfo);
  SDL_BindGPUGraphicsPipeline(renderPass, state.pipeline);

  SDL_PushGPUVertexUniformData(
    commandBuffer, 0, &state.uniforms, sizeof(state.uniforms));

  SDL_GPUBufferBinding vertexBufferBinding {
    .buffer = state.vertexBuffer,
    .offset = 0,
  };
  SDL_BindGPUVertexBuffers(
    renderPass, 0, &vertexBufferBinding, 1);

  SDL_GPUBufferBinding indexBufferBinding {
    .buffer = state.indexBuffer,
    .offset = 0,
  };
  SDL_BindGPUIndexBuffer(
    renderPass, &indexBufferBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

  SDL_DrawGPUIndexedPrimitives(renderPass, INDICES.size(), 1, 0, 0, 0);

  SDL_EndGPURenderPass(renderPass);
  // end render pass

  SDL_SubmitGPUCommandBuffer(commandBuffer);

  return SDL_APP_CONTINUE;
}
} // namespace app
} // namespace flb
