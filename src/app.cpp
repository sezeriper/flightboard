#include "app.hpp"
#include "utils.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>

namespace flb
{
SDL_AppResult App::createPipeline()
{
  // create shaders
  SDL_GPUShader* vertexShader = NULL;
  SDL_GPUShader* fragmentShader = NULL;
  SDL_AppResult shaderResult = createShaders(device, &vertexShader, &fragmentShader);
  if (shaderResult != SDL_APP_CONTINUE)
  {
    return shaderResult;
  }

  // create the pipeline
  SDL_GPUVertexBufferDescription vertexBufferDescriptions[1] {
    {
      .slot = 0,
      .pitch = sizeof(Vertex),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
      .instance_step_rate = 0,
    }
  };

  SDL_GPUVertexAttribute vertexAttributes[2] {
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

  SDL_GPUColorTargetDescription colorTargetDescriptions[1] {
    {
      .format = SDL_GetGPUSwapchainTextureFormat(device, window),
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

  SDL_GPUGraphicsPipeline* p = SDL_CreateGPUGraphicsPipeline(
    device, &pipelineCreateInfo);
  if (p == NULL)
  {
    SDL_Log("CreateGPUGraphicsPipeline failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  // clean up shaders after pipeline creation
  SDL_ReleaseGPUShader(device, vertexShader);
  SDL_ReleaseGPUShader(device, fragmentShader);

  pipeline = p;
  return SDL_APP_CONTINUE;
}

SDL_AppResult App::createVertexBuffer(const std::span<const Vertex> vertices)
{
  // first create transfer buffer and copy vertex data to it
  const Uint32 vertexBufferSize = static_cast<Uint32>(vertices.size_bytes());
  SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo {
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    .size = vertexBufferSize,
    .props = 0,
  };
  SDL_GPUTransferBuffer* transferBuf =
    SDL_CreateGPUTransferBuffer(device, &transferBufferCreateInfo);
  if (transferBuf == NULL)
  {
    SDL_Log("CreateGPUTransferBuffer failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  Vertex* mappedBuf =
    (Vertex*)SDL_MapGPUTransferBuffer(device, transferBuf, false);
  if (mappedBuf == NULL)
  {
    SDL_Log("MapGPUTransferBuffer failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  std::copy(vertices.begin(), vertices.end(), mappedBuf);
  SDL_UnmapGPUTransferBuffer(device, transferBuf);
  mappedBuf = NULL;

  // then create vertex buffer and copy data from transfer buffer to it
  SDL_GPUBufferCreateInfo bufferCreateInfo {
    .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
    .size = vertexBufferSize,
    .props = 0,
  };

  SDL_GPUBuffer* buf =
    SDL_CreateGPUBuffer(device, &bufferCreateInfo);
  if (buf == NULL)
  {
    SDL_Log("CreateGPUBuffer failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(device);
  SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);
  SDL_GPUTransferBufferLocation sourceBufferLocation {
    .transfer_buffer = transferBuf,
    .offset = 0,
  };
  SDL_GPUBufferRegion destBufferRegion {
    .buffer = buf,
    .offset = 0,
    .size = vertexBufferSize,
  };

  SDL_UploadToGPUBuffer(copyPass, &sourceBufferLocation, &destBufferRegion, false);
  SDL_EndGPUCopyPass(copyPass);
  bool success = SDL_SubmitGPUCommandBuffer(cmdBuf);
  if (!success)
  {
    SDL_Log("SubmitGPUCommandBuffer failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_ReleaseGPUTransferBuffer(device, transferBuf);

  vertexBuffer = buf;
  return SDL_APP_CONTINUE;
}

SDL_AppResult App::createIndexBuffer(const std::span<const Index> indices)
{
  // first create transfer buffer and copy index data to it
  const Uint32 indexBufferSize = static_cast<Uint32>(indices.size_bytes());
  SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo {
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    .size = indexBufferSize,
    .props = 0,
  };
  SDL_GPUTransferBuffer* transferBuf =
    SDL_CreateGPUTransferBuffer(device, &transferBufferCreateInfo);
  if (transferBuf == NULL)
  {
    SDL_Log("CreateGPUTransferBuffer failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  Index* mappedBuf =
    (Index*)SDL_MapGPUTransferBuffer(device, transferBuf, false);
  if (mappedBuf == NULL)
  {
    SDL_Log("MapGPUTransferBuffer failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  std::copy(indices.begin(), indices.end(), mappedBuf);
  SDL_UnmapGPUTransferBuffer(device, transferBuf);
  mappedBuf = NULL;

  // then create index buffer and copy data from transfer buffer to it
  SDL_GPUBufferCreateInfo bufferCreateInfo {
    .usage = SDL_GPU_BUFFERUSAGE_INDEX,
    .size = indexBufferSize,
    .props = 0,
  };

  SDL_GPUBuffer* buf =
    SDL_CreateGPUBuffer(device, &bufferCreateInfo);
  if (buf == NULL)
  {
    SDL_Log("CreateGPUBuffer failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_GPUCommandBuffer* cmdBuf = SDL_AcquireGPUCommandBuffer(device);
  SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmdBuf);
  SDL_GPUTransferBufferLocation sourceBufferLocation {
    .transfer_buffer = transferBuf,
    .offset = 0,
  };
  SDL_GPUBufferRegion destBufferRegion {
    .buffer = buf,
    .offset = 0,
    .size = indexBufferSize,
  };

  SDL_UploadToGPUBuffer(copyPass, &sourceBufferLocation, &destBufferRegion, true);
  SDL_EndGPUCopyPass(copyPass);
  bool success = SDL_SubmitGPUCommandBuffer(cmdBuf);
  if (!success)
  {
    SDL_Log("SubmitGPUCommandBuffer failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  SDL_ReleaseGPUTransferBuffer(device, transferBuf);

  indexBuffer = buf;
  return SDL_APP_CONTINUE;
}

SDL_AppResult App::createDepthTexture(Uint32 width, Uint32 height)
{
  if (depthTexture != NULL)
  {
    SDL_ReleaseGPUTexture(device, depthTexture);
    depthTexture = NULL;
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
  depthTexture =
    SDL_CreateGPUTexture(device, &depthTextCreateInfo);
  if (depthTexture == NULL)
  {
    SDL_Log("CreateGPUTexture failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  return SDL_APP_CONTINUE;
}

SDL_AppResult App::init()
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

  if (!SDL_ClaimWindowForGPUDevice(device, window))
  {
    SDL_Log("ClaimWindowForGPUDevice failed: %s", SDL_GetError());
  }

  SDL_AppResult result = createPipeline();
  if (result != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }

  const std::array<Vertex, 8> vertices {{
    Vertex{{-0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}}, // 0
    Vertex{{ 0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}}, // 1
    Vertex{{ 0.5f,  0.5f, -0.5f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}, // 2
    Vertex{{-0.5f,  0.5f, -0.5f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}}, // 3
    Vertex{{-0.5f, -0.5f,  0.5f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}}, // 4
    Vertex{{ 0.5f, -0.5f,  0.5f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}}, // 5
    Vertex{{ 0.5f,  0.5f,  0.5f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 6
    Vertex{{-0.5f,  0.5f,  0.5f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}  // 7
  }};
  result = createVertexBuffer(vertices);
  if (result != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }

  const std::array<Index, 36> indices {
    0, 3, 2, 2, 1, 0, // back face
    4, 5, 6, 6, 7, 4, // front face
    0, 4, 7, 7, 3, 0, // left face
    1, 2, 6, 6, 5, 1, // right face
    3, 7, 6, 6, 2, 3, // top face
    0, 1, 5, 5, 4, 0  // bottom face
  };
  result = createIndexBuffer(indices);
  if (result != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }

  return SDL_APP_CONTINUE;
}

void App::cleanup()
{
  SDL_ReleaseGPUTexture(device, depthTexture);
  SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
  SDL_ReleaseWindowFromGPUDevice(device, window);
  SDL_DestroyWindow(window);
  SDL_DestroyGPUDevice(device);
}

SDL_AppResult App::handleEvent(SDL_Event* event)
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
    camera.aspect =
      static_cast<float>(width) /
      static_cast<float>(height);

    SDL_AppResult result = createDepthTexture(width, height);
    if (result != SDL_APP_CONTINUE)
    {
      return SDL_APP_FAILURE;
    }
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult App::update(float dt)
{
  camera.pos = {0.0f, 0.0f, 3.0f};

  cubeRotation += glm::radians(90.0f) * dt; // rotate 90 degrees per second
  glm::mat4 model = glm::rotate(
    glm::mat4{1.0f},
    cubeRotation,
    UP
  );

  glm::mat4 view = camera.getViewProjMat();

  uniforms.modelViewProjection = view * model;

  return SDL_APP_CONTINUE;
}

SDL_AppResult App::draw()
{
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

  SDL_GPUDepthStencilTargetInfo depthTargetInfo {
    .texture = depthTexture,
    .clear_depth = 1.0f,
    .load_op = SDL_GPU_LOADOP_CLEAR,
    .store_op = SDL_GPU_STOREOP_STORE,
  };

  // begin render pass
  SDL_GPURenderPass* renderPass =
    SDL_BeginGPURenderPass(commandBuffer, &colorTargetInfo, 1, &depthTargetInfo);
  SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

  SDL_PushGPUVertexUniformData(
    commandBuffer, 0, &uniforms, sizeof(uniforms));

  SDL_GPUBufferBinding vertexBufferBinding {
    .buffer = vertexBuffer,
    .offset = 0,
  };
  SDL_BindGPUVertexBuffers(
    renderPass, 0, &vertexBufferBinding, 1);

  SDL_GPUBufferBinding indexBufferBinding {
    .buffer = indexBuffer,
    .offset = 0,
  };
  SDL_BindGPUIndexBuffer(
    renderPass, &indexBufferBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

  // TODO: use actual index count instead of hardcoding 36
  SDL_DrawGPUIndexedPrimitives(renderPass, 36, 1, 0, 0, 0);

  SDL_EndGPURenderPass(renderPass);
  // end render pass

  SDL_SubmitGPUCommandBuffer(commandBuffer);

  return SDL_APP_CONTINUE;
}

} // namespace flb
