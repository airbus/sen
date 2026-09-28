// === util.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_UTIL_H
#define SEN_COMPONENTS_TERM_SRC_UTIL_H

// sen
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/unit.h"

// generated code
#include "stl/term.stl.h"

// spdlog
#include <spdlog/logger.h>

// std
#include <array>
#include <cstddef>
#include <memory>
#include <string>

namespace sen::components::term
{

/// The component's own logger, so its messages are attributed and can be levelled by name like
/// every other component's. LogRouter injects the term sink into it, so they render in the output.
std::shared_ptr<spdlog::logger> getLogger();

/// How many bytes of text one line may hold: the command line, one field of a guided-input form, and one
/// line of the history file. Some bound is necessary, because past it the term stays alive and stops
/// acting on input, re-wrapping the whole buffer every frame. One constant for all three, so that raising
/// it cannot leave one behind and make a command the line accepted absent from the next session's
/// history.
///
/// The figure is a judgement rather than a measurement. 4096 refused a few hundred small structs as one
/// sequence argument, which is a real call; 8192 holds roughly twice that and keeps the per-frame work
/// bounded.
constexpr std::size_t maxLineBytes = 8192;

/// A return type spelled "void". One function, because a literal compared in several places drifts when
/// the type's name changes.
[[nodiscard]] inline bool isVoidTypeName(std::string_view returnTypeName) { return returnTypeName == "void"; }

/// Every unit category, in declaration order. One list, because a hand-written copy drifts and drops a
/// category. Display and filter names come from Unit::getCategoryString, so completion offers exactly
/// what the filter compares against.
inline constexpr std::array<UnitCategory, 15U> allUnitCategories {
  UnitCategory::length,
  UnitCategory::mass,
  UnitCategory::time,
  UnitCategory::angle,
  UnitCategory::temperature,
  UnitCategory::density,
  UnitCategory::pressure,
  UnitCategory::area,
  UnitCategory::force,
  UnitCategory::frequency,
  UnitCategory::velocity,
  UnitCategory::angularVelocity,
  UnitCategory::acceleration,
  UnitCategory::angularAcceleration,
  UnitCategory::torque,
};

/// The time term shows, in the style the configuration asked for.
///
/// Every time the term draws comes through here, so the configured style holds for all of them rather
/// than for the call sites that remember to ask.
[[nodiscard]] inline std::string formatTime(const TimeStamp& time, TimeStyle style)
{
  return style == TimeStyle::local ? time.toLocalString() : time.toUtcString();
}

/// The same, cut to `HH:MM:SS`. Both spellings put the clock at offset 11.
[[nodiscard]] inline std::string formatShortTime(const TimeStamp& time, TimeStyle style)
{
  auto full = formatTime(time, style);
  return (full.size() >= 19U) ? full.substr(11U, 8U) : full;
}

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_UTIL_H
