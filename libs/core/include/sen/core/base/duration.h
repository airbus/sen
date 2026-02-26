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

/// A time duration.
class Duration final: public StrongType<std::int64_t, Duration>
{
public:
  using Base = sen::StrongType<int64_t, Duration>;
  using Base::Base;
  using Base::get;
  using Base::set;
  using ValueType = Base::ValueType;

  template <class Rep, class Period>
  constexpr Duration(std::chrono::nanoseconds value) noexcept  // NOLINT(hicpp-explicit-conversions)
    : Base(value.count())
  {
  }

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
  /// Number of nanoseconds.
  [[nodiscard]] constexpr ValueType getNanoseconds() const noexcept { return get(); }

  /// Conversion to std::chrono representation.
  [[nodiscard]] constexpr std::chrono::nanoseconds toChrono() const noexcept
  {
    return std::chrono::nanoseconds {get()};
  }

  /// Number of seconds.
  [[nodiscard]] constexpr float64_t toSeconds() const noexcept
  {
    return std::chrono::duration<float64_t>(toChrono()).count();
  }

public:
  /// Creates a duration out of a frequency in Hertz.
  /// @pre hz != 0.0
  [[nodiscard]] static constexpr Duration fromHertz(float64_t hz) noexcept
  {
    using F64Seconds = std::chrono::duration<float64_t, std::chrono::seconds::period>;
    return {F64Seconds(1.0 / hz)};
  }

  /// Creates a duration out of a frequency in Hertz.
  /// @pre hz != 0.0
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
