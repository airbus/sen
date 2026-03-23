// === duration.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_DURATION_H
#define SEN_CORE_BASE_DURATION_H

#include "sen/core/base/numbers.h"
#include "sen/core/base/strong_type.h"

// std
#include <chrono>

namespace sen
{

/// \addtogroup util
/// @{

/// A signed time interval stored as a 64-bit nanosecond count.
///
/// `Duration` is trivially copyable and interoperates with `std::chrono::duration`
/// via implicit constructors. Use `fromHertz()` to express cycle periods as frequencies.
class Duration final: public StrongType<std::int64_t, Duration>
{
public:
  using Base = sen::StrongType<int64_t, Duration>;
  using Base::Base;
  using Base::get;
  using Base::set;
  using ValueType = Base::ValueType;

  /// Implicit conversion from `std::chrono::nanoseconds`.
  /// @param value Nanosecond duration from the `<chrono>` library.
  template <class Rep, class Period>
  constexpr Duration(std::chrono::nanoseconds value) noexcept  // NOLINT(hicpp-explicit-conversions)
    : Base(value.count())
  {
  }

  /// Implicit conversion from any `std::chrono::duration` specialisation.
  /// The value is truncated to whole nanoseconds.
  /// @param value Any `std::chrono::duration` (e.g. `std::chrono::milliseconds{5}`).
  template <class Rep, class Period>
  constexpr Duration(std::chrono::duration<Rep, Period> value) noexcept  // NOLINT(hicpp-explicit-conversions)
  {
    set(static_cast<ValueType>((value.count() / std::ratio_divide<Period, std::nano>::den) *
                               std::ratio_divide<Period, std::nano>::num));
  }

public:  // special members
  constexpr SEN_MOVE_CONSTRUCT(Duration) = default;
  constexpr SEN_COPY_CONSTRUCT(Duration) = default;
  constexpr SEN_MOVE_ASSIGN(Duration) = default;
  constexpr SEN_COPY_ASSIGN(Duration) = default;
  ~Duration() noexcept = default;

public:  // accessors
  /// Returns the duration as a raw nanosecond count.
  /// @return Signed 64-bit nanosecond count.
  [[nodiscard]] constexpr ValueType getNanoseconds() const noexcept { return get(); }

  /// Converts to a `std::chrono::nanoseconds` for use with the `<chrono>` library.
  /// @return Equivalent `std::chrono::nanoseconds` value.
  [[nodiscard]] constexpr std::chrono::nanoseconds toChrono() const noexcept
  {
    return std::chrono::nanoseconds {get()};
  }

  /// Returns the duration in seconds as a 64-bit floating-point number.
  /// @return Duration in seconds; fractional for sub-second values.
  [[nodiscard]] constexpr float64_t toSeconds() const noexcept
  {
    return std::chrono::duration<float64_t>(toChrono()).count();
  }

public:
  /// Creates a `Duration` equal to one period of the given frequency.
  /// Useful for expressing component cycle times as Hz (e.g. `Duration::fromHertz(60.0)`).
  /// @pre `hz != 0.0`
  /// @param hz Frequency in Hertz.
  /// @return Duration equal to `1 / hz` seconds.
  [[nodiscard]] static constexpr Duration fromHertz(float64_t hz) noexcept
  {
    using F64Seconds = std::chrono::duration<float64_t, std::chrono::seconds::period>;
    return {F64Seconds(1.0 / hz)};
  }

  /// Creates a `Duration` equal to one period of the given frequency (single-precision overload).
  /// @pre `hz != 0.0f`
  /// @param hz Frequency in Hertz.
  /// @return Duration equal to `1 / hz` seconds.
  [[nodiscard]] static constexpr Duration fromHertz(float32_t hz) noexcept
  {
    using F32Seconds = std::chrono::duration<float32_t, std::chrono::seconds::period>;
    return {F32Seconds(1.0f / hz)};
  }
};

template <>
struct ShouldBePassedByValue<Duration>: std::true_type
{
};

static_assert(std::is_trivially_copyable_v<Duration>);

/// @}

}  // namespace sen

#endif  // SEN_CORE_BASE_DURATION_H
