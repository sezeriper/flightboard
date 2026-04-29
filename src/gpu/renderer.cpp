#include "gpu/renderer.hpp"
#include "SDL3/SDL_gpu.h"
#include "components.hpp"
#include "gpu/allocator.hpp"
#include "imgui_layer.hpp"
#include "obj_loader.hpp"
#include "tile_generator.hpp"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

using namespace flb;

namespace
{
void renderMain(const gpu::RenderContext& context, entt::registry& registry, const Camera& camera)
{
  gpu::bindPipeline(context);

  SDL_GPUBuffer* boundVertexBuffer = NULL;
  SDL_GPUBuffer* boundIndexBuffer = NULL;
  SDL_GPUTexture* boundTexture = NULL;

  const glm::mat4 viewProjMat = camera.getViewProjMat();
  const auto view = registry.view<component::Position, component::Model>();
  for (const auto entity : view)
  {
    // if (!registry.all_of<component::Visible>(entity))
    //   continue;

    const auto& position = view.get<component::Position>(entity);
    const auto& model = view.get<component::Model>(entity);
    const Mesh mesh = model.value.getMesh();
    const gpu::TextureHandle texture = model.value.getTexture();

    if (
      mesh.vertexBuffer.buffer == nullptr || mesh.indexBuffer.buffer == nullptr || texture.texture == nullptr ||
      mesh.indexCount == 0)
    {
      continue;
    }

    if (boundIndexBuffer != mesh.indexBuffer.buffer)
      gpu::bindIndexBuffer(context, mesh.indexBuffer.buffer);
    if (boundVertexBuffer != mesh.vertexBuffer.buffer)
      gpu::bindVertexBuffer(context, mesh.vertexBuffer.buffer);
    if (boundTexture != texture.texture)
      gpu::bindSampler(context, texture.texture);

    boundVertexBuffer = mesh.vertexBuffer.buffer;
    boundIndexBuffer = mesh.indexBuffer.buffer;
    boundTexture = texture.texture;

    glm::mat4 modelTransform{1.0f};
    if (const auto* transform = registry.try_get<component::Transform>(entity))
    {
      modelTransform = glm::mat4{transform->value};
    }

    const gpu::Uniforms uniforms{
      .viewProjection = viewProjMat,
      .modelPosition = glm::vec4{position.value - camera.position, 1.0f},
      .modelTransform = modelTransform,
    };
    SDL_PushGPUVertexUniformData(context.commandBuffer, 0, &uniforms, sizeof(uniforms));
    SDL_DrawGPUIndexedPrimitives(context.renderPass, mesh.indexCount, 1, 0, 0, 0);
  }
}

void renderTiles(
  const gpu::RenderContext& context, entt::registry& registry, const Camera& camera, SDL_GPUBuffer* tileIndexBuffer)
{
  gpu::bindPipeline(context);
  gpu::bindIndexBuffer(context, tileIndexBuffer);

  const glm::mat4 viewProjMat = camera.getViewProjMat();
  const auto group = registry.group<component::Position, component::VertexBuffer, component::Texture>();
  for (const auto [entity, position, vertexBuffer, texture] : group.each())
  {
    if (!registry.all_of<component::Visible>(entity))
      continue;

    gpu::bindVertexBuffer(context, vertexBuffer.value);
    gpu::bindSampler(context, texture.value);

    const gpu::Uniforms uniforms{
      .viewProjection = viewProjMat,
      .modelPosition = glm::vec4{position.value - camera.position, 1.0f},
      .modelTransform = glm::mat4{1.0f},
    };
    SDL_PushGPUVertexUniformData(context.commandBuffer, 0, &uniforms, sizeof(uniforms));
    SDL_DrawGPUIndexedPrimitives(context.renderPass, TILE_NUM_INDICES, 1, 0, 0, 0);
  }
}

void renderDebug(
  const gpu::RenderContext& context,
  entt::registry& registry,
  const Camera& camera,
  SDL_GPUBuffer* debugSphereVertexBuffer,
  SDL_GPUBuffer* debugSphereIndexBuffer,
  Uint32 debugSphereIndexCount)
{
  if (!debugSphereVertexBuffer || !debugSphereIndexBuffer)
    return;

  gpu::bindPipeline(context);
  gpu::bindVertexBuffer(context, debugSphereVertexBuffer);
  gpu::bindIndexBuffer(context, debugSphereIndexBuffer);

  const glm::mat4 viewProjMat = camera.getViewProjMat();
  const auto view = registry.view<component::Position, component::BoundingSphere>();
  for (const auto [entity, position, boundingSphere] : view.each())
  {
    if (!registry.all_of<component::Visible>(entity))
      continue;

    glm::mat4 modelTransform = glm::scale(glm::mat4(1.0f), glm::vec3(boundingSphere.value.radius));

    const gpu::Uniforms uniforms{
      .viewProjection = viewProjMat,
      .modelPosition = glm::vec4{boundingSphere.value.position - camera.position, 1.0f},
      .modelTransform = modelTransform,
    };
    SDL_PushGPUVertexUniformData(context.commandBuffer, 0, &uniforms, sizeof(uniforms));
    SDL_DrawGPUIndexedPrimitives(context.renderPass, debugSphereIndexCount, 1, 0, 0, 0);
  }
}

void renderIndicators(
  const gpu::RenderContext& context, entt::registry& registry, const Camera& camera, float alphaScale)
{
  gpu::bindPipeline(context);

  SDL_GPUBuffer* boundVertexBuffer = NULL;
  SDL_GPUBuffer* boundIndexBuffer = NULL;

  const glm::mat4 viewProjMat = camera.getViewProjMat();
  const auto view = registry.view<component::Position, component::IndicatorModel>();
  for (const auto entity : view)
  {
    const auto& position = view.get<component::Position>(entity);
    const auto& indicator = view.get<component::IndicatorModel>(entity);
    const Mesh mesh = indicator.value.getMesh();

    if (mesh.vertexBuffer.buffer == nullptr || mesh.indexBuffer.buffer == nullptr || mesh.indexCount == 0)
    {
      continue;
    }

    if (boundIndexBuffer != mesh.indexBuffer.buffer)
      gpu::bindIndexBuffer(context, mesh.indexBuffer.buffer);
    if (boundVertexBuffer != mesh.vertexBuffer.buffer)
      gpu::bindVertexBuffer(context, mesh.vertexBuffer.buffer);

    boundVertexBuffer = mesh.vertexBuffer.buffer;
    boundIndexBuffer = mesh.indexBuffer.buffer;

    glm::mat4 modelTransform{1.0f};
    if (const auto* transform = registry.try_get<component::Transform>(entity))
    {
      modelTransform = glm::mat4{transform->value};
    }

    const gpu::Uniforms uniforms{
      .viewProjection = viewProjMat,
      .modelPosition = glm::vec4{position.value - camera.position, 1.0f},
      .modelTransform = modelTransform,
    };
    glm::vec4 color = indicator.value.getColor();
    color.a = std::clamp(color.a * alphaScale, 0.0f, 1.0f);

    SDL_PushGPUVertexUniformData(context.commandBuffer, 0, &uniforms, sizeof(uniforms));
    SDL_PushGPUFragmentUniformData(context.commandBuffer, 0, &color, sizeof(color));
    SDL_DrawGPUIndexedPrimitives(context.renderPass, mesh.indexCount, 1, 0, 0, 0);
  }
}

void applyViewport(const gpu::RenderContext& context, const ViewportRect& rect)
{
  if (!rect.valid)
  {
    return;
  }

  SDL_GPUViewport viewport{
    .x = rect.x,
    .y = rect.y,
    .w = rect.width,
    .h = rect.height,
    .min_depth = 0.0f,
    .max_depth = 1.0f,
  };
  SDL_SetGPUViewport(context.renderPass, &viewport);

  const int scissorX = static_cast<int>(std::floor(rect.x));
  const int scissorY = static_cast<int>(std::floor(rect.y));
  const int scissorRight = static_cast<int>(std::ceil(rect.x + rect.width));
  const int scissorBottom = static_cast<int>(std::ceil(rect.y + rect.height));
  SDL_Rect scissor{
    .x = scissorX,
    .y = scissorY,
    .w = scissorRight - scissorX,
    .h = scissorBottom - scissorY,
  };
  SDL_SetGPUScissor(context.renderPass, &scissor);
}

} // namespace

namespace flb
{

SDL_AppResult Renderer::init(SDL_Window* window)
{
  if (device.init() != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }

  if (!SDL_ClaimWindowForGPUDevice(device.getPtr(), window))
  {
    SDL_Log("ClaimWindowForGPUDevice failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (!SDL_ClaimWindowForGPUDevice(device.getPtr(), window))
  {
    SDL_Log("ClaimWindowForGPUDevice failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  gpu::PipelineConfig mainConfig{
    .vertexShaderPath = "content/shaders/lighting_basic.vert.hlsl",
    .fragmentShaderPath = "content/shaders/lighting_basic.frag.hlsl",
  };

  if (mainPipeline.init(device.getPtr(), window, mainConfig) != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }

  gpu::PipelineConfig debugConfig{
    .vertexShaderPath = "content/shaders/lighting_basic.vert.hlsl",
    .fragmentShaderPath = "content/shaders/debug_color.frag.hlsl",
  };

  if (debugPipeline.init(device.getPtr(), window, debugConfig) != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }

  gpu::PipelineConfig indicatorDepthConfig{
    .vertexShaderPath = "content/shaders/lighting_basic.vert.hlsl",
    .fragmentShaderPath = "content/shaders/indicator_color.frag.hlsl",
    .cullMode = SDL_GPU_CULLMODE_NONE,
    .enableDepthWrite = true,
  };

  if (indicatorDepthPipeline.init(device.getPtr(), window, indicatorDepthConfig) != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }

  gpu::PipelineConfig indicatorConfig{
    .vertexShaderPath = "content/shaders/lighting_basic.vert.hlsl",
    .fragmentShaderPath = "content/shaders/indicator_color.frag.hlsl",
    .cullMode = SDL_GPU_CULLMODE_NONE,
    .compareOp = SDL_GPU_COMPAREOP_GREATER_OR_EQUAL,
    .enableDepthWrite = false,
  };

  if (indicatorPipeline.init(device.getPtr(), window, indicatorConfig) != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }

  if (sampler.init(device.getPtr()) != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }

  sceneColorFormat = SDL_GetGPUSwapchainTextureFormat(device.getPtr(), window);

  SDL_PropertiesID props = SDL_GetGPUDeviceProperties(device.getPtr());
  auto deviceName = SDL_GetStringProperty(props, SDL_PROP_GPU_DEVICE_NAME_STRING, "Unknown GPU");
  SDL_Log("Using GPU: %s", deviceName);
  auto driverName = SDL_GetStringProperty(props, SDL_PROP_GPU_DEVICE_DRIVER_NAME_STRING, "Unknown Driver");
  SDL_Log("GPU Driver: %s", driverName);
  auto driverVersion = SDL_GetStringProperty(props, SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING, "Unknown Version");
  SDL_Log("GPU Driver Version: %s", driverVersion);
  auto driverInfo = SDL_GetStringProperty(props, SDL_PROP_GPU_DEVICE_DRIVER_INFO_STRING, "No additional info");
  SDL_Log("GPU Driver Info: %s", driverInfo);
  auto backend = SDL_GetGPUDeviceDriver(device.getPtr());
  SDL_Log("GPU Backend: %s", backend);

  return SDL_APP_CONTINUE;
}

SDL_AppResult Renderer::initDebugSphere(gpu::Allocator& allocator)
{
  auto [vertices, indices] = loadOBJ("content/models/debug/sphere.obj");
  if (vertices.empty() || indices.empty())
  {
    SDL_Log("Failed to load debug sphere OBJ");
    return SDL_APP_FAILURE;
  }

  debugSphereIndexCount = indices.size();

  auto vboHandle = allocator.createVertexBuffer(vertices.size() * sizeof(gpu::Vertex));
  auto iboHandle = allocator.createIndexBuffer(indices.size() * sizeof(gpu::Index));

  auto vboMem = allocator.allocateBuffer(vboHandle);
  auto iboMem = allocator.allocateBuffer(iboHandle);

  memcpy(vboMem.data(), vertices.data(), vboHandle.size);
  memcpy(iboMem.data(), indices.data(), iboHandle.size);

  debugSphereVertexBuffer = vboHandle.buffer;
  debugSphereIndexBuffer = iboHandle.buffer;

  return SDL_APP_CONTINUE;
}

void Renderer::initTileIndexBuffer(gpu::Allocator& allocator)
{
  const auto bufHandle = allocator.createIndexBuffer(TILE_INDEX_BUFFER_SIZE);
  tileIndexBuffer = bufHandle.buffer;
  const auto bufMemory = allocator.allocateBuffer(bufHandle);
  std::span<gpu::Index> indices(reinterpret_cast<gpu::Index*>(bufMemory.data()), TILE_NUM_INDICES);
  generateTileIndices(indices);
}

void Renderer::cleanup(SDL_Window* window)
{
  releaseSceneTarget();

  SDL_ReleaseGPUBuffer(device.getPtr(), debugSphereVertexBuffer);
  SDL_ReleaseGPUBuffer(device.getPtr(), debugSphereIndexBuffer);
  SDL_ReleaseGPUBuffer(device.getPtr(), tileIndexBuffer);

  sampler.cleanup(device.getPtr());
  mainPipeline.cleanup(device.getPtr());
  debugPipeline.cleanup(device.getPtr());
  indicatorDepthPipeline.cleanup(device.getPtr());
  indicatorPipeline.cleanup(device.getPtr());
  SDL_ReleaseWindowFromGPUDevice(device.getPtr(), window);
  device.cleanup();
}

void Renderer::releaseSceneTarget()
{
  if (sceneTarget.colorTexture != nullptr)
  {
    SDL_ReleaseGPUTexture(device.getPtr(), sceneTarget.colorTexture);
  }
  if (sceneTarget.depthTexture != nullptr)
  {
    SDL_ReleaseGPUTexture(device.getPtr(), sceneTarget.depthTexture);
  }

  sceneTarget = {};
}

SDL_AppResult Renderer::ensureSceneTarget(const ViewportRect& rect)
{
  if (!rect.valid)
  {
    return SDL_APP_CONTINUE;
  }

  const auto width = static_cast<Uint32>(std::ceil(rect.width));
  const auto height = static_cast<Uint32>(std::ceil(rect.height));
  if (width == 0 || height == 0)
  {
    return SDL_APP_CONTINUE;
  }

  if (sceneTarget.colorTexture != nullptr && sceneTarget.width == width && sceneTarget.height == height)
  {
    return SDL_APP_CONTINUE;
  }

  releaseSceneTarget();

  SDL_GPUTextureCreateInfo colorTextureCreateInfo{
    .type = SDL_GPU_TEXTURETYPE_2D,
    .format = sceneColorFormat,
    .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER,
    .width = width,
    .height = height,
    .layer_count_or_depth = 1,
    .num_levels = 1,
  };
  sceneTarget.colorTexture = SDL_CreateGPUTexture(device.getPtr(), &colorTextureCreateInfo);
  if (sceneTarget.colorTexture == nullptr)
  {
    SDL_Log("CreateGPUTexture scene color failed: %s", SDL_GetError());
    releaseSceneTarget();
    return SDL_APP_FAILURE;
  }

  SDL_GPUTextureCreateInfo depthTextureCreateInfo{
    .type = SDL_GPU_TEXTURETYPE_2D,
    .format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
    .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
    .width = width,
    .height = height,
    .layer_count_or_depth = 1,
    .num_levels = 1,
  };
  sceneTarget.depthTexture = SDL_CreateGPUTexture(device.getPtr(), &depthTextureCreateInfo);
  if (sceneTarget.depthTexture == nullptr)
  {
    SDL_Log("CreateGPUTexture scene depth failed: %s", SDL_GetError());
    releaseSceneTarget();
    return SDL_APP_FAILURE;
  }

  sceneTarget.width = width;
  sceneTarget.height = height;

  return SDL_APP_CONTINUE;
}

SDL_AppResult Renderer::draw(
  entt::registry& registry, const Camera& camera, const Window& window, const ImGuiLayer* imGuiLayer) const
{
  gpu::RenderContext context;
  context.pipeline = mainPipeline.get();
  context.sampler = sampler.getSampler();
  context.commandBuffer = device.getDrawCommandBuffer();
  SDL_GPUTexture* swapchainTexture = window.getSwapChainTexture(context);

  // window is minimized or 0 size, exit early
  if (swapchainTexture == NULL)
  {
    SDL_SubmitGPUCommandBuffer(context.commandBuffer);
    return SDL_APP_CONTINUE;
  }

  if (sceneTarget.colorTexture != nullptr && sceneTarget.depthTexture != nullptr)
  {
    context.swapchainTexture = sceneTarget.colorTexture;
    context.depthTexture = sceneTarget.depthTexture;
    context.renderPass = gpu::beginRenderPass(context);

    ViewportRect sceneRect{
      .width = static_cast<float>(sceneTarget.width),
      .height = static_cast<float>(sceneTarget.height),
      .valid = true,
    };
    applyViewport(context, sceneRect);

    context.pipeline = mainPipeline.get();
    renderMain(context, registry, camera);
    renderTiles(context, registry, camera, tileIndexBuffer);

    context.pipeline = indicatorDepthPipeline.get();
    renderIndicators(context, registry, camera, 0.0f);

    context.pipeline = indicatorPipeline.get();
    renderIndicators(context, registry, camera, 1.0f);

    // context.pipeline = debugPipeline.get();
    // renderDebug(
    //   context,
    //   registry,
    //   camera,
    //   debugSphereVertexBuffer,
    //   debugSphereIndexBuffer,
    //   debugSphereIndexCount);

    gpu::endRenderPass(context);
  }

  if (imGuiLayer != nullptr && imGuiLayer->hasDrawData())
  {
    imGuiLayer->prepareDrawData(context.commandBuffer);
    context.swapchainTexture = swapchainTexture;
    context.renderPass = gpu::beginColorClearRenderPass(context);
    imGuiLayer->renderDrawData(context.commandBuffer, context.renderPass);
    gpu::endRenderPass(context);
  }

  gpu::submitCommandBuffer(context);

  return SDL_APP_CONTINUE;
}

} // namespace flb
