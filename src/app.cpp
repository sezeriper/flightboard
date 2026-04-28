#include "app.hpp"
#include "components.hpp"
#include "math.hpp"
#include "obj_loader.hpp"

#include <glm/ext/vector_common.hpp>

#include <cstddef>
#include <filesystem>
#include <span>

using namespace flb;

namespace
{
Model loadModel(
  MeshManager& meshManager,
  TextureManager& textureManager,
  gpu::Allocator& allocator,
  const std::filesystem::path& objPath,
  const std::filesystem::path& texturePath,
  int textureWidth,
  int textureHeight,
  const char* modelName)
{
  const auto [vertexData, indexData] = loadOBJ(objPath);
  const MeshHandle meshHandle = meshManager.allocate(vertexData, indexData);
  const Mesh mesh = meshManager.get(meshHandle);
  if (!meshHandle.isValid() || mesh.vertexBuffer.buffer == nullptr || mesh.indexBuffer.buffer == nullptr)
  {
    SDL_Log("Failed to create %s mesh", modelName);
    return {};
  }

  auto imageFile = loadFileBinary(texturePath);
  const TextureHandle textureHandle = textureManager.allocate(textureWidth, textureHeight);
  const auto texture = textureManager.get(textureHandle);
  if (imageFile.empty() || !textureHandle.isValid() || texture.texture == nullptr)
  {
    SDL_Log("Failed to create %s texture", modelName);
    meshManager.release(meshHandle);
    textureManager.release(textureHandle);
    return {};
  }

  const std::span<std::byte> textureMemory = allocator.allocateTexture(texture);
  if (textureMemory.empty())
  {
    SDL_Log("Failed to allocate %s texture upload memory", modelName);
    meshManager.release(meshHandle);
    textureManager.release(textureHandle);
    return {};
  }

  loadPNG(imageFile, textureMemory);

  return {&meshManager, &textureManager, meshHandle, textureHandle};
}

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
    if (!registry.any_of<component::MeshHandle, component::Model>(entity))
    {
      allocator.releaseBuffer(vertexBufferView.get<component::VertexBuffer>(entity).value);
    }
  }

  auto indexBufferView = registry.view<component::IndexBuffer>();
  for (auto entity : indexBufferView)
  {
    if (!registry.any_of<component::MeshHandle, component::Model>(entity))
    {
      allocator.releaseBuffer(indexBufferView.get<component::IndexBuffer>(entity).value);
    }
  }

  auto textureView = registry.view<component::Texture>();
  for (auto entity : textureView)
  {
    if (!registry.any_of<component::TextureHandle, component::Model>(entity))
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
    // camera.position = geoToECEF(startCoords, 1'000'000.0);
    camera.position = geoToECEF(startCoords);
    camera.up = getSurfaceNormal(startCoords);
    camera.speed = 3000.0;

    textureManager.init(&allocator);
    meshManager.init(&allocator);
    tileManager.init(&registry, &allocator, &textureManager);

    const Model vehicleModel = loadModel(
      meshManager,
      textureManager,
      allocator,
      "content/models/floatplane/floatplane.obj",
      "content/models/floatplane/textures/floatplane_Albedo.png",
      4096,
      4096,
      "vehicle");
    if (!vehicleModel.isValid())
    {
      return SDL_APP_FAILURE;
    }

    const Model tb2Model = loadModel(
      meshManager,
      textureManager,
      allocator,
      "content/models/tb2/tb2.obj",
      "content/models/tb2/Image_0.png",
      1024,
      1024,
      "tb2");
    if (!tb2Model.isValid())
    {
      return SDL_APP_FAILURE;
    }

    auto vehicle = registry.create();
    registry.emplace<component::Position>(vehicle, camera.position);
    registry.emplace<component::Transform>(vehicle, getSurfaceAlignedTransform(startCoords));
    registry.emplace<component::Model>(vehicle, vehicleModel);

    GeoCoords tb2Coords{39.811224, 30.528496};
    auto tb2 = registry.create();
    registry.emplace<component::Position>(tb2, geoToECEF(tb2Coords));
    registry.emplace<component::Transform>(tb2, getSurfaceAlignedTransform(tb2Coords));
    registry.emplace<component::Model>(tb2, tb2Model);

    if (flightBoundary.init(registry, meshManager) != SDL_APP_CONTINUE)
    {
      return SDL_APP_FAILURE;
    }

    if (noFlyZones.init(registry, meshManager) != SDL_APP_CONTINUE)
    {
      return SDL_APP_FAILURE;
    }

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
  noFlyZones.clear(registry);
  flightBoundary.clear(registry);
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
