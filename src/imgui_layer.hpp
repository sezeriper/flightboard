#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

struct ImGuiContext;
struct ImFont;

namespace flb
{
struct ViewportRect
{
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
  bool valid = false;

  float aspect() const { return height > 0.0f ? width / height : 1.0f; }
};

class ImGuiLayer
{
public:
  SDL_AppResult init(SDL_Window* window, SDL_GPUDevice* device);
  void cleanup(SDL_GPUDevice* device);

  void handleEvent(const SDL_Event& event);
  void beginFrame();
  ViewportRect beginMainView();
  void endMainView(SDL_GPUTexture* sceneTexture);
  void drawSidePanel();
  void endFrame();

  void prepareDrawData(SDL_GPUCommandBuffer* commandBuffer) const;
  void renderDrawData(SDL_GPUCommandBuffer* commandBuffer, SDL_GPURenderPass* renderPass) const;

  bool wantsMouseCapture() const;
  bool wantsKeyboardCapture() const;
  bool isMainViewHovered() const { return mainViewHovered; }
  bool isMainViewFocused() const { return mainViewFocused; }

  const ViewportRect& getMainViewRect() const { return mainViewRect; }
  bool hasDrawData() const;

private:
  void setupFonts();
  void setupTheme();
  void setupDefaultDockLayout(unsigned int dockspaceId, float width, float height);
  void drawDockspace();

  ImGuiContext* context = nullptr;
  ImFont* defaultFont = nullptr;
  ImFont* boldFont = nullptr;
  ViewportRect mainViewRect;
  float mainViewLogicalWidth = 0.0f;
  float mainViewLogicalHeight = 0.0f;
  bool mainViewHovered = false;
  bool mainViewFocused = false;
  bool platformBackendInitialized = false;
  bool rendererBackendInitialized = false;
  bool defaultLayoutBuilt = false;
};
} // namespace flb
