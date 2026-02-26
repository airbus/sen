// === timestamp.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_TIMESTAMP_H
#define SEN_CORE_BASE_TIMESTAMP_H

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/result.h"

// std
#include <string>

namespace sen
{

/// \addtogroup util
/// @{

/// A point in time.
class TimeStamp
{
public:
  using ValueType = Duration::ValueType;

public:  // defaults
  constexpr TimeStamp() noexcept = default;
  constexpr explicit TimeStamp(Duration timeSinceEpoch) noexcept: timeSinceEpoch_(timeSinceEpoch) {}
  ~TimeStamp() noexcept = default;

public:  // special members
  constexpr SEN_COPY_CONSTRUCT(TimeStamp) = default;
  constexpr SEN_MOVE_CONSTRUCT(TimeStamp) = default;
  SEN_COPY_ASSIGN(TimeStamp) = default;
  SEN_MOVE_ASSIGN(TimeStamp) = default;

public:  // factories
  static Result<TimeStamp, std::string> make(const std::string_view iso8601Time);

public:  // accessors
  /// Time passed since 1 January 1970 UTC.
  [[nodiscard]] constexpr Duration sinceEpoch() const noexcept { return timeSinceEpoch_; }

public:  // operators
  /// Compares two Timestamps to determine if they are equal.
  constexpr bool operator==(const TimeStamp& other) const noexcept { return timeSinceEpoch_ == other.timeSinceEpoch_; }

  /// Compares two Timestamps to determine if they are not equal.
  constexpr bool operator!=(const TimeStamp& other) const noexcept { return timeSinceEpoch_ != other.timeSinceEpoch_; }

  /// True if *this is less than other.
  constexpr bool operator<(const TimeStamp& other) const noexcept { return timeSinceEpoch_ < other.timeSinceEpoch_; }

  /// True if *this is less or equal than other.
  constexpr bool operator<=(const TimeStamp& other) const noexcept { return timeSinceEpoch_ <= other.timeSinceEpoch_; }

  /// True if *this is greater than other.
  constexpr bool operator>(const TimeStamp& other) const noexcept { return timeSinceEpoch_ > other.timeSinceEpoch_; }

  /// True if *this is greater or equal than other.
  constexpr bool operator>=(const TimeStamp& other) const noexcept { return timeSinceEpoch_ >= other.timeSinceEpoch_; }

  /// Adds other to *this and returns *this.
  TimeStamp& operator+=(const Duration& other) noexcept
  {
    timeSinceEpoch_ += other;
    return *this;
  }

  /// Returns a TimeStamp of *this + other.
  constexpr TimeStamp operator+(const Duration& other) const noexcept { return TimeStamp(timeSinceEpoch_ + other); }

  /// Removes other from *this and returns *this.
  TimeStamp& operator-=(const Duration& other) noexcept
  {
    timeSinceEpoch_ -= other;
    return *this;
  }

  /// Returns a TimeStamp of *this - other.
  constexpr TimeStamp operator-(const Duration& other) const noexcept { return TimeStamp(timeSinceEpoch_ - other); }

  /// Returns a Duration of *this - other.
  constexpr Duration operator-(const TimeStamp& other) const noexcept { return sinceEpoch() - other.sinceEpoch(); }

public:
  /// UTC string representation of this time point.
  [[nodiscard]] std::string toUtcString() const;

  /// Local string representation of this time point.
  [[nodiscard]] std::string toLocalString() const;

private:
  Duration timeSinceEpoch_ {};
};

template <>
struct ShouldBePassedByValue<TimeStamp>: std::true_type
{
};

static_assert(sizeof(TimeStamp) == 8, "TimeStamp should be 8 bytes");
static_assert(std::is_trivially_copyable_v<TimeStamp>);

/// @}

}  // namespace sen

#endif  // SEN_CORE_BASE_TIMESTAMP_H
