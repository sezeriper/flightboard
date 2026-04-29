#include "imgui_layer.hpp"

#include "IconsMaterialDesign.h"
#include "camera.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstddef>
#include <cstdint>

using namespace flb;

namespace
{
constexpr const char* DOCKSPACE_WINDOW_NAME = "Flightboard Dockspace";
constexpr const char* TOP_BAR_WINDOW_NAME = "Flightboard Top Bar";
constexpr const char* MAIN_VIEW_WINDOW_NAME = "Main View";
constexpr const char* SIDE_PANEL_WINDOW_NAME = "Side Panel";
constexpr const char* ACTIONS_WINDOW_NAME = "Actions";
constexpr const char* TELEMETRY_WINDOW_NAME = "Telemetry";
constexpr const char* DEFAULT_FONT_PATH = "content/fonts/JetBrainsMonoNL-Regular.ttf";
constexpr const char* BOLD_FONT_PATH = "content/fonts/JetBrainsMonoNL-Bold.ttf";
constexpr const char* ICON_FONT_PATH = "content/fonts/" FONT_ICON_FILE_NAME_MD;
constexpr float UI_FONT_SIZE_SMALL = 16.0f;
constexpr float UI_FONT_SIZE_MEDIUM = 20.0f;
constexpr float UI_FONT_SIZE_BIG = 28.0f;
constexpr float ICON_GLYPH_OFFSET_Y = UI_FONT_SIZE_MEDIUM * 0.2f;
constexpr float TOP_BAR_PADDING = 10.0f;
constexpr float TOP_BAR_GAP = 10.0f;
constexpr float TOP_BAR_PANEL_ROUNDING = 7.0f;
constexpr float TELEMETRY_TILE_HEIGHT = 88.0f;

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
  iconConfig.GlyphMinAdvanceX = UI_FONT_SIZE_MEDIUM;
  iconConfig.GlyphOffset.y = ICON_GLYPH_OFFSET_Y;

  return fontAtlas->AddFontFromFileTTF(ICON_FONT_PATH, UI_FONT_SIZE_MEDIUM, &iconConfig, iconRanges) != nullptr;
}

ImVec4 color(float r, float g, float b, float a = 1.0f) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a); }

ImU32 colorU32(const ImVec4& value) { return ImGui::ColorConvertFloat4ToU32(value); }

ImVec4 withAlpha(ImVec4 value, float alpha)
{
  value.w = alpha;
  return value;
}

bool beginTopBarPanel(const char* id, const ImVec2& size, const ImVec4& accent, const ImVec4& background)
{
  constexpr ImGuiWindowFlags windowFlags =
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;

  ImGui::PushStyleColor(ImGuiCol_ChildBg, background);
  ImGui::PushStyleColor(ImGuiCol_Border, withAlpha(accent, 0.38f));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, TOP_BAR_PANEL_ROUNDING);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 7.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 3.0f));

  const bool visible =
    ImGui::BeginChild(id, size, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding, windowFlags);

  ImGui::PopStyleVar(4);
  ImGui::PopStyleColor(2);

  if (visible)
  {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 panelSize = ImGui::GetWindowSize();
    const ImVec2 max(min.x + panelSize.x, min.y + panelSize.y);

    drawList->AddRectFilled(
      min, ImVec2(min.x + 4.0f, max.y), colorU32(accent), TOP_BAR_PANEL_ROUNDING, ImDrawFlags_RoundCornersLeft);
    drawList->AddRect(min, max, colorU32(withAlpha(accent, 0.22f)), TOP_BAR_PANEL_ROUNDING);
  }

  return visible;
}

void drawTopBarHeader(ImFont* boldFont, const char* icon, const char* title, const ImVec4& accent)
{
  ImGui::PushFont(boldFont, UI_FONT_SIZE_MEDIUM);
  ImGui::PushStyleColor(ImGuiCol_Text, withAlpha(accent, 0.95f));
  ImGui::TextUnformatted(icon);
  ImGui::SameLine(0.0f, 6.0f);
  ImGui::TextUnformatted(title);
  ImGui::PopStyleColor();
  ImGui::PopFont();
}

void drawStatusPanel(
  const char* id,
  const ImVec2& size,
  const char* icon,
  const char* title,
  const char* primary,
  const char* secondary,
  const ImVec4& accent,
  ImFont* defaultFont,
  ImFont* boldFont)
{
  if (beginTopBarPanel(id, size, accent, color(18.0f, 21.0f, 27.0f, 0.96f)))
  {
    drawTopBarHeader(boldFont, icon, title, accent);

    ImGui::PushFont(boldFont, UI_FONT_SIZE_BIG);
    ImGui::PushStyleColor(ImGuiCol_Text, accent);
    ImGui::TextUnformatted(primary);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    if (secondary != nullptr && secondary[0] != '\0')
    {
      ImGui::PushFont(defaultFont, UI_FONT_SIZE_SMALL);
      ImGui::PushStyleColor(ImGuiCol_Text, color(162.0f, 169.0f, 180.0f));
      ImGui::TextWrapped("%s", secondary);
      ImGui::PopStyleColor();
      ImGui::PopFont();
    }
  }

  ImGui::EndChild();
}

void drawMetricCell(
  const char* label, const char* value, const ImVec4& valueColor, ImFont* defaultFont, ImFont* boldFont)
{
  ImGui::PushFont(defaultFont, UI_FONT_SIZE_SMALL);
  ImGui::PushStyleColor(ImGuiCol_Text, color(133.0f, 141.0f, 153.0f));
  ImGui::TextUnformatted(label);
  ImGui::PopStyleColor();
  ImGui::PopFont();

  ImGui::PushFont(boldFont, UI_FONT_SIZE_MEDIUM);
  ImGui::PushStyleColor(ImGuiCol_Text, valueColor);
  ImGui::TextUnformatted(value);
  ImGui::PopStyleColor();
  ImGui::PopFont();
}

void drawTimeSyncPanel(const ImVec2& size, ImFont* defaultFont, ImFont* boldFont)
{
  const ImVec4 accent = color(92.0f, 204.0f, 220.0f);

  if (beginTopBarPanel("##time-sync", size, accent, color(17.0f, 22.0f, 29.0f, 0.96f)))
  {
    drawTopBarHeader(boldFont, ICON_MD_SYNC, "Time sync", accent);

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.0f, 3.0f));
    if (ImGui::BeginTable(
          "##time-sync-metrics", 3, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
    {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      drawMetricCell("SERVER", "14:32:08Z", color(224.0f, 243.0f, 246.0f), defaultFont, boldFont);
      ImGui::TableNextColumn();
      drawMetricCell("OFFSET", "+12 ms", color(177.0f, 230.0f, 196.0f), defaultFont, boldFont);
      ImGui::TableNextColumn();
      drawMetricCell("LATENCY", "38 ms", color(255.0f, 205.0f, 127.0f), defaultFont, boldFont);
      ImGui::EndTable();
    }
    ImGui::PopStyleVar();
  }

  ImGui::EndChild();
}

void drawModeStrip(ImFont* boldFont, float width, float height, int activeIndex)
{
  struct ModeChip
  {
    const char* label;
    bool manualClass;
  };

  static constexpr std::array<ModeChip, 8> modes = {
    ModeChip{"AUTO", false},
    ModeChip{"MAN", true},
    ModeChip{"SEMI", true},
    ModeChip{"FS", false},
    ModeChip{"LAND", false},
    ModeChip{"T/O", false},
    ModeChip{"MSN", false},
    ModeChip{"RTL", false},
  };

  const float gap = 4.0f;
  const float chipWidth = (width - gap * static_cast<float>(modes.size() - 1)) / static_cast<float>(modes.size());
  if (chipWidth < 18.0f || height < 12.0f)
  {
    return;
  }

  ImDrawList* drawList = ImGui::GetWindowDrawList();
  const ImVec2 start = ImGui::GetCursorScreenPos();
  constexpr float fontSize = UI_FONT_SIZE_SMALL;

  for (std::size_t i = 0; i < modes.size(); ++i)
  {
    const bool active = static_cast<int>(i) == activeIndex;
    const ImVec4 chipAccent = modes[i].manualClass ? color(239.0f, 101.0f, 88.0f) : color(117.0f, 128.0f, 143.0f);
    const ImVec4 chipBg =
      active ? withAlpha(chipAccent, 0.95f) : withAlpha(chipAccent, modes[i].manualClass ? 0.24f : 0.16f);
    const ImVec4 textColor = active ? color(18.0f, 21.0f, 26.0f) : color(218.0f, 223.0f, 231.0f);
    const ImVec2 min(start.x + (chipWidth + gap) * static_cast<float>(i), start.y);
    const ImVec2 max(min.x + chipWidth, min.y + height);

    drawList->AddRectFilled(min, max, colorU32(chipBg), 4.0f);
    drawList->AddRect(min, max, colorU32(withAlpha(chipAccent, active ? 0.0f : 0.35f)), 4.0f);

    const ImVec2 textSize = boldFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, modes[i].label);
    const ImVec2 textPos(
      min.x + std::max(2.0f, (chipWidth - textSize.x) * 0.5f), min.y + std::max(0.0f, (height - textSize.y) * 0.5f));
    drawList->AddText(boldFont, fontSize, textPos, colorU32(textColor), modes[i].label);
  }

  ImGui::Dummy(ImVec2(width, height));
}

void drawManualSwitchBudget(const ImVec4& accent)
{
  ImDrawList* drawList = ImGui::GetWindowDrawList();
  const ImVec2 start = ImGui::GetCursorScreenPos();
  constexpr float segmentWidth = 18.0f;
  constexpr float segmentHeight = 6.0f;
  constexpr float gap = 4.0f;

  for (int i = 0; i < 3; ++i)
  {
    const ImVec2 min(start.x + static_cast<float>(i) * (segmentWidth + gap), start.y);
    const ImVec2 max(min.x + segmentWidth, min.y + segmentHeight);
    const bool used = i < 2;
    drawList->AddRectFilled(min, max, colorU32(used ? accent : color(63.0f, 68.0f, 78.0f)), 3.0f);
  }

  ImGui::Dummy(ImVec2(segmentWidth * 3.0f + gap * 2.0f, segmentHeight));
}

void drawFlightModePanel(const ImVec2& size, ImFont* defaultFont, ImFont* boldFont)
{
  const ImVec4 accent = color(239.0f, 101.0f, 88.0f);

  if (beginTopBarPanel("##flight-mode", size, accent, color(32.0f, 20.0f, 22.0f, 0.98f)))
  {
    drawTopBarHeader(boldFont, ICON_MD_FLIGHT, "Flight mode", accent);

    ImGui::PushFont(boldFont, UI_FONT_SIZE_BIG);
    ImGui::PushStyleColor(ImGuiCol_Text, color(255.0f, 216.0f, 209.0f));
    ImGui::TextUnformatted("MANUAL");
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::PushFont(defaultFont, UI_FONT_SIZE_SMALL);
    ImGui::PushStyleColor(ImGuiCol_Text, color(255.0f, 186.0f, 176.0f));
    ImGui::TextUnformatted("Manual/Semi-auto switches 2/3");
    ImGui::PopStyleColor();
    ImGui::PopFont();

    drawManualSwitchBudget(accent);

    constexpr float stripHeight = UI_FONT_SIZE_SMALL + 2.0f;
    const float availableY = ImGui::GetContentRegionAvail().y;
    if (availableY >= stripHeight)
    {
      drawModeStrip(boldFont, ImGui::GetContentRegionAvail().x, stripHeight, 1);
    }
  }

  ImGui::EndChild();
}

void drawRoundProgressBar(float activeElapsedSeconds, ImFont* boldFont)
{
  constexpr float activeRoundSeconds = 15.0f * 60.0f;
  constexpr float clearUploadSeconds = 10.0f * 60.0f;
  constexpr float totalSeconds = activeRoundSeconds + clearUploadSeconds;
  const float activeSectionFraction = activeRoundSeconds / totalSeconds;
  const float activeElapsedFraction = std::clamp(activeElapsedSeconds / activeRoundSeconds, 0.0f, 1.0f);

  ImDrawList* drawList = ImGui::GetWindowDrawList();
  const float width = ImGui::GetContentRegionAvail().x;
  const ImVec2 min = ImGui::GetCursorScreenPos();
  const ImVec2 max(min.x + width, min.y + 18.0f);
  const float activeSectionWidth = width * activeSectionFraction;
  const ImVec2 activeMax(min.x + activeSectionWidth, max.y);
  const ImVec2 activeFillMax(min.x + activeSectionWidth * activeElapsedFraction, max.y);

  drawList->AddRectFilled(min, max, colorU32(color(48.0f, 53.0f, 62.0f)), 4.0f);
  drawList->AddRectFilled(min, activeMax, colorU32(color(34.0f, 77.0f, 50.0f)), 4.0f, ImDrawFlags_RoundCornersLeft);
  drawList->AddRectFilled(activeMax, max, colorU32(color(65.0f, 55.0f, 37.0f)), 4.0f, ImDrawFlags_RoundCornersRight);
  drawList->AddRectFilled(min, activeFillMax, colorU32(color(126.0f, 223.0f, 149.0f)), 4.0f);
  drawList->AddLine(
    ImVec2(activeMax.x, min.y + 1.0f),
    ImVec2(activeMax.x, max.y - 1.0f),
    colorU32(color(224.0f, 231.0f, 239.0f, 0.45f)),
    1.0f);

  constexpr float labelFontSize = UI_FONT_SIZE_SMALL;
  const char* activeLabel = "15 MIN ROUND";
  const char* clearLabel = "10 MIN CLEAR/UPLOAD";
  const ImVec4 activeLabelColor = color(19.0f, 31.0f, 24.0f);
  const ImVec4 clearLabelColor = color(234.0f, 218.0f, 184.0f);
  const ImVec2 activeTextSize = boldFont->CalcTextSizeA(labelFontSize, FLT_MAX, 0.0f, activeLabel);
  const ImVec2 clearTextSize = boldFont->CalcTextSizeA(labelFontSize, FLT_MAX, 0.0f, clearLabel);

  if (activeSectionWidth > activeTextSize.x + 12.0f)
  {
    drawList->AddText(
      boldFont,
      labelFontSize,
      ImVec2(min.x + std::max(6.0f, (activeSectionWidth - activeTextSize.x) * 0.5f), min.y + 2.0f),
      colorU32(activeLabelColor),
      activeLabel);
  }

  const float clearSectionWidth = width - activeSectionWidth;
  if (clearSectionWidth > clearTextSize.x + 12.0f)
  {
    drawList->AddText(
      boldFont,
      labelFontSize,
      ImVec2(activeMax.x + std::max(6.0f, (clearSectionWidth - clearTextSize.x) * 0.5f), min.y + 2.0f),
      colorU32(clearLabelColor),
      clearLabel);
  }

  ImGui::Dummy(ImVec2(width, 18.0f));
}

void drawRoundTimerPanel(const ImVec2& size, ImFont* defaultFont, ImFont* boldFont)
{
  const ImVec4 accent = color(126.0f, 223.0f, 149.0f);

  if (beginTopBarPanel("##round-timer", size, accent, color(17.0f, 24.0f, 22.0f, 0.96f)))
  {
    drawTopBarHeader(boldFont, ICON_MD_TIMER, "Round timer", accent);

    ImGui::PushFont(boldFont, UI_FONT_SIZE_BIG);
    ImGui::PushStyleColor(ImGuiCol_Text, color(218.0f, 251.0f, 226.0f));
    ImGui::TextUnformatted("12:41");
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::SameLine(0.0f, 8.0f);
    ImGui::PushFont(defaultFont, UI_FONT_SIZE_SMALL);
    ImGui::PushStyleColor(ImGuiCol_Text, color(146.0f, 190.0f, 158.0f));
    ImGui::TextUnformatted("ACTIVE / 15:00");
    ImGui::PopStyleColor();
    ImGui::PopFont();

    drawRoundProgressBar(12.0f * 60.0f + 41.0f, boldFont);
  }

  ImGui::EndChild();
}

void drawAlertPanel(const ImVec2& size, ImFont* defaultFont, ImFont* boldFont)
{
  const ImVec4 accent = color(255.0f, 72.0f, 89.0f);

  if (beginTopBarPanel("##active-alert", size, accent, color(32.0f, 18.0f, 22.0f, 0.98f)))
  {
    drawTopBarHeader(boldFont, ICON_MD_WARNING, "Active alert", accent);

    ImGui::PushFont(boldFont, UI_FONT_SIZE_BIG);
    ImGui::PushStyleColor(ImGuiCol_Text, color(255.0f, 220.0f, 224.0f));
    ImGui::TextWrapped("%s", "TELEMETRY LOST");
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::PushFont(defaultFont, UI_FONT_SIZE_SMALL);
    ImGui::PushStyleColor(ImGuiCol_Text, color(255.0f, 166.0f, 175.0f));
    ImGui::TextUnformatted("Switch to manual mode");
    ImGui::PopStyleColor();
    ImGui::PopFont();
  }

  ImGui::EndChild();
}

void drawTelemetrySectionHeader(ImFont* boldFont, const char* icon, const char* label, const ImVec4& accent)
{
  ImGui::Spacing();
  ImGui::PushFont(boldFont, UI_FONT_SIZE_MEDIUM);
  ImGui::PushStyleColor(ImGuiCol_Text, withAlpha(accent, 0.95f));
  ImGui::TextUnformatted(icon);
  ImGui::SameLine(0.0f, 7.0f);
  ImGui::TextUnformatted(label);
  ImGui::PopStyleColor();
  ImGui::PopFont();
}

void drawTelemetryIcon(
  ImDrawList* drawList, ImFont* boldFont, const char* icon, const ImVec2& center, const ImVec4& accent)
{
  constexpr float iconRadius = 17.0f;
  constexpr float iconFontSize = UI_FONT_SIZE_MEDIUM;
  const ImVec2 iconSize = boldFont->CalcTextSizeA(iconFontSize, FLT_MAX, 0.0f, icon);
  const ImVec2 iconPos(center.x - iconSize.x * 0.5f, center.y - iconSize.y * 0.5f);

  drawList->AddCircleFilled(center, iconRadius, colorU32(withAlpha(accent, 0.16f)), 24);
  drawList->AddCircle(center, iconRadius, colorU32(withAlpha(accent, 0.32f)), 24, 1.0f);
  drawList->AddText(boldFont, iconFontSize, iconPos, colorU32(withAlpha(accent, 0.95f)), icon);
}

void drawTelemetryTile(
  const char* id,
  const char* icon,
  const char* label,
  const char* value,
  const ImVec4& accent,
  ImFont* defaultFont,
  ImFont* boldFont)
{
  constexpr ImGuiWindowFlags windowFlags =
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;

  ImGui::PushStyleColor(ImGuiCol_ChildBg, color(21.0f, 24.0f, 30.0f, 0.96f));
  ImGui::PushStyleColor(ImGuiCol_Border, withAlpha(accent, 0.24f));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 9.0f));
  const bool visible = ImGui::BeginChild(id, ImVec2(0.0f, TELEMETRY_TILE_HEIGHT), ImGuiChildFlags_Borders, windowFlags);
  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(2);

  if (visible)
  {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 contentMin = ImGui::GetCursorScreenPos();
    const ImVec2 contentAvail = ImGui::GetContentRegionAvail();
    const ImVec2 iconCenter(contentMin.x + 17.0f, contentMin.y + contentAvail.y * 0.5f);

    drawTelemetryIcon(drawList, boldFont, icon, iconCenter, accent);

    ImGui::SetCursorScreenPos(ImVec2(contentMin.x + 40.0f, contentMin.y - 1.0f));
    ImGui::PushFont(defaultFont, UI_FONT_SIZE_SMALL);
    ImGui::PushStyleColor(ImGuiCol_Text, color(135.0f, 144.0f, 156.0f));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::SetCursorScreenPos(ImVec2(contentMin.x + 40.0f, contentMin.y + 28.0f));
    ImGui::PushFont(boldFont, UI_FONT_SIZE_BIG);
    ImGui::PushStyleColor(ImGuiCol_Text, color(232.0f, 237.0f, 244.0f));
    ImGui::TextUnformatted(value);
    ImGui::PopStyleColor();
    ImGui::PopFont();
  }

  ImGui::EndChild();
}

void drawTelemetryBatteryTile(
  const char* id,
  const char* icon,
  const char* label,
  const char* value,
  float chargeFraction,
  const ImVec4& accent,
  ImFont* defaultFont,
  ImFont* boldFont)
{
  constexpr ImGuiWindowFlags windowFlags =
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings;

  ImGui::PushStyleColor(ImGuiCol_ChildBg, color(21.0f, 24.0f, 30.0f, 0.96f));
  ImGui::PushStyleColor(ImGuiCol_Border, withAlpha(accent, 0.24f));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 7.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 9.0f));
  const bool visible = ImGui::BeginChild(id, ImVec2(0.0f, TELEMETRY_TILE_HEIGHT), ImGuiChildFlags_Borders, windowFlags);
  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor(2);

  if (visible)
  {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 contentMin = ImGui::GetCursorScreenPos();
    const ImVec2 contentAvail = ImGui::GetContentRegionAvail();
    const ImVec2 iconCenter(contentMin.x + 17.0f, contentMin.y + contentAvail.y * 0.5f);

    drawTelemetryIcon(drawList, boldFont, icon, iconCenter, accent);

    ImGui::SetCursorScreenPos(ImVec2(contentMin.x + 40.0f, contentMin.y - 1.0f));
    ImGui::PushFont(defaultFont, UI_FONT_SIZE_SMALL);
    ImGui::PushStyleColor(ImGuiCol_Text, color(135.0f, 144.0f, 156.0f));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::SetCursorScreenPos(ImVec2(contentMin.x + 40.0f, contentMin.y + 28.0f));
    ImGui::PushFont(boldFont, UI_FONT_SIZE_BIG);
    ImGui::PushStyleColor(ImGuiCol_Text, color(232.0f, 237.0f, 244.0f));
    ImGui::TextUnformatted(value);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    const float barWidth = std::max(24.0f, ImGui::GetContentRegionAvail().x - 40.0f);
    const ImVec2 barMin(contentMin.x + 40.0f, contentMin.y + 61.0f);
    const ImVec2 barMax(barMin.x + barWidth, barMin.y + 7.0f);
    const ImVec2 fillMax(barMin.x + barWidth * std::clamp(chargeFraction, 0.0f, 1.0f), barMax.y);
    drawList->AddRectFilled(barMin, barMax, colorU32(color(49.0f, 54.0f, 63.0f)), 3.0f);
    drawList->AddRectFilled(barMin, fillMax, colorU32(accent), 3.0f);
  }

  ImGui::EndChild();
}

void drawTelemetryTwoColumnRow(
  const char* tableId,
  const char* leftId,
  const char* leftIcon,
  const char* leftLabel,
  const char* leftValue,
  const ImVec4& leftAccent,
  const char* rightId,
  const char* rightIcon,
  const char* rightLabel,
  const char* rightValue,
  const ImVec4& rightAccent,
  ImFont* defaultFont,
  ImFont* boldFont)
{
  ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f, 4.0f));
  if (ImGui::BeginTable(tableId, 2, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings))
  {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    drawTelemetryTile(leftId, leftIcon, leftLabel, leftValue, leftAccent, defaultFont, boldFont);
    ImGui::TableNextColumn();
    drawTelemetryTile(rightId, rightIcon, rightLabel, rightValue, rightAccent, defaultFont, boldFont);
    ImGui::EndTable();
  }
  ImGui::PopStyleVar();
}

void drawWindowTitle(ImFont* boldFont, const char* icon, const char* title, const ImVec4& accent)
{
  ImGui::PushFont(boldFont, UI_FONT_SIZE_MEDIUM);
  ImGui::PushStyleColor(ImGuiCol_Text, color(234.0f, 239.0f, 246.0f));
  ImGui::TextUnformatted(icon);
  ImGui::SameLine(0.0f, 7.0f);
  ImGui::TextUnformatted(title);
  ImGui::PopStyleColor();
  ImGui::PopFont();

  ImDrawList* drawList = ImGui::GetWindowDrawList();
  const ImVec2 min = ImGui::GetCursorScreenPos();
  const float width = ImGui::GetContentRegionAvail().x;
  drawList->AddRectFilled(min, ImVec2(min.x + width, min.y + 2.0f), colorU32(withAlpha(accent, 0.68f)), 1.0f);
  ImGui::Dummy(ImVec2(width, 10.0f));
}

void drawActionSectionHeader(ImFont* boldFont, const char* icon, const char* label, const ImVec4& accent)
{
  ImGui::PushFont(boldFont, UI_FONT_SIZE_MEDIUM);
  ImGui::PushStyleColor(ImGuiCol_Text, withAlpha(accent, 0.95f));
  ImGui::TextUnformatted(icon);
  ImGui::SameLine(0.0f, 7.0f);
  ImGui::TextUnformatted(label);
  ImGui::PopStyleColor();
  ImGui::PopFont();
}

void drawActionInputRow(
  const char* inputId,
  const char* buttonId,
  const char* unit,
  float& value,
  float step,
  float minValue,
  ImFont* defaultFont,
  ImFont* boldFont)
{
  const float width = ImGui::GetContentRegionAvail().x;
  const float setButtonWidth = 92.0f;
  const float unitWidth = 46.0f;
  const float inputWidth = std::max(96.0f, width - setButtonWidth - unitWidth - ImGui::GetStyle().ItemSpacing.x * 2.0f);

  ImGui::PushItemWidth(inputWidth);
  ImGui::InputFloat(inputId, &value, step, step * 5.0f, "%.1f");
  value = std::max(minValue, value);
  ImGui::PopItemWidth();

  ImGui::SameLine();
  ImGui::PushFont(defaultFont, UI_FONT_SIZE_SMALL);
  ImGui::TextUnformatted(unit);
  ImGui::PopFont();

  ImGui::SameLine();
  ImGui::PushFont(boldFont, UI_FONT_SIZE_MEDIUM);
  ImGui::Button(buttonId, ImVec2(setButtonWidth, 0.0f));
  ImGui::PopFont();
}

void drawActionButton(
  const char* label, const ImVec4& accent, ImFont* boldFont, const ImVec2& size = ImVec2(0.0f, 0.0f))
{
  ImGui::PushFont(boldFont, UI_FONT_SIZE_MEDIUM);
  ImGui::PushStyleColor(ImGuiCol_Button, withAlpha(accent, 0.24f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, withAlpha(accent, 0.38f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, withAlpha(accent, 0.62f));
  ImGui::PushStyleColor(ImGuiCol_Text, color(234.0f, 239.0f, 246.0f));
  ImGui::Button(label, size);
  ImGui::PopStyleColor(4);
  ImGui::PopFont();
}

float calculateTopBarHeight(float width)
{
  if (width >= 1740.0f)
  {
    return 126.0f;
  }

  if (width >= 1050.0f)
  {
    return 214.0f;
  }

  return 318.0f;
}
} // namespace

void ImGuiLayer::setupFonts()
{
  ImGuiIO& io = ImGui::GetIO();

  defaultFont = io.Fonts->AddFontFromFileTTF(DEFAULT_FONT_PATH, UI_FONT_SIZE_MEDIUM);
  if (defaultFont == nullptr)
  {
    SDL_Log("Failed to load ImGui font '%s'; falling back to built-in font", DEFAULT_FONT_PATH);
    defaultFont = io.Fonts->AddFontDefault();
  }

  if (!mergeMaterialIcons(io.Fonts))
  {
    SDL_Log("Failed to merge ImGui icon font '%s'", ICON_FONT_PATH);
  }

  boldFont = io.Fonts->AddFontFromFileTTF(BOLD_FONT_PATH, UI_FONT_SIZE_MEDIUM);
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
  style.FontSizeBase = UI_FONT_SIZE_MEDIUM;
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
  topBarHeight = 0.0f;
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

void ImGuiLayer::drawTopBar()
{
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  topBarHeight = calculateTopBarHeight(viewport->WorkSize.x);

  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, topBarHeight));
  ImGui::SetNextWindowViewport(viewport->ID);

  constexpr ImGuiWindowFlags windowFlags =
    ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

  ImGui::PushStyleColor(ImGuiCol_WindowBg, color(10.0f, 12.0f, 16.0f, 0.98f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

  ImGui::Begin(TOP_BAR_WINDOW_NAME, nullptr, windowFlags);
  ImGui::PopStyleVar(3);
  ImGui::PopStyleColor();

  ImDrawList* drawList = ImGui::GetWindowDrawList();
  const ImVec2 windowPos = ImGui::GetWindowPos();
  const ImVec2 windowSize = ImGui::GetWindowSize();
  drawList->AddRectFilled(
    windowPos,
    ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
    colorU32(color(10.0f, 12.0f, 16.0f, 0.98f)));
  drawList->AddRectFilled(
    ImVec2(windowPos.x, windowPos.y + windowSize.y - 1.0f),
    ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y),
    colorU32(color(54.0f, 59.0f, 68.0f, 0.9f)));

  const float contentWidth = std::max(0.0f, viewport->WorkSize.x - TOP_BAR_PADDING * 2.0f);

  if (viewport->WorkSize.x >= 1740.0f)
  {
    const float panelHeight = topBarHeight - TOP_BAR_PADDING * 2.0f;
    const float availablePanelWidth = std::max(0.0f, contentWidth - TOP_BAR_GAP * 5.0f);
    const float uavWidth = availablePanelWidth * 0.12f;
    const float serverWidth = availablePanelWidth * 0.13f;
    const float timeWidth = availablePanelWidth * 0.20f;
    const float flightWidth = availablePanelWidth * 0.19f;
    const float timerWidth = availablePanelWidth * 0.18f;
    const float alertWidth =
      std::max(260.0f, availablePanelWidth - uavWidth - serverWidth - timeWidth - flightWidth - timerWidth);

    float x = TOP_BAR_PADDING;
    const float y = TOP_BAR_PADDING;
    ImGui::SetCursorPos(ImVec2(x, y));
    drawStatusPanel(
      "##uav-link",
      ImVec2(uavWidth, panelHeight),
      ICON_MD_SIGNAL_CELLULAR_ALT_2_BAR,
      "UAV connection",
      "WEAK",
      "RSSI -67 dBm",
      color(255.0f, 193.0f, 100.0f),
      defaultFont,
      boldFont);

    x += uavWidth + TOP_BAR_GAP;
    ImGui::SetCursorPos(ImVec2(x, y));
    drawStatusPanel(
      "##competition-server",
      ImVec2(serverWidth, panelHeight),
      ICON_MD_DNS,
      "Competition server",
      "1.78 Hz",
      "ONLINE",
      color(95.0f, 219.0f, 147.0f),
      defaultFont,
      boldFont);

    x += serverWidth + TOP_BAR_GAP;
    ImGui::SetCursorPos(ImVec2(x, y));
    drawTimeSyncPanel(ImVec2(timeWidth, panelHeight), defaultFont, boldFont);

    x += timeWidth + TOP_BAR_GAP;
    ImGui::SetCursorPos(ImVec2(x, y));
    drawFlightModePanel(ImVec2(flightWidth, panelHeight), defaultFont, boldFont);

    x += flightWidth + TOP_BAR_GAP;
    ImGui::SetCursorPos(ImVec2(x, y));
    drawRoundTimerPanel(ImVec2(timerWidth, panelHeight), defaultFont, boldFont);

    x += timerWidth + TOP_BAR_GAP;
    ImGui::SetCursorPos(ImVec2(x, y));
    drawAlertPanel(ImVec2(alertWidth, panelHeight), defaultFont, boldFont);
  }
  else if (viewport->WorkSize.x >= 1050.0f)
  {
    const float rowHeight = (topBarHeight - TOP_BAR_PADDING * 2.0f - TOP_BAR_GAP) * 0.5f;
    const float flightWidth = std::clamp(contentWidth * 0.25f, 300.0f, 360.0f);
    const float rightWidth = std::max(0.0f, contentWidth - flightWidth - TOP_BAR_GAP);
    const float xRight = TOP_BAR_PADDING + flightWidth + TOP_BAR_GAP;
    const float yTop = TOP_BAR_PADDING;
    const float yBottom = TOP_BAR_PADDING + rowHeight + TOP_BAR_GAP;

    ImGui::SetCursorPos(ImVec2(TOP_BAR_PADDING, yTop));
    drawFlightModePanel(ImVec2(flightWidth, rowHeight * 2.0f + TOP_BAR_GAP), defaultFont, boldFont);

    float uavWidth = std::clamp(rightWidth * 0.24f, 160.0f, 190.0f);
    float serverWidth = std::clamp(rightWidth * 0.28f, 190.0f, 226.0f);
    float timeWidth = rightWidth - uavWidth - serverWidth - TOP_BAR_GAP * 2.0f;
    if (timeWidth < 260.0f)
    {
      const float shortage = 260.0f - timeWidth;
      uavWidth = std::max(150.0f, uavWidth - shortage * 0.45f);
      serverWidth = std::max(170.0f, serverWidth - shortage * 0.55f);
      timeWidth = rightWidth - uavWidth - serverWidth - TOP_BAR_GAP * 2.0f;
    }

    float x = xRight;
    ImGui::SetCursorPos(ImVec2(x, yTop));
    drawStatusPanel(
      "##uav-link",
      ImVec2(uavWidth, rowHeight),
      ICON_MD_SIGNAL_CELLULAR_ALT_2_BAR,
      "UAV connection",
      "WEAK",
      "RSSI -67 dBm",
      color(255.0f, 193.0f, 100.0f),
      defaultFont,
      boldFont);

    x += uavWidth + TOP_BAR_GAP;
    ImGui::SetCursorPos(ImVec2(x, yTop));
    drawStatusPanel(
      "##competition-server",
      ImVec2(serverWidth, rowHeight),
      ICON_MD_DNS,
      "Competition server",
      "1.78 Hz",
      "ONLINE",
      color(95.0f, 219.0f, 147.0f),
      defaultFont,
      boldFont);

    x += serverWidth + TOP_BAR_GAP;
    ImGui::SetCursorPos(ImVec2(x, yTop));
    drawTimeSyncPanel(ImVec2(timeWidth, rowHeight), defaultFont, boldFont);

    const float timerWidth = std::clamp(rightWidth * 0.36f, 280.0f, 316.0f);
    const float alertWidth = std::max(260.0f, rightWidth - timerWidth - TOP_BAR_GAP);
    ImGui::SetCursorPos(ImVec2(xRight, yBottom));
    drawRoundTimerPanel(ImVec2(timerWidth, rowHeight), defaultFont, boldFont);

    ImGui::SetCursorPos(ImVec2(xRight + timerWidth + TOP_BAR_GAP, yBottom));
    drawAlertPanel(ImVec2(alertWidth, rowHeight), defaultFont, boldFont);
  }
  else
  {
    const float flightHeight = 106.0f;
    const float rowHeight = (topBarHeight - TOP_BAR_PADDING * 2.0f - flightHeight - TOP_BAR_GAP * 2.0f) * 0.5f;
    const float yTop = TOP_BAR_PADDING;
    const float yMiddle = yTop + flightHeight + TOP_BAR_GAP;
    const float yBottom = yMiddle + rowHeight + TOP_BAR_GAP;

    ImGui::SetCursorPos(ImVec2(TOP_BAR_PADDING, yTop));
    drawFlightModePanel(ImVec2(contentWidth, flightHeight), defaultFont, boldFont);

    float uavWidth = std::max(155.0f, contentWidth * 0.25f);
    float serverWidth = std::max(185.0f, contentWidth * 0.28f);
    float timeWidth = contentWidth - uavWidth - serverWidth - TOP_BAR_GAP * 2.0f;
    if (timeWidth < 240.0f)
    {
      const float shortage = 240.0f - timeWidth;
      uavWidth = std::max(140.0f, uavWidth - shortage * 0.45f);
      serverWidth = std::max(165.0f, serverWidth - shortage * 0.55f);
      timeWidth = contentWidth - uavWidth - serverWidth - TOP_BAR_GAP * 2.0f;
    }

    float x = TOP_BAR_PADDING;
    ImGui::SetCursorPos(ImVec2(x, yMiddle));
    drawStatusPanel(
      "##uav-link",
      ImVec2(uavWidth, rowHeight),
      ICON_MD_SIGNAL_CELLULAR_ALT_2_BAR,
      "UAV connection",
      "WEAK",
      "RSSI -67 dBm",
      color(255.0f, 193.0f, 100.0f),
      defaultFont,
      boldFont);

    x += uavWidth + TOP_BAR_GAP;
    ImGui::SetCursorPos(ImVec2(x, yMiddle));
    drawStatusPanel(
      "##competition-server",
      ImVec2(serverWidth, rowHeight),
      ICON_MD_DNS,
      "Competition server",
      "1.78 Hz",
      "ONLINE",
      color(95.0f, 219.0f, 147.0f),
      defaultFont,
      boldFont);

    x += serverWidth + TOP_BAR_GAP;
    ImGui::SetCursorPos(ImVec2(x, yMiddle));
    drawTimeSyncPanel(ImVec2(timeWidth, rowHeight), defaultFont, boldFont);

    const float timerWidth = std::min(std::max(contentWidth * 0.38f, 260.0f), std::max(260.0f, contentWidth * 0.48f));
    const float alertWidth = std::max(220.0f, contentWidth - timerWidth - TOP_BAR_GAP);
    ImGui::SetCursorPos(ImVec2(TOP_BAR_PADDING, yBottom));
    drawRoundTimerPanel(ImVec2(timerWidth, rowHeight), defaultFont, boldFont);

    ImGui::SetCursorPos(ImVec2(TOP_BAR_PADDING + timerWidth + TOP_BAR_GAP, yBottom));
    drawAlertPanel(ImVec2(alertWidth, rowHeight), defaultFont, boldFont);
  }

  ImGui::End();
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
  ImGuiID upperSideDockId = 0;
  ImGuiID controlsDockId = 0;
  ImGuiID actionsDockId = 0;
  ImGuiID telemetryDockId = 0;
  ImGui::DockBuilderSplitNode(mainDockId, ImGuiDir_Right, 0.26f, &sideDockId, &mainDockId);
  ImGui::DockBuilderSplitNode(sideDockId, ImGuiDir_Down, 0.48f, &telemetryDockId, &upperSideDockId);
  ImGui::DockBuilderSplitNode(upperSideDockId, ImGuiDir_Down, 0.50f, &actionsDockId, &controlsDockId);
  ImGui::DockBuilderDockWindow(MAIN_VIEW_WINDOW_NAME, mainDockId);
  ImGui::DockBuilderDockWindow(SIDE_PANEL_WINDOW_NAME, controlsDockId);
  ImGui::DockBuilderDockWindow(ACTIONS_WINDOW_NAME, actionsDockId);
  ImGui::DockBuilderDockWindow(TELEMETRY_WINDOW_NAME, telemetryDockId);
  ImGui::DockBuilderFinish(dockspaceId);
  defaultLayoutBuilt = true;
}

void ImGuiLayer::drawDockspace()
{
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const ImVec2 dockspacePos(viewport->WorkPos.x, viewport->WorkPos.y + topBarHeight);
  const ImVec2 dockspaceSize(viewport->WorkSize.x, std::max(0.0f, viewport->WorkSize.y - topBarHeight));

  ImGui::SetNextWindowPos(dockspacePos);
  ImGui::SetNextWindowSize(dockspaceSize);
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
  setupDefaultDockLayout(dockspaceId, dockspaceSize.x, dockspaceSize.y);

  constexpr ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;
  ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspaceFlags);

  ImGui::End();
}

ViewportRect ImGuiLayer::beginMainView()
{
  drawTopBar();
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

bool ImGuiLayer::drawSidePanel(CameraMode cameraMode, double& cameraSpeed)
{
  ImGui::Begin(SIDE_PANEL_WINDOW_NAME, nullptr, ImGuiWindowFlags_NoCollapse);

  ImGui::PushFont(boldFont, UI_FONT_SIZE_MEDIUM);
  ImGui::TextUnformatted(ICON_MD_TUNE " Controls");
  ImGui::PopFont();
  ImGui::Separator();
  ImGui::Text("FPS %.1f", ImGui::GetIO().Framerate);
  ImGui::Spacing();

  const char* modeLabel = cameraMode == CameraMode::Orthographic ? ICON_MD_MAP " Orthographic" : ICON_MD_FLIGHT " FPS";
  const bool cameraModeClicked = ImGui::Button(modeLabel);
  ImGui::SameLine();
  ImGui::PushFont(defaultFont, UI_FONT_SIZE_SMALL);
  ImGui::TextUnformatted("Camera");
  ImGui::PopFont();

  float cameraSpeedValue = static_cast<float>(cameraSpeed);
  if (ImGui::SliderFloat("Speed", &cameraSpeedValue, 10.0f, 100000.0f, "%.0f m/s", ImGuiSliderFlags_Logarithmic))
  {
    cameraSpeed = static_cast<double>(cameraSpeedValue);
  }

  ImGui::Spacing();
  ImGui::PushFont(boldFont, UI_FONT_SIZE_MEDIUM);
  ImGui::TextUnformatted(ICON_MD_INFO " Info");
  ImGui::PopFont();
  ImGui::Separator();
  ImGui::PushFont(defaultFont, UI_FONT_SIZE_SMALL);
  ImGui::TextUnformatted("No telemetry selected");
  ImGui::PopFont();

  ImGui::End();

  return cameraModeClicked;
}

void ImGuiLayer::drawActionsWindow()
{
  static float targetSpeedMetersPerSecond = 24.0f;
  static float targetAltitudeMeters = 120.0f;

  const ImVec4 commandAccent = color(92.0f, 204.0f, 220.0f);
  const ImVec4 missionAccent = color(95.0f, 219.0f, 147.0f);
  const ImVec4 loiterAccent = color(181.0f, 148.0f, 255.0f);
  const ImVec4 landAccent = color(255.0f, 205.0f, 127.0f);
  const ImVec4 dangerAccent = color(255.0f, 72.0f, 89.0f);

  ImGui::Begin(ACTIONS_WINDOW_NAME, nullptr, ImGuiWindowFlags_NoCollapse);

  drawWindowTitle(boldFont, ICON_MD_TOUCH_APP, "Actions", commandAccent);

  drawActionSectionHeader(boldFont, ICON_MD_SPEED, "Change speed", commandAccent);
  drawActionInputRow(
    "##action-speed", "Set##action-speed", "m/s", targetSpeedMetersPerSecond, 1.0f, 0.0f, defaultFont, boldFont);

  ImGui::Spacing();
  drawActionSectionHeader(boldFont, ICON_MD_HEIGHT, "Change altitude", commandAccent);
  drawActionInputRow(
    "##action-altitude", "Set##action-altitude", "m", targetAltitudeMeters, 5.0f, 0.0f, defaultFont, boldFont);

  ImGui::Spacing();
  drawActionSectionHeader(boldFont, ICON_MD_FLAG, "Missions", missionAccent);
  drawActionButton(ICON_MD_FLIGHT_TAKEOFF " Start fight mission", missionAccent, boldFont, ImVec2(-1.0f, 0.0f));
  drawActionButton(ICON_MD_ROCKET_LAUNCH " Start kamikaze mission", dangerAccent, boldFont, ImVec2(-1.0f, 0.0f));

  ImGui::Spacing();
  const float halfWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
  drawActionButton(ICON_MD_PAUSE_CIRCLE " Loiter", loiterAccent, boldFont, ImVec2(halfWidth, 0.0f));
  ImGui::SameLine();
  drawActionButton(ICON_MD_FLIGHT_LAND " Land", landAccent, boldFont, ImVec2(halfWidth, 0.0f));

  ImGui::End();
}

void ImGuiLayer::drawTelemetryWindow()
{
  ImGui::Begin(TELEMETRY_WINDOW_NAME, nullptr, ImGuiWindowFlags_NoCollapse);

  drawWindowTitle(boldFont, ICON_MD_ASSISTANT_NAVIGATION, "Aircraft telemetry", color(92.0f, 204.0f, 220.0f));

  const ImVec4 speedAccent = color(92.0f, 204.0f, 220.0f);
  const ImVec4 headingAccent = color(181.0f, 148.0f, 255.0f);
  const ImVec4 climbAccent = color(126.0f, 223.0f, 149.0f);
  const ImVec4 powerAccent = color(255.0f, 205.0f, 127.0f);
  const ImVec4 gpsAccent = color(95.0f, 219.0f, 147.0f);

  drawTelemetrySectionHeader(boldFont, ICON_MD_SPEED, "Motion", speedAccent);
  drawTelemetryTwoColumnRow(
    "##telemetry-motion-speed",
    "##telemetry-airspeed",
    ICON_MD_AIR,
    "Airspeed",
    "28.4 m/s",
    speedAccent,
    "##telemetry-groundspeed",
    ICON_MD_SPEED,
    "Ground speed",
    "31.2 m/s",
    speedAccent,
    defaultFont,
    boldFont);
  drawTelemetryTwoColumnRow(
    "##telemetry-motion-attitude",
    "##telemetry-heading",
    ICON_MD_EXPLORE,
    "Heading",
    "086 deg",
    headingAccent,
    "##telemetry-vertical-speed",
    ICON_MD_SWAP_VERT,
    "Vertical speed",
    "+1.6 m/s",
    climbAccent,
    defaultFont,
    boldFont);

  drawTelemetrySectionHeader(boldFont, ICON_MD_BATTERY_6_BAR, "Power", powerAccent);
  drawTelemetryTwoColumnRow(
    "##telemetry-power-main",
    "##telemetry-voltage",
    ICON_MD_BATTERY_6_BAR,
    "Voltage",
    "23.6 V",
    powerAccent,
    "##telemetry-current",
    ICON_MD_ELECTRIC_BOLT,
    "Current draw",
    "14.8 A",
    powerAccent,
    defaultFont,
    boldFont);
  drawTelemetryBatteryTile(
    "##telemetry-charge", ICON_MD_BATTERY_FULL, "State of charge", "78%", 0.78f, powerAccent, defaultFont, boldFont);

  drawTelemetrySectionHeader(boldFont, ICON_MD_GPS_FIXED, "GPS", gpsAccent);
  drawTelemetryTwoColumnRow(
    "##telemetry-gps-status",
    "##telemetry-gps-fix",
    ICON_MD_GPS_FIXED,
    "Status",
    "3D Fix",
    gpsAccent,
    "##telemetry-gps-sats",
    ICON_MD_SATELLITE_ALT,
    "Satellites",
    "18",
    gpsAccent,
    defaultFont,
    boldFont);
  drawTelemetryTwoColumnRow(
    "##telemetry-gps-dop",
    "##telemetry-gps-hdop",
    ICON_MD_MY_LOCATION,
    "HDOP",
    "0.8",
    gpsAccent,
    "##telemetry-gps-vdop",
    ICON_MD_VERTICAL_ALIGN_CENTER,
    "VDOP",
    "1.2",
    gpsAccent,
    defaultFont,
    boldFont);

  ImGui::End();
}
