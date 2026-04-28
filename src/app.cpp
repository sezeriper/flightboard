#include "app.hpp"
#include "math.hpp"
#include "obj_loader.hpp"

#include <glm/ext/vector_common.hpp>

using namespace flb;

namespace
{
void releaseRegistryGpuResources(
  entt::registry& registry, gpu::Allocator& allocator, MeshManager& meshManager, TextureManager& textureManager)
{
  auto meshHandleView = registry.view<component::MeshHandle>();
  for (auto entity : meshHandleView)
  {
    meshManager.release(meshHandleView.get<component::MeshHandle>(entity).value);
  }

  auto textureHandleView = registry.view<component::TextureHandle>();
  for (auto entity : textureHandleView)
  {
    textureManager.release(textureHandleView.get<component::TextureHandle>(entity).value);
  }

  auto vertexBufferView = registry.view<component::VertexBuffer>();
  for (auto entity : vertexBufferView)
  {
    if (!registry.all_of<component::MeshHandle>(entity))
    {
      allocator.releaseBuffer(vertexBufferView.get<component::VertexBuffer>(entity).value);
    }
  }

  auto indexBufferView = registry.view<component::IndexBuffer>();
  for (auto entity : indexBufferView)
  {
    if (!registry.all_of<component::MeshHandle>(entity))
    {
      allocator.releaseBuffer(indexBufferView.get<component::IndexBuffer>(entity).value);
    }
  }

  auto textureView = registry.view<component::Texture>();
  for (auto entity : textureView)
  {
    if (!registry.all_of<component::TextureHandle>(entity))
    {
      allocator.releaseTexture(textureView.get<component::Texture>(entity).value);
    }
  }
}
} // namespace

SDL_AppResult App::init()
{
  {
    Timer timer("Initialization");

    if (window.init() != SDL_APP_CONTINUE)
    {
      return SDL_APP_FAILURE;
    }

    if (renderer.init(window.getPtr()) != SDL_APP_CONTINUE)
    {
      return SDL_APP_FAILURE;
    }

    if (imGuiLayer.init(window.getPtr(), renderer.getDevice().getPtr()) != SDL_APP_CONTINUE)
    {
      return SDL_APP_FAILURE;
    }

    allocator.init(renderer.getDevice().getPtr());

    GeoCoords startCoords{39.811124, 30.528396};
    camera.position = geoToECEF(startCoords, 1'000'000.0);
    // camera.position = geoToECEF(startCoords);
    camera.up = getSurfaceNormal(startCoords);
    camera.speed = 3000.0;

    textureManager.init(&allocator);
    meshManager.init(&allocator);
    tileManager.init(&registry, &allocator, &textureManager);

    const auto [vertexData, indexData] = loadOBJ("content/models/floatplane/floatplane.obj");
    const MeshHandle vehicleMeshHandle = meshManager.allocate(vertexData, indexData);
    const Mesh vehicleMesh = meshManager.get(vehicleMeshHandle);
    if (
      !vehicleMeshHandle.isValid() || vehicleMesh.vertexBuffer.buffer == nullptr ||
      vehicleMesh.indexBuffer.buffer == nullptr)
    {
      SDL_Log("Failed to create vehicle mesh");
      return SDL_APP_FAILURE;
    }

    auto imageFile = loadFileBinary("content/models/floatplane/textures/floatplane_Albedo.png");
    const TextureHandle vehicleTextureHandle = textureManager.allocate(4096, 4096);
    const auto vehicleTexture = textureManager.get(vehicleTextureHandle);
    if (imageFile.empty() || !vehicleTextureHandle.isValid() || vehicleTexture.texture == nullptr)
    {
      SDL_Log("Failed to create vehicle texture");
      meshManager.release(vehicleMeshHandle);
      textureManager.release(vehicleTextureHandle);
      return SDL_APP_FAILURE;
    }

    const std::span<std::byte> textureMemory = allocator.allocateTexture(vehicleTexture);
    if (textureMemory.empty())
    {
      SDL_Log("Failed to allocate vehicle texture upload memory");
      meshManager.release(vehicleMeshHandle);
      textureManager.release(vehicleTextureHandle);
      return SDL_APP_FAILURE;
    }

    loadPNG(imageFile, textureMemory);

    auto vehicle = registry.create();
    registry.emplace<component::Position>(vehicle, camera.position);
    registry.emplace<component::VertexBuffer>(vehicle, vehicleMesh.vertexBuffer.buffer);
    registry.emplace<component::IndexBuffer>(vehicle, vehicleMesh.indexBuffer.buffer);
    registry.emplace<component::IndexCount>(vehicle, vehicleMesh.indexCount);
    registry.emplace<component::Texture>(vehicle, vehicleTexture.texture);
    registry.emplace<component::MeshHandle>(vehicle, vehicleMeshHandle);
    registry.emplace<component::TextureHandle>(vehicle, vehicleTextureHandle);

    renderer.initDebugSphere(allocator);
    renderer.initTileIndexBuffer(allocator);

    ros.init();
  }

  return SDL_APP_CONTINUE;
}

void App::cleanup()
{
  imGuiLayer.cleanup(renderer.getDevice().getPtr());
  ros.cleanup();
  tileManager.cleanup();
  releaseRegistryGpuResources(registry, allocator, meshManager, textureManager);
  registry.clear();
  allocator.cleanup();
  renderer.cleanup(window.getPtr());
  window.cleanup();
}

SDL_AppResult App::handleEvent(SDL_Event* event)
{
  imGuiLayer.handleEvent(*event);

  if (event->type == SDL_EVENT_QUIT || event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
  {
    return SDL_APP_SUCCESS;
  }

  if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
  {
    auto width = event->window.data1;
    auto height = event->window.data2;
    camera.aspect = static_cast<float>(width) / static_cast<float>(height);
    return SDL_APP_CONTINUE;
  }

  if (event->type == SDL_EVENT_MOUSE_MOTION)
  {
    if (cameraMouseLook)
    {
      const auto dtx = event->motion.xrel;
      const auto dty = event->motion.yrel;
      camera.updateMouse(dtx, dty);
    }
    return SDL_APP_CONTINUE;
  }

  if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_RIGHT)
  {
    if (imGuiLayer.isMainViewHovered() || !imGuiLayer.wantsMouseCapture())
    {
      cameraMouseLook = true;
      SDL_SetWindowRelativeMouseMode(window.getPtr(), true);
    }
    return SDL_APP_CONTINUE;
  }

  if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && event->button.button == SDL_BUTTON_RIGHT)
  {
    cameraMouseLook = false;
    SDL_SetWindowRelativeMouseMode(window.getPtr(), false);
    return SDL_APP_CONTINUE;
  }

  if (event->type == SDL_EVENT_MOUSE_WHEEL)
  {
    if (imGuiLayer.wantsMouseCapture() && !imGuiLayer.isMainViewHovered())
    {
      return SDL_APP_CONTINUE;
    }

    const auto scrollAmount = glm::abs(event->wheel.y);
    const auto direction = event->wheel.y > 0.0f ? 1.0f : -1.0f;

    if (direction > 0.0f)
      camera.speed *= scrollAmount * 5.0f;
    else
      camera.speed /= scrollAmount * 5.0f;

    camera.speed = glm::max(camera.speed, 10.0);
    return SDL_APP_CONTINUE;
  }

  return SDL_APP_CONTINUE;
}

SDL_AppResult App::update(float dt)
{
  ros.update();
  glm::dvec3 coords = ros.getVehicleCoords();

  const bool* keyStates = SDL_GetKeyboardState(NULL);
  if (cameraMouseLook || imGuiLayer.isMainViewFocused() || !imGuiLayer.wantsKeyboardCapture())
  {
    camera.updateKeyboard(dt, keyStates);
  }

  tileManager.update(camera, now());

  return SDL_APP_CONTINUE;
}

SDL_AppResult App::draw()
{
  imGuiLayer.beginFrame();

  const ViewportRect mainViewRect = imGuiLayer.beginMainView();
  if (mainViewRect.valid)
  {
    if (renderer.ensureSceneTarget(mainViewRect) != SDL_APP_CONTINUE)
    {
      return SDL_APP_FAILURE;
    }
    camera.aspect = mainViewRect.aspect();
  }

  imGuiLayer.endMainView(renderer.getSceneTexture());
  imGuiLayer.drawSidePanel();
  imGuiLayer.endFrame();

  return renderer.draw(registry, camera, window, &imGuiLayer);
}
