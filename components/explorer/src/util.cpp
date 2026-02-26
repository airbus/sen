// === util.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "util.h"

// sen
#include "sen/core/base/checked_conversions.h"

// spdlog
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

// imgui
#include <imgui.h>

// std
#include <memory>
#include <string_view>

namespace sen::components::explorer
{

std::shared_ptr<spdlog::logger> getLogger()
{
  constexpr auto* loggerName = "explorer";
  static auto logger = spdlog::get(loggerName);
  if (!logger)
  {
    logger = spdlog::stdout_color_mt(loggerName);
  }

  return logger;
}

void setUIError(UIError& error, const std::string_view msg)
{
  error.message = msg;
  error.errorTime = ImGui::GetTime();
}

void clearUIError(UIError& error) { error.message.clear(); }

void drawMessageError(UIError& error, const double duration)
{
  if (!error.message.empty())
  {
    if (const double timeSinceError = ImGui::GetTime() - error.errorTime; timeSinceError < duration)
    {
      float alpha = 1.0f;
      if (const double fadeStartTime = duration - 1.0; timeSinceError > fadeStartTime)
      {
        alpha = 1.0f - std_util::ignoredLossyConversion<float>(timeSinceError - fadeStartTime);
      }

      ImGui::TextColored(  // NOLINT (hicpp-vararg)
        ImVec4(1.0f, 0.2f, 0.2f, alpha),
        "%s",
        error.message.c_str());
    }
    else
    {
      clearUIError(error);
    }
  }
}

void drawTooltipError(UIError& error, std::string tooltipTitle, const double duration)
{
  if (!error.message.empty())
  {
    if (const double timeSinceError = ImGui::GetTime() - error.errorTime; timeSinceError < duration)
    {
      float alpha = 1.0f;
      if (const double fadeStartTime = duration - 1.0; timeSinceError > fadeStartTime)
      {
        alpha = 1.0f - std_util::ignoredLossyConversion<float>(timeSinceError - fadeStartTime);
      }

      ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, alpha), "(!)");  // NOLINT (hicpp-vararg)

      if (ImGui::IsItemHovered())
      {
        tooltipTitle.append("\n");
        ImGui::SetTooltip("%s", tooltipTitle.c_str());  // NOLINT (hicpp-vararg)
      }

      if (ImGui::BeginItemTooltip())  // NOLINT (hicpp-vararg)
      {
        ImGui::TextColored(  // NOLINT (hicpp-vararg)
          ImVec4(1, 0.5, 0.5, 1.0f),
          "%s",
          error.message.c_str());
        ImGui::EndTooltip();
      }
    }
    else
    {
      clearUIError(error);
    }
  }
}

}  // namespace sen::components::explorer
