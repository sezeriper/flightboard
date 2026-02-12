#include "app.hpp"
#include "utils.hpp"
#include "obj_loader.hpp"

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

  SDL_GPUVertexAttribute vertexAttributes[3] {
    {
      .location = 0,
      .buffer_slot = 0,
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
      .offset = offsetof(Vertex, position),
    },
    {
      .location = 1,
      .buffer_slot = 0,
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
      .offset = offsetof(Vertex, color),

    },
    {
      .location = 2,
      .buffer_slot = 0,
      .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
      .offset = offsetof(Vertex, normal),
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
      .num_vertex_attributes = 3,
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

template<typename R>
requires std::ranges::contiguous_range<R> &&
         std::ranges::sized_range<R> &&
         VertexOrIndex<std::ranges::range_value_t<R>>
SDL_AppResult App::uploadDataToGPUBuffer(const R& data, SDL_GPUBuffer** outBuffer) const
{
  using T = std::ranges::range_value_t<R>;
  const Uint32 bufferSize = static_cast<Uint32>(std::ranges::size(data) * sizeof(T));

  SDL_GPUTransferBufferCreateInfo transferBufferCreateInfo {
    .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
    .size = bufferSize,
    .props = 0,
  };
  SDL_GPUTransferBuffer* transferBuf =
    SDL_CreateGPUTransferBuffer(device, &transferBufferCreateInfo);
  if (transferBuf == NULL)
  {
    SDL_Log("CreateGPUTransferBuffer failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  T* mappedBuf = reinterpret_cast<T*>(
    SDL_MapGPUTransferBuffer(device, transferBuf, false));
  if (mappedBuf == NULL)
  {
    SDL_Log("MapGPUTransferBuffer failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  std::ranges::copy(data, mappedBuf);
  SDL_UnmapGPUTransferBuffer(device, transferBuf);
  mappedBuf = NULL;

  // then create vertex buffer and copy data from transfer buffer to it
  SDL_GPUBufferCreateInfo bufferCreateInfo {
    .size = bufferSize,
    .props = 0,
  };

  if constexpr (std::same_as<T, Vertex>)
  {
    bufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
  }
  else if constexpr (std::same_as<T, Index>)
  {
    bufferCreateInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
  }

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
    .size = bufferSize,
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

  *outBuffer = buf;
  return SDL_APP_CONTINUE;
}

SDL_AppResult App::uploadMeshToGPUBuffers(const MeshData& meshData, MeshGPUBuffers& outBuffers) const
{
  SDL_GPUBuffer* vertexBuf;
  SDL_AppResult result = uploadDataToGPUBuffer(meshData.vertices, &vertexBuf);
  if (result == SDL_APP_FAILURE)
  {
    SDL_Log("Failed to upload vertex buffer");
    return SDL_APP_FAILURE;
  }

  SDL_GPUBuffer* indexBuf;
  result = uploadDataToGPUBuffer(meshData.indices, &indexBuf);
  if (result == SDL_APP_FAILURE)
  {
    SDL_Log("Failed to upload index buffer");
    return SDL_APP_FAILURE;
  }

  outBuffers = {
    .vertexBuffer = vertexBuf,
    .indexBuffer = indexBuf,
    .numOfIndices = static_cast<Uint32>(meshData.indices.size()),
  };
  return SDL_APP_CONTINUE;
}

SDL_AppResult App::createModel(const MeshData& meshData, const Transform& transform)
{
  MeshGPUBuffers buffers;
  SDL_AppResult result = uploadMeshToGPUBuffers(meshData, buffers);
  if (result == SDL_APP_FAILURE)
  {
    SDL_Log("Failed to upload mesh data to gpu");
    return SDL_APP_FAILURE;
  }

  const auto entity = registry.create();
  registry.emplace<Transform>(entity, transform);
  registry.emplace<MeshGPUBuffers>(entity, buffers);
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

  device = SDL_CreateGPUDevice(
    SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_METALLIB,
    true, NULL);
  if (device == NULL)
  {
    SDL_Log("CreateGPUDevice failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  window = SDL_CreateWindow("flightboard v0.0.1", 1280, 720,
    SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE);
  if (window == NULL)
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

  camera.center = {0.0f, 0.0f, 0.0f};
  camera.distance = 5.0f;

  const auto planeMesh = loadOBJ("content/models/floatplane/floatplane.obj");
  createModel(planeMesh, glm::scale(glm::mat4{1.0f}, glm::vec3{0.01f}));

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

  if (event->type == SDL_EVENT_MOUSE_MOTION && event->motion.state == SDL_BUTTON_LEFT)
  {
    const auto dtx = event->motion.xrel;
    const auto dty = event->motion.yrel;
    camera.yaw += dtx * mouseSensitivity;
    camera.pitch -= dty * mouseSensitivity;
  }

  if (event->type == SDL_EVENT_MOUSE_WHEEL)
  {
    camera.distance += event->wheel.y * scrollSensitivity;
    camera.distance = glm::clamp(camera.distance, 2.0f, 10.0f);
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult App::update(float dt)
{
  // auto view = registry.view<Transform>();
  // for (auto entity: view)
  // {
  //   auto& transform = view.get<Transform>(entity);
  //   transform = glm::rotate(transform, dt, UP);
  // }

  const bool* keyStates = SDL_GetKeyboardState(NULL);
  if (keyStates[SDL_SCANCODE_W])
  {
    camera.pitch -= keyboardSensitivity * dt;
  }
  if (keyStates[SDL_SCANCODE_A])
  {
    camera.yaw -= keyboardSensitivity * dt;
  }
  if (keyStates[SDL_SCANCODE_S])
  {
    camera.pitch += keyboardSensitivity * dt;
  }
  if (keyStates[SDL_SCANCODE_D])
  {
    camera.yaw += keyboardSensitivity * dt;
  }

  camera.pitch = glm::clamp(camera.pitch, glm::radians(-89.9f), glm::radians(89.9f));

  return SDL_APP_CONTINUE;
}

SDL_AppResult App::draw() const
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

  // window is minimized or 0 size, exit early
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

  auto view = registry.view<MeshGPUBuffers, Transform>();
  for (const auto [entity, buffers, transform]: view.each())
  {
    SDL_GPUBufferBinding vertexBufferBinding {
      .buffer = buffers.vertexBuffer,
      .offset = 0,
    };
    SDL_BindGPUVertexBuffers(
      renderPass, 0, &vertexBufferBinding, 1);

    SDL_GPUBufferBinding indexBufferBinding {
      .buffer = buffers.indexBuffer,
      .offset = 0,
    };
    SDL_BindGPUIndexBuffer(
      renderPass, &indexBufferBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    const Uniforms uniforms {
      .viewProjection = camera.getViewProjMat(),
      .model = transform
    };
    SDL_PushGPUVertexUniformData(
      commandBuffer, 0, &uniforms, sizeof(uniforms));
    SDL_DrawGPUIndexedPrimitives(renderPass, buffers.numOfIndices, 1, 0, 0, 0);
  }

  SDL_EndGPURenderPass(renderPass);
  // end render pass

  SDL_SubmitGPUCommandBuffer(commandBuffer);

  return SDL_APP_CONTINUE;
}

} // namespace flb
