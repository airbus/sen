// === value.cpp =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "value.h"

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/obj/object.h"

// imgui
#include "imgui.h"

// std
#include <cfloat>
#include <cstdint>
#include <string_view>

constexpr ImVec4 stringColor(0.7f, 0.7f, 0.7f, 1.0f);
constexpr ImVec4 numberColor(0.3f, 1.0f, 0.3f, 1.0f);
constexpr ImVec4 enumColor(1.0f, 1.0f, 0.0f, 1.0f);
constexpr ImVec4 boolColor(0.0f, 1.0f, 1.0f, 1.0f);

const ImVec4& Value::getNumberColor() noexcept { return numberColor; }

const ImVec4& Value::getStringColor() noexcept { return stringColor; }

void Value::setupColumns()
{
  ImGui::TableSetupColumn("name");
  ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
}

void Value::drawHelpTooltip(const char* description)
{
  if (description == nullptr || *description == '\0')
  {
    return;
  }
  ImGui::SameLine();
  ImGui::TextColored(Value::getStringColor(), "(?)");  // NOLINT(hicpp-vararg)
  if (ImGui::BeginItemTooltip())
  {
    ImGui::TextColored(Value::getStringColor(), "%s", description);  // NOLINT(hicpp-vararg)
    ImGui::EndTooltip();
  }
  ImGui::SameLine();
}

void Value::writeName(std::string_view name, const char* description)
{
  ImGui::TableNextRow();

  ImGui::TableSetColumnIndex(0);
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(name.data());

  drawHelpTooltip(description);

  ImGui::TableSetColumnIndex(1);
  ImGui::SetNextItemWidth(-FLT_MIN);
}

void Value::printEnum(uint32_t key, const sen::EnumType& type)
{
  const auto* enumerator = type.getEnumFromKey(key);
  if (enumerator)
  {
    printEnum(enumerator->name);
  }
  else
  {
    printEnum("?");
  }
}

void Value::printEnum(const std::string& value)
{
  ImGui::TextColored(enumColor, "[%s]", value.c_str());  // NOLINT
}

void Value::printIntegral(uint8_t value)
{
  ImGui::PushStyleColor(ImGuiCol_Text, numberColor);
  ImGui::Text("%u", value);  // NOLINT
  ImGui::PopStyleColor();
}

void Value::printIntegral(uint16_t value)
{
  ImGui::PushStyleColor(ImGuiCol_Text, numberColor);
  ImGui::Text("%u", value);  // NOLINT
  ImGui::PopStyleColor();
}

void Value::printIntegral(int16_t value)
{
  ImGui::PushStyleColor(ImGuiCol_Text, numberColor);
  ImGui::Text("%d", value);  // NOLINT
  ImGui::PopStyleColor();
}

void Value::printIntegral(uint32_t value)
{
  ImGui::PushStyleColor(ImGuiCol_Text, numberColor);
  ImGui::Text("%u", value);  // NOLINT
  ImGui::PopStyleColor();
}

void Value::printIntegral(int32_t value)
{
  ImGui::PushStyleColor(ImGuiCol_Text, numberColor);
  ImGui::Text("%d", value);  // NOLINT
  ImGui::PopStyleColor();
}

void Value::printIntegral(uint64_t value)
{
  ImGui::PushStyleColor(ImGuiCol_Text, numberColor);
  ImGui::Text("%lu", value);  // NOLINT
  ImGui::PopStyleColor();
}

void Value::printIntegral(int64_t value)
{
  ImGui::PushStyleColor(ImGuiCol_Text, numberColor);
  ImGui::Text("%ld", value);  // NOLINT
  ImGui::PopStyleColor();
}

void Value::printFloat(float32_t value)
{
  ImGui::PushStyleColor(ImGuiCol_Text, numberColor);
  ImGui::Text("%f", static_cast<double>(value));  // NOLINT
  ImGui::PopStyleColor();
}

void Value::printFloat(float64_t value)
{
  ImGui::PushStyleColor(ImGuiCol_Text, numberColor);
  ImGui::Text("%f", value);  // NOLINT
  ImGui::PopStyleColor();
}

void Value::printBool(bool value)
{
  ImGui::PushStyleColor(ImGuiCol_Text, boolColor);
  ImGui::TextUnformatted(value ? "[true]" : "[false]");
  ImGui::PopStyleColor();
}

void Value::printString(const std::string& value)
{
  ImGui::TextColored(stringColor, "\"%s\"", value.c_str());  // NOLINT
}

void Value::printTime(const sen::TimeStamp& value)
{
  ImGui::PushStyleColor(ImGuiCol_Text, numberColor);
  ImGui::TextUnformatted(value.toLocalString().c_str());
  ImGui::PopStyleColor();
}

void Value::printObjectId(const sen::ObjectId id)
{
  ImGui::PushStyleColor(ImGuiCol_Text, numberColor);
  ImGui::Text("0x%08x", id.get());  // NOLINT
  ImGui::PopStyleColor();
}
