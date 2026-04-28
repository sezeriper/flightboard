#include "imgui_layer.hpp"

#include "IconsMaterialDesign.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <imgui_internal.h>

#include <cstdint>

using namespace flb;

namespace
{
constexpr const char* DOCKSPACE_WINDOW_NAME = "Flightboard Dockspace";
constexpr const char* MAIN_VIEW_WINDOW_NAME = "Main View";
constexpr const char* SIDE_PANEL_WINDOW_NAME = "Side Panel";
constexpr const char* DEFAULT_FONT_PATH = "content/fonts/JetBrainsMonoNL-Regular.ttf";
constexpr const char* BOLD_FONT_PATH = "content/fonts/JetBrainsMonoNL-Bold.ttf";
constexpr const char* ICON_FONT_PATH = "content/fonts/" FONT_ICON_FILE_NAME_MD;
constexpr float UI_FONT_SIZE = 22.0f;
constexpr float ICON_FONT_SIZE = 22.0f;
constexpr float ICON_GLYPH_OFFSET_Y = ICON_FONT_SIZE * 0.2f;

bool isDrawDataVisible(const ImDrawData* drawData)
{
  return drawData != nullptr && drawData->DisplaySize.x > 0.0f && drawData->DisplaySize.y > 0.0f;
}

bool mergeMaterialIcons(ImFontAtlas* fontAtlas)
{
  static constexpr ImWchar iconRanges[] = {ICON_MIN_MD, ICON_MAX_16_MD, 0};

  ImFontConfig iconConfig;
  iconConfig.MergeMode = true;
  iconConfig.PixelSnapH = true;
  iconConfig.GlyphMinAdvanceX = ICON_FONT_SIZE;
  iconConfig.GlyphOffset.y = ICON_GLYPH_OFFSET_Y;

  return fontAtlas->AddFontFromFileTTF(ICON_FONT_PATH, ICON_FONT_SIZE, &iconConfig, iconRanges) != nullptr;
}

ImVec4 color(float r, float g, float b, float a = 1.0f) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a); }
} // namespace

void ImGuiLayer::setupFonts()
{
  ImGuiIO& io = ImGui::GetIO();

  defaultFont = io.Fonts->AddFontFromFileTTF(DEFAULT_FONT_PATH, UI_FONT_SIZE);
  if (defaultFont == nullptr)
  {
    SDL_Log("Failed to load ImGui font '%s'; falling back to built-in font", DEFAULT_FONT_PATH);
    defaultFont = io.Fonts->AddFontDefault();
  }

  if (!mergeMaterialIcons(io.Fonts))
  {
    SDL_Log("Failed to merge ImGui icon font '%s'", ICON_FONT_PATH);
  }

  boldFont = io.Fonts->AddFontFromFileTTF(BOLD_FONT_PATH, UI_FONT_SIZE);
  if (boldFont == nullptr)
  {
    SDL_Log("Failed to load ImGui font '%s'; bold text will use the default font", BOLD_FONT_PATH);
    boldFont = defaultFont;
  }
  else if (!mergeMaterialIcons(io.Fonts))
  {
    SDL_Log("Failed to merge ImGui icon font '%s' into bold font", ICON_FONT_PATH);
  }

  io.FontDefault = defaultFont;
}

void ImGuiLayer::setupTheme()
{
  ImGuiStyle& style = ImGui::GetStyle();
  style.FontSizeBase = UI_FONT_SIZE;
  style.Alpha = 1.0f;
  style.DisabledAlpha = 0.45f;
  style.WindowPadding = ImVec2(12.0f, 10.0f);
  style.WindowRounding = 6.0f;
  style.WindowBorderSize = 1.0f;
  style.WindowMinSize = ImVec2(160.0f, 120.0f);
  style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
  style.ChildRounding = 6.0f;
  style.ChildBorderSize = 1.0f;
  style.PopupRounding = 6.0f;
  style.PopupBorderSize = 1.0f;
  style.FramePadding = ImVec2(10.0f, 6.0f);
  style.FrameRounding = 5.0f;
  style.FrameBorderSize = 0.0f;
  style.ItemSpacing = ImVec2(10.0f, 8.0f);
  style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
  style.CellPadding = ImVec2(8.0f, 6.0f);
  style.IndentSpacing = 24.0f;
  style.ScrollbarSize = 14.0f;
  style.ScrollbarRounding = 6.0f;
  style.GrabMinSize = 12.0f;
  style.GrabRounding = 5.0f;
  style.ImageRounding = 4.0f;
  style.TabRounding = 5.0f;
  style.TabBorderSize = 0.0f;
  style.TabBarBorderSize = 1.0f;
  style.TabBarOverlineSize = 2.0f;
  style.DockingSeparatorSize = 2.0f;

  ImVec4* colors = style.Colors;
  colors[ImGuiCol_Text] = color(232.0f, 234.0f, 238.0f);
  colors[ImGuiCol_TextDisabled] = color(118.0f, 124.0f, 133.0f);
  colors[ImGuiCol_WindowBg] = color(16.0f, 17.0f, 20.0f);
  colors[ImGuiCol_ChildBg] = color(20.0f, 22.0f, 26.0f);
  colors[ImGuiCol_PopupBg] = color(22.0f, 24.0f, 29.0f, 0.98f);
  colors[ImGuiCol_Border] = color(48.0f, 52.0f, 60.0f);
  colors[ImGuiCol_BorderShadow] = color(0.0f, 0.0f, 0.0f, 0.0f);
  colors[ImGuiCol_FrameBg] = color(30.0f, 33.0f, 39.0f);
  colors[ImGuiCol_FrameBgHovered] = color(55.0f, 39.0f, 44.0f);
  colors[ImGuiCol_FrameBgActive] = color(77.0f, 42.0f, 48.0f);
  colors[ImGuiCol_TitleBg] = color(14.0f, 15.0f, 18.0f);
  colors[ImGuiCol_TitleBgActive] = color(22.0f, 24.0f, 29.0f);
  colors[ImGuiCol_TitleBgCollapsed] = color(14.0f, 15.0f, 18.0f, 0.85f);
  colors[ImGuiCol_MenuBarBg] = color(18.0f, 20.0f, 24.0f);
  colors[ImGuiCol_ScrollbarBg] = color(15.0f, 16.0f, 19.0f);
  colors[ImGuiCol_ScrollbarGrab] = color(55.0f, 59.0f, 68.0f);
  colors[ImGuiCol_ScrollbarGrabHovered] = color(78.0f, 62.0f, 68.0f);
  colors[ImGuiCol_ScrollbarGrabActive] = color(178.0f, 47.0f, 60.0f);
  colors[ImGuiCol_CheckMark] = color(231.0f, 76.0f, 91.0f);
  colors[ImGuiCol_SliderGrab] = color(192.0f, 54.0f, 68.0f);
  colors[ImGuiCol_SliderGrabActive] = color(230.0f, 82.0f, 96.0f);
  colors[ImGuiCol_Button] = color(42.0f, 45.0f, 53.0f);
  colors[ImGuiCol_ButtonHovered] = color(95.0f, 49.0f, 58.0f);
  colors[ImGuiCol_ButtonActive] = color(170.0f, 44.0f, 58.0f);
  colors[ImGuiCol_Header] = color(44.0f, 31.0f, 36.0f);
  colors[ImGuiCol_HeaderHovered] = color(88.0f, 44.0f, 53.0f);
  colors[ImGuiCol_HeaderActive] = color(158.0f, 43.0f, 56.0f);
  colors[ImGuiCol_Separator] = color(53.0f, 57.0f, 66.0f);
  colors[ImGuiCol_SeparatorHovered] = color(146.0f, 45.0f, 57.0f);
  colors[ImGuiCol_SeparatorActive] = color(218.0f, 65.0f, 79.0f);
  colors[ImGuiCol_ResizeGrip] = color(192.0f, 54.0f, 68.0f, 0.22f);
  colors[ImGuiCol_ResizeGripHovered] = color(218.0f, 65.0f, 79.0f, 0.55f);
  colors[ImGuiCol_ResizeGripActive] = color(236.0f, 83.0f, 96.0f, 0.85f);
  colors[ImGuiCol_InputTextCursor] = color(239.0f, 88.0f, 101.0f);
  colors[ImGuiCol_Tab] = color(24.0f, 26.0f, 31.0f);
  colors[ImGuiCol_TabHovered] = color(79.0f, 39.0f, 47.0f);
  colors[ImGuiCol_TabSelected] = color(34.0f, 37.0f, 44.0f);
  colors[ImGuiCol_TabSelectedOverline] = color(220.0f, 62.0f, 76.0f);
  colors[ImGuiCol_TabDimmed] = color(18.0f, 20.0f, 24.0f);
  colors[ImGuiCol_TabDimmedSelected] = color(28.0f, 30.0f, 36.0f);
  colors[ImGuiCol_TabDimmedSelectedOverline] = color(136.0f, 43.0f, 54.0f);
  colors[ImGuiCol_DockingPreview] = color(220.0f, 62.0f, 76.0f, 0.45f);
  colors[ImGuiCol_DockingEmptyBg] = color(12.0f, 13.0f, 16.0f);
  colors[ImGuiCol_PlotLines] = color(205.0f, 210.0f, 219.0f);
  colors[ImGuiCol_PlotLinesHovered] = color(237.0f, 84.0f, 98.0f);
  colors[ImGuiCol_PlotHistogram] = color(187.0f, 52.0f, 65.0f);
  colors[ImGuiCol_PlotHistogramHovered] = color(236.0f, 83.0f, 96.0f);
  colors[ImGuiCol_TableHeaderBg] = color(28.0f, 31.0f, 37.0f);
  colors[ImGuiCol_TableBorderStrong] = color(58.0f, 63.0f, 72.0f);
  colors[ImGuiCol_TableBorderLight] = color(38.0f, 42.0f, 49.0f);
  colors[ImGuiCol_TableRowBg] = color(0.0f, 0.0f, 0.0f, 0.0f);
  colors[ImGuiCol_TableRowBgAlt] = color(255.0f, 255.0f, 255.0f, 0.035f);
  colors[ImGuiCol_TextLink] = color(237.0f, 84.0f, 98.0f);
  colors[ImGuiCol_TextSelectedBg] = color(180.0f, 47.0f, 60.0f, 0.42f);
  colors[ImGuiCol_TreeLines] = color(80.0f, 86.0f, 96.0f);
  colors[ImGuiCol_DragDropTarget] = color(236.0f, 83.0f, 96.0f, 0.9f);
  colors[ImGuiCol_DragDropTargetBg] = color(180.0f, 47.0f, 60.0f, 0.18f);
  colors[ImGuiCol_UnsavedMarker] = color(236.0f, 83.0f, 96.0f);
  colors[ImGuiCol_NavCursor] = color(237.0f, 84.0f, 98.0f);
  colors[ImGuiCol_NavWindowingHighlight] = color(255.0f, 255.0f, 255.0f, 0.7f);
  colors[ImGuiCol_NavWindowingDimBg] = color(0.0f, 0.0f, 0.0f, 0.45f);
  colors[ImGuiCol_ModalWindowDimBg] = color(0.0f, 0.0f, 0.0f, 0.55f);
}

SDL_AppResult ImGuiLayer::init(SDL_Window* window, SDL_GPUDevice* device)
{
  IMGUI_CHECKVERSION();
  context = ImGui::CreateContext();
  ImGui::SetCurrentContext(context);

  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  setupFonts();
  setupTheme();

  if (!ImGui_ImplSDL3_InitForSDLGPU(window))
  {
    SDL_Log("ImGui_ImplSDL3_InitForSDLGPU failed");
    cleanup(device);
    return SDL_APP_FAILURE;
  }
  platformBackendInitialized = true;

  ImGui_ImplSDLGPU3_InitInfo initInfo{};
  initInfo.Device = device;
  initInfo.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(device, window);
  initInfo.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
  initInfo.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
  initInfo.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;

  if (!ImGui_ImplSDLGPU3_Init(&initInfo))
  {
    SDL_Log("ImGui_ImplSDLGPU3_Init failed");
    cleanup(device);
    return SDL_APP_FAILURE;
  }
  rendererBackendInitialized = true;

  return SDL_APP_CONTINUE;
}

void ImGuiLayer::cleanup(SDL_GPUDevice* device)
{
  if (context == nullptr)
  {
    return;
  }

  ImGui::SetCurrentContext(context);

  if (device != nullptr)
  {
    SDL_WaitForGPUIdle(device);
  }

  if (rendererBackendInitialized)
  {
    ImGui_ImplSDLGPU3_Shutdown();
    rendererBackendInitialized = false;
  }

  if (platformBackendInitialized)
  {
    ImGui_ImplSDL3_Shutdown();
    platformBackendInitialized = false;
  }

  ImGui::DestroyContext(context);
  context = nullptr;
  defaultFont = nullptr;
  boldFont = nullptr;
  mainViewRect = {};
  mainViewLogicalWidth = 0.0f;
  mainViewLogicalHeight = 0.0f;
  mainViewHovered = false;
  mainViewFocused = false;
  defaultLayoutBuilt = false;
}

void ImGuiLayer::handleEvent(const SDL_Event& event)
{
  if (context == nullptr)
  {
    return;
  }

  ImGui::SetCurrentContext(context);
  ImGui_ImplSDL3_ProcessEvent(&event);
}

void ImGuiLayer::beginFrame()
{
  ImGui::SetCurrentContext(context);

  mainViewRect = {};
  ImGui_ImplSDLGPU3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
}

void ImGuiLayer::endFrame() { ImGui::Render(); }

void ImGuiLayer::prepareDrawData(SDL_GPUCommandBuffer* commandBuffer) const
{
  ImGui::SetCurrentContext(context);

  ImDrawData* drawData = ImGui::GetDrawData();
  if (isDrawDataVisible(drawData))
  {
    ImGui_ImplSDLGPU3_PrepareDrawData(drawData, commandBuffer);
  }
}

void ImGuiLayer::renderDrawData(SDL_GPUCommandBuffer* commandBuffer, SDL_GPURenderPass* renderPass) const
{
  ImGui::SetCurrentContext(context);

  ImDrawData* drawData = ImGui::GetDrawData();
  if (isDrawDataVisible(drawData))
  {
    ImGui_ImplSDLGPU3_RenderDrawData(drawData, commandBuffer, renderPass);
  }
}

bool ImGuiLayer::wantsMouseCapture() const
{
  if (context == nullptr)
  {
    return false;
  }

  ImGui::SetCurrentContext(context);
  return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiLayer::wantsKeyboardCapture() const
{
  if (context == nullptr)
  {
    return false;
  }

  ImGui::SetCurrentContext(context);
  return ImGui::GetIO().WantCaptureKeyboard;
}

bool ImGuiLayer::hasDrawData() const
{
  if (context == nullptr)
  {
    return false;
  }

  ImGui::SetCurrentContext(context);
  return isDrawDataVisible(ImGui::GetDrawData());
}

void ImGuiLayer::setupDefaultDockLayout(unsigned int dockspaceId, float width, float height)
{
  if (defaultLayoutBuilt)
  {
    return;
  }

  if (ImGui::DockBuilderGetNode(dockspaceId) != nullptr)
  {
    ImGui::DockBuilderRemoveNode(dockspaceId);
  }

  ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspaceId, ImVec2(width, height));

  ImGuiID mainDockId = dockspaceId;
  ImGuiID sideDockId = 0;
  ImGui::DockBuilderSplitNode(mainDockId, ImGuiDir_Right, 0.26f, &sideDockId, &mainDockId);
  ImGui::DockBuilderDockWindow(MAIN_VIEW_WINDOW_NAME, mainDockId);
  ImGui::DockBuilderDockWindow(SIDE_PANEL_WINDOW_NAME, sideDockId);
  ImGui::DockBuilderFinish(dockspaceId);
  defaultLayoutBuilt = true;
}

void ImGuiLayer::drawDockspace()
{
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);
  ImGui::SetNextWindowBgAlpha(0.0f);

  ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
                                 ImGuiWindowFlags_NoBackground;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

  ImGui::Begin(DOCKSPACE_WINDOW_NAME, nullptr, windowFlags);
  ImGui::PopStyleVar(3);

  const ImGuiID dockspaceId = ImGui::GetID("FlightboardDockspace");
  setupDefaultDockLayout(dockspaceId, viewport->WorkSize.x, viewport->WorkSize.y);

  constexpr ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;
  ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspaceFlags);

  ImGui::End();
}

ViewportRect ImGuiLayer::beginMainView()
{
  drawDockspace();

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

  constexpr ImGuiWindowFlags windowFlags =
    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

  ImGui::Begin(MAIN_VIEW_WINDOW_NAME, nullptr, windowFlags);

  mainViewHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
  mainViewFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

  const ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
  const ImVec2 contentAvail = ImGui::GetContentRegionAvail();
  const ImVec2 framebufferScale = ImGui::GetIO().DisplayFramebufferScale;

  mainViewLogicalWidth = contentAvail.x;
  mainViewLogicalHeight = contentAvail.y;
  mainViewRect.x = cursorScreenPos.x * framebufferScale.x;
  mainViewRect.y = cursorScreenPos.y * framebufferScale.y;
  mainViewRect.width = contentAvail.x * framebufferScale.x;
  mainViewRect.height = contentAvail.y * framebufferScale.y;
  mainViewRect.valid = mainViewRect.width > 1.0f && mainViewRect.height > 1.0f;

  return mainViewRect;
}

void ImGuiLayer::endMainView(SDL_GPUTexture* sceneTexture)
{
  if (sceneTexture != nullptr && mainViewRect.valid)
  {
    ImGui::Image(
      ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(sceneTexture))),
      ImVec2(mainViewLogicalWidth, mainViewLogicalHeight));
  }

  ImGui::End();
  ImGui::PopStyleVar();
}

void ImGuiLayer::drawSidePanel()
{
  ImGui::Begin(SIDE_PANEL_WINDOW_NAME, nullptr, ImGuiWindowFlags_NoCollapse);

  ImGui::PushFont(boldFont, 0.0f);
  ImGui::TextUnformatted(ICON_MD_TUNE " Controls");
  ImGui::PopFont();
  ImGui::Separator();
  ImGui::Text("FPS %.1f", ImGui::GetIO().Framerate);

  ImGui::Spacing();
  ImGui::PushFont(boldFont, 0.0f);
  ImGui::TextUnformatted(ICON_MD_INFO " Info");
  ImGui::PopFont();
  ImGui::Separator();
  ImGui::TextUnformatted("No telemetry selected");

  ImGui::End();
}
