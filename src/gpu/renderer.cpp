#include "gpu/renderer.hpp"
#include "SDL3/SDL_gpu.h"
#include "components.hpp"
#include "gpu/allocator.hpp"
#include "obj_loader.hpp"
#include "tile_generator.hpp"
#include <glm/gtc/matrix_transform.hpp>

using namespace flb;

namespace
{
void renderMain(const gpu::RenderContext& context, entt::registry& registry, const FPSCamera& camera)
{
  gpu::bindPipeline(context);

  SDL_GPUBuffer* boundVertexBuffer = NULL;
  SDL_GPUBuffer* boundIndexBuffer = NULL;
  SDL_GPUTexture* boundTexture = NULL;

  const glm::mat4 viewProjMat = camera.getViewProjMat();
  const auto view = registry.view<
    component::Position,
    component::VertexBuffer,
    component::IndexBuffer,
    component::IndexCount,
    component::Texture>();
  for (const auto [entity, position, vertexBuffer, indexBuffer, indexCount, texture] : view.each())
  {
    // if (!registry.all_of<component::Visible>(entity))
    //   continue;

    if (boundIndexBuffer != indexBuffer.value)
      gpu::bindIndexBuffer(context, indexBuffer.value);
    if (boundVertexBuffer != vertexBuffer.value)
      gpu::bindVertexBuffer(context, vertexBuffer.value);
    if (boundTexture != texture.value)
      gpu::bindSampler(context, texture.value);

    boundVertexBuffer = vertexBuffer.value;
    boundIndexBuffer = indexBuffer.value;
    boundTexture = texture.value;

    const gpu::Uniforms uniforms{
      .viewProjection = viewProjMat,
      .modelPosition = glm::vec4{position.value - camera.position, 1.0f},
      .modelTransform = glm::mat4{1.0f},
    };
    SDL_PushGPUVertexUniformData(context.commandBuffer, 0, &uniforms, sizeof(uniforms));
    SDL_DrawGPUIndexedPrimitives(context.renderPass, indexCount.value, 1, 0, 0, 0);
    // SDL_Log("LOOO");
  }
  // SDL_Log("LOOO2");
}

void renderTiles(
  const gpu::RenderContext& context, entt::registry& registry, const FPSCamera& camera, SDL_GPUBuffer* tileIndexBuffer)
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
  const FPSCamera& camera,
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

  if (sampler.init(device.getPtr()) != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }

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
  SDL_ReleaseGPUBuffer(device.getPtr(), debugSphereVertexBuffer);
  SDL_ReleaseGPUBuffer(device.getPtr(), debugSphereIndexBuffer);
  SDL_ReleaseGPUBuffer(device.getPtr(), tileIndexBuffer);

  sampler.cleanup(device.getPtr());
  mainPipeline.cleanup(device.getPtr());
  debugPipeline.cleanup(device.getPtr());
  SDL_ReleaseWindowFromGPUDevice(device.getPtr(), window);
  device.cleanup();
}

SDL_AppResult Renderer::draw(entt::registry& registry, const FPSCamera& camera, const Window& window) const
{
  gpu::RenderContext context;
  context.pipeline = mainPipeline.get();
  context.sampler = sampler.getSampler();
  context.commandBuffer = device.getDrawCommandBuffer();
  context.swapchainTexture = window.getSwapChainTexture(context);
  context.depthTexture = device.getDepthTexture();

  // window is minimized or 0 size, exit early
  if (context.swapchainTexture == NULL)
  {
    SDL_SubmitGPUCommandBuffer(context.commandBuffer);
    return SDL_APP_CONTINUE;
  }

  context.renderPass = gpu::beginRenderPass(context);

  context.pipeline = mainPipeline.get();
  renderMain(context, registry, camera);
  renderTiles(context, registry, camera, tileIndexBuffer);

  // context.pipeline = debugPipeline.get();
  // renderDebug(
  //   context,
  //   registry,
  //   camera,
  //   debugSphereVertexBuffer,
  //   debugSphereIndexBuffer,
  //   debugSphereIndexCount);

  gpu::endRenderPass(context);

  return SDL_APP_CONTINUE;
}

} // namespace flb
