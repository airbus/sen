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

/// How many bytes of text one line may hold: the command line, one field of a guided-input form, and
/// one line of the history file. Some bound is necessary, because past it the term stays alive and
/// stops acting on input -- it re-wraps the whole buffer every frame. One constant because there were
/// three, in two files, and raising two of them left the third behind: a command the line accepted and
/// ran was then silently absent from the next session's history.
///
/// The figure is a judgement rather than a measurement. 4096 refused a few hundred small structs as one
/// sequence argument, which is a real call; 8192 holds roughly twice that and keeps the per-frame work
/// bounded.
constexpr std::size_t maxLineBytes = 8192;

/// A return type spelled "void". Compared against that literal in four places, which is the sort of
/// thing that drifts when the type's name changes.
[[nodiscard]] inline bool isVoidTypeName(std::string_view returnTypeName) { return returnTypeName == "void"; }

/// Every unit category, in declaration order. One list because there were two hand-written copies
/// and both had drifted: neither listed `torque`, so `units` could never show it. Display and filter
/// names come from Unit::getCategoryString, so completion offers exactly what the filter compares
/// against -- it used to offer "angularVelocity" where the filter matched "angular velocity".
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
/// `timeStyle` was read out of the configuration and then used nowhere: every time on screen was local,
/// including under the declared default of `utc`, and TimeStamp *values* rendered as UTC beside them.
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
