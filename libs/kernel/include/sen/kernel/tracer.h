// === tracer.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_KERNEL_TRACER_H
#define SEN_KERNEL_TRACER_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/scope_guard.h"
#include "sen/core/base/source_location.h"

// std
#include <functional>
#include <memory>
#include <string_view>

namespace sen::kernel
{

/// Interface implemented by tracers. You can use it to trace the behavior
/// of your code. Please note that not all tracers support the functions below.
class Tracer
{
  SEN_NOCOPY_NOMOVE(Tracer)

public:
  Tracer() = default;
  virtual ~Tracer() = default;

public:  // for scoped (RAII) zones
  /// Create a scoped zone.
  /// You can use scoped zones to trace the lifetime of a given block (the typical example is a function).
  ///
  /// @param location  Source location; pass the result of `SEN_SL()`.
  /// @return RAII scope-guard that calls `zoneEnd()` on destruction.
  ///
  /// example usage:
  /// @code
  /// int myFunc()
  /// {
  ///   static constexpr auto sl = SEN_SL();
  ///   auto zone = tracer.makeScopedZone(sl);
  /// }
  /// @endcode
  ///
  /// For a less verbose approach, you can use:
  ///
  /// @code
  /// int myFunc(Tracer& tracer)
  /// {
  ///   SEN_TRACE_ZONE(tracer);
  /// }
  /// @endcode
  [[nodiscard]] auto makeScopedZone(const SourceLocation& location);

  /// Create a scoped zone with a given name.
  /// @param name      Human-readable label; must remain constant for a given call site across the program lifetime.
  /// @param location  Source location; pass the result of `SEN_SL()`.
  /// @return RAII scope-guard that calls `zoneEnd()` on destruction.
  ///
  /// example usage:
  /// @code
  /// void myFunc(Tracer& tracer)
  /// {
  ///   static constexpr auto sl = SEN_SL();
  ///   auto zone = tracer.makeScopedZone("doing something", sl);
  /// }
  /// @endcode
  ///
  /// For a less verbose approach, you can use:
  ///
  /// @code
  /// void myFunc(Tracer& tracer)
  /// {
  ///   SEN_TRACE_ZONE_NAMED(tracer, "doing something");
  /// }
  /// @endcode
  [[nodiscard]] auto makeScopedZone(std::string_view name, const SourceLocation& location);

public:  // for frame-based tracers
  /// Marks the start of a (named) frame.
  /// Needs to be paired with a corresponding call to frameEnd.
  /// @param name should not change for a given frame mark during the program execution.
  /// @note Only supported by frame-oriented tracers.
  virtual void frameStart(std::string_view name) = 0;

  /// Marks the end of a (named) frame.
  /// Needs to be paired with a corresponding call to frameStart.
  /// @param name should not change for a given frame mark during the program execution.
  /// @note Only supported by frame-oriented tracers.
  virtual void frameEnd(std::string_view name) = 0;

public:  // messages
  /// Sends a message that will be traced.
  /// @param name should not change for a given trace during the program execution.
  virtual void message(std::string_view name) = 0;

  /// Sends a value that will be plotted.
  /// @param name should not change for a given plot during the program execution.
  /// @param value a value.
  /// @note This function might not be available in all tracers.
  virtual void plot(std::string_view name, float64_t value) = 0;

  /// Sends a value that will be plotted.
  /// @param name should not change for a given plot during the program execution.
  /// @param value a value.
  /// @note This function might not be available in all tracers.
  virtual void plot(std::string_view name, int64_t value) = 0;

protected:
  virtual void zoneStart(std::string_view name, const SourceLocation& location) = 0;
  virtual void zoneEnd() = 0;
};

/// A factory function for tracers
using TracerFactory = std::function<std::unique_ptr<Tracer>(std::string_view)>;

/// A function that returns a no-op tracer.
[[nodiscard]] TracerFactory getDefaultTracerFactory();

// clang-format off
/// Helper macro that creates a static source location and defines a named scoped zone.
///
/// Example:
///
/// @code
/// void myFunc(Tracer& tracer)
/// {
///   SEN_TRACE_ZONE_NAMED(tracer, "doing something");
/// }
/// @endcode
/// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SEN_TRACE_ZONE_NAMED(tracer, name) static constexpr auto __senSourceLocation ## __LINE__ = SEN_SL(); auto __senScopedZone ## __LINE__ = (tracer).makeScopedZone(name, __senSourceLocation ## __LINE__)

/// Helper macro that creates a static source location and defines a scoped zone.
///
/// Example:
///
/// @code
/// int myFunc(Tracer& tracer)
/// {
///   SEN_TRACE_ZONE(tracer);
/// }
/// @endcode
/// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SEN_TRACE_ZONE(tracer) static constexpr auto __senSourceLocation ## __LINE__ = SEN_SL(); auto __senScopedZone ## __LINE__ = (tracer).makeScopedZone(__senSourceLocation ## __LINE__)
// clang-format on

//--------------------------------------------------------------------------------------------------------------
// Inline method implementation
//--------------------------------------------------------------------------------------------------------------

inline auto Tracer::makeScopedZone(std::string_view name, const SourceLocation& location)
{
  zoneStart(std::move(name), location);
  return makeScopeGuard([this]() { zoneEnd(); });
}

inline auto Tracer::makeScopedZone(const SourceLocation& location) { return makeScopedZone({}, location); }

}  // namespace sen::kernel

#endif  // SEN_KERNEL_TRACER_H
