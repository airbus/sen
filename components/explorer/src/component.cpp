// === component.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "event_explorer.h"
#include "icons_font.h"
#include "main_bar.h"
#include "value_font.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/io/util.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/configuration.stl.h"

// imgui
#include "imgui.h"

// implot
#include "implot.h"

// std
#include <string>
#include <utility>

//--------------------------------------------------------------------------------------------------------------
// Forward declarations of helper functions
//--------------------------------------------------------------------------------------------------------------

void backendOpen();
void backendSetup();
bool backendPreUpdate();
void backendPostUpdate(const ImVec4& clearColor);
void backendClose();

//--------------------------------------------------------------------------------------------------------------
// Theme
//--------------------------------------------------------------------------------------------------------------

void setTheme()
{
  auto& colors = ImGui::GetStyle().Colors;
  colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
  colors[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 0.40f);
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.4f);
  colors[ImGuiCol_FrameBg] = ImVec4(0.6f, 0.6f, 0.6f, 0.54f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.4f, 0.4f, 0.4f, 0.54f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
  colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
  colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
  colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
  colors[ImGuiCol_Button] = ImVec4(0.10f, 0.10f, 0.10f, 0.54f);
  colors[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
  colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
  colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 0.52f);
  colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
  colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
  colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
  colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
  colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
  colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
  colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
  colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
  colors[ImGuiCol_Tab] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
  colors[ImGuiCol_TabActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
  colors[ImGuiCol_TabUnfocused] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
  colors[ImGuiCol_DockingPreview] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
  colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
  colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
  colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
  colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
  colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
  colors[ImGuiCol_TableHeaderBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
  colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
  colors[ImGuiCol_TableBorderLight] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
  colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
  colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
  colors[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
  colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
  colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 0.70f);
  colors[ImGuiCol_NavWindowingDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.20f);
  colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.50f);

  auto& style = ImGui::GetStyle();
  style.WindowPadding = ImVec2(8.00f, 8.00f);
  style.FramePadding = ImVec2(5.00f, 2.00f);
  style.CellPadding = ImVec2(6.00f, 1.00f);
  style.ItemSpacing = ImVec2(6.00f, 6.00f);
  style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
  style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
  style.IndentSpacing = 25;
  style.ScrollbarSize = 15;
  style.GrabMinSize = 10;
  style.WindowBorderSize = 1;
  style.ChildBorderSize = 1;
  style.PopupBorderSize = 1;
  style.FrameBorderSize = 1;
  style.TabBorderSize = 1;
  style.WindowRounding = 7;
  style.ChildRounding = 4;
  style.FrameRounding = 3;
  style.PopupRounding = 4;
  style.ScrollbarRounding = 9;
  style.GrabRounding = 3;
  style.LogSliderDeadzone = 4;
  style.TabRounding = 4;
}

//--------------------------------------------------------------------------------------------------------------
// ExplorerComponent
//--------------------------------------------------------------------------------------------------------------

struct ExplorerComponent: public sen::kernel::Component
{
  [[nodiscard]] sen::kernel::FuncResult run(sen::kernel::RunApi& api) override
  {
    const auto config = sen::toValue<sen::components::explorer::Configuration>(api.getConfig());

    static constexpr ImVec4 clearColor(0.45f, 0.55f, 0.6f, 1.0f);
    constexpr auto cycleTime = sen::Duration::fromHertz(60.f);

    // context
    backendOpen();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    // general flags
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // NOLINT(hicpp-signed-bitwise)
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // NOLINT(hicpp-signed-bitwise)

    const std::string lastLayoutFile = "last-layout.ini";
    io.IniFilename = nullptr;
    if (!config.layoutFile.empty())
    {
      ImGui::LoadIniSettingsFromDisk(config.layoutFile.c_str());
    }
    else
    {
      ImGui::LoadIniSettingsFromDisk(lastLayoutFile.data());
    }

    // setup
    setTheme();
    backendSetup();
    addValueFont();
    addIconsFont();

    sen::kernel::FuncResult result = done();
    {
      EventExplorer eventExplorer(api.getWorkQueue());
      MainBar mainBar(api, &eventExplorer);

      auto update = [&api, &mainBar, &eventExplorer, lastLayoutFile]
      {
        static bool firstFrame = true;

        if (backendPreUpdate())
        {
          api.requestKernelStop();
        }

        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport();

        ImGui::SetNextWindowSize(ImVec2(150, 0), ImGuiCond_FirstUseEver);
        mainBar.update();

        if (firstFrame)
        {
          const auto sessionsWindowPos = mainBar.getWindowPos();
          const auto sessionsWindowSize = mainBar.getWindowSize();
          const auto eventsWindowPos =
            ImVec2(sessionsWindowPos.x + sessionsWindowSize.x + ImGui::GetStyle().ItemSpacing.x, sessionsWindowPos.y);
          ImGui::SetNextWindowPos(eventsWindowPos, ImGuiCond_FirstUseEver);
          firstFrame = false;
        }

        eventExplorer.update();

        ImGui::Render();
        backendPostUpdate(clearColor);

        if (ImGui::GetIO().WantSaveIniSettings)
        {
          static float saveTimer = 0.0f;
          saveTimer += ImGui::GetIO().DeltaTime;

          if (saveTimer > 1.0f)
          {
            ImGui::SaveIniSettingsToDisk(lastLayoutFile.data());
            ImGui::GetIO().WantSaveIniSettings = false;
            saveTimer = 0.0f;
          }
        }
      };

      result = api.execLoop(cycleTime, std::move(update), false);
    }

    backendClose();
    ImGui::DestroyContext();
    ImPlot::DestroyContext();
    return result;
  }

  [[nodiscard]] bool isRealTimeOnly() const noexcept override { return true; }
};

SEN_COMPONENT(ExplorerComponent)
