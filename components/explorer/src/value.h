// === value.h =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_EXPLORER_SRC_VALUE_H
#define SEN_COMPONENTS_EXPLORER_SRC_VALUE_H

// sen
#include "imgui.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/obj/object.h"

// std
#include <string_view>

class Value
{
  SEN_NOCOPY_NOMOVE(Value)

public:
  Value() = default;
  ~Value() = default;

public:
  static constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |  // NOLINT
                                                ImGuiTableFlags_Resizable |                        // NOLINT
                                                ImGuiTableFlags_SizingFixedFit;                    // NOLINT

public:
  static void setupColumns();
  static void drawHelpTooltip(const char* description);
  static void writeName(std::string_view name, const char* description = nullptr);
  [[nodiscard]] static const ImVec4& getNumberColor() noexcept;
  [[nodiscard]] static const ImVec4& getStringColor() noexcept;

public:
  static void printEnum(uint32_t key, const sen::EnumType& type);
  static void printEnum(const std::string& value);
  static void printIntegral(uint8_t value);
  static void printIntegral(uint16_t value);
  static void printIntegral(int16_t value);
  static void printIntegral(uint32_t value);
  static void printIntegral(int32_t value);
  static void printIntegral(uint64_t value);
  static void printIntegral(int64_t value);
  static void printFloat(float32_t value);
  static void printFloat(float64_t value);
  static void printBool(bool value);
  static void printString(const std::string& value);
  static void printTime(const sen::TimeStamp& value);
  static void printObjectId(const sen::ObjectId id);
};

#endif  // SEN_COMPONENTS_EXPLORER_SRC_VALUE_H
