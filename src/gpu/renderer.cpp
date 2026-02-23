#include "gpu/renderer.hpp"
#include "components.hpp"
#include "tile_generator.hpp"

using namespace flb;

namespace
{
void executeRenderLoop(const gpu::RenderContext& context, entt::registry& registry, const FPSCamera& camera)
{
  SDL_GPUBuffer* boundVertexBuffer = NULL;
  SDL_GPUBuffer* boundIndexBuffer = NULL;
  SDL_GPUTexture* boundTexture = NULL;

  const glm::mat4 viewProjMat = camera.getViewProjMat();
  const auto group =
    registry.group<component::Position, component::VertexBuffer, component::IndexBuffer, component::Texture>();
  for (const auto [entity, position, vertexBuffer, indexBuffer, texture] : group.each())
  {
    if (boundIndexBuffer != indexBuffer.value)
      gpu::bindIndexBuffer(context, indexBuffer.value);
    if (boundVertexBuffer != vertexBuffer.value)
      gpu::bindVertexBuffer(context, vertexBuffer.value);
    if (boundTexture != texture.value)
      gpu::bindSampler(context, texture.value);

    boundVertexBuffer = vertexBuffer.value;
    boundIndexBuffer = indexBuffer.value;
    boundTexture = texture.value;

    const gpu::Uniforms uniforms {
      .viewProjection = viewProjMat,
      .modelPosition = glm::vec4 {position.value - camera.position, 1.0f},
      .modelTransform = glm::mat4 {1.0f},
    };
    SDL_PushGPUVertexUniformData(context.commandBuffer, 0, &uniforms, sizeof(uniforms));
    SDL_DrawGPUIndexedPrimitives(context.renderPass, NUM_INDICES_PER_TILE, 1, 0, 0, 0);
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

  if (!SDL_ClaimWindowForGPUDevice(device.getDevice(), window))
  {
    SDL_Log("ClaimWindowForGPUDevice failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (pipeline.init(device.getDevice(), window) != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }

  if (sampler.init(device.getDevice()) != SDL_APP_CONTINUE)
  {
    return SDL_APP_FAILURE;
  }

  SDL_PropertiesID props = SDL_GetGPUDeviceProperties(device.getDevice());
  auto deviceName = SDL_GetStringProperty(props, SDL_PROP_GPU_DEVICE_NAME_STRING, "Unknown GPU");
  SDL_Log("Using GPU: %s", deviceName);
  auto driverName = SDL_GetStringProperty(props, SDL_PROP_GPU_DEVICE_DRIVER_NAME_STRING, "Unknown Driver");
  SDL_Log("GPU Driver: %s", driverName);
  auto driverVersion = SDL_GetStringProperty(props, SDL_PROP_GPU_DEVICE_DRIVER_VERSION_STRING, "Unknown Version");
  SDL_Log("GPU Driver Version: %s", driverVersion);
  auto driverInfo = SDL_GetStringProperty(props, SDL_PROP_GPU_DEVICE_DRIVER_INFO_STRING, "No additional info");
  SDL_Log("GPU Driver Info: %s", driverInfo);

  auto backend = SDL_GetGPUDeviceDriver(device.getDevice());
  SDL_Log("GPU Backend: %s", backend);

  return SDL_APP_CONTINUE;
}

void Renderer::cleanup(SDL_Window* window)
{
  sampler.cleanup(device.getDevice());
  pipeline.cleanup(device.getDevice());
  SDL_ReleaseWindowFromGPUDevice(device.getDevice(), window);
  device.cleanup();
}

SDL_AppResult Renderer::draw(entt::registry& registry, const FPSCamera& camera, const Window& window) const
{
  gpu::RenderContext context;
  context.pipeline = pipeline.getPipeline();
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
  executeRenderLoop(context, registry, camera);
  gpu::endRenderPass(context);

  return SDL_APP_CONTINUE;
}

} // namespace flb
