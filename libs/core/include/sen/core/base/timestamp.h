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

/// An absolute point in simulation or wall-clock time, stored as nanoseconds since the Unix epoch.
///
/// `TimeStamp` is a thin, trivially-copyable wrapper around a `Duration`. All arithmetic
/// operators follow the same conventions as `std::chrono::time_point`.
class TimeStamp
{
public:
  using ValueType = Duration::ValueType;

public:  // defaults
  constexpr TimeStamp() noexcept = default;

  /// Constructs a `TimeStamp` at the given offset from the Unix epoch (1970-01-01T00:00:00Z).
  /// @param timeSinceEpoch Nanoseconds elapsed since the epoch.
  constexpr explicit TimeStamp(Duration timeSinceEpoch) noexcept: timeSinceEpoch_(timeSinceEpoch) {}
  ~TimeStamp() noexcept = default;

public:  // special members
  constexpr SEN_COPY_CONSTRUCT(TimeStamp) = default;
  constexpr SEN_MOVE_CONSTRUCT(TimeStamp) = default;
  SEN_COPY_ASSIGN(TimeStamp) = default;
  SEN_MOVE_ASSIGN(TimeStamp) = default;

public:  // factories
  /// Parses an ISO 8601 date-time string and returns the corresponding `TimeStamp`.
  /// @param iso8601Time String in ISO 8601 format (e.g. `"2024-01-15T10:30:00Z"`).
  /// @return `Ok(timestamp)` on success, or `Err(message)` if parsing fails.
  static Result<TimeStamp, std::string> make(const std::string_view iso8601Time);

public:  // accessors
  /// Returns the duration elapsed since the Unix epoch (1970-01-01T00:00:00Z).
  /// @return Duration in nanoseconds since the epoch.
  [[nodiscard]] constexpr Duration sinceEpoch() const noexcept { return timeSinceEpoch_; }

public:  // operators
  /// @return `true` if both timestamps represent the same point in time.
  constexpr bool operator==(const TimeStamp& other) const noexcept { return timeSinceEpoch_ == other.timeSinceEpoch_; }

  /// @return `true` if the timestamps represent different points in time.
  constexpr bool operator!=(const TimeStamp& other) const noexcept { return timeSinceEpoch_ != other.timeSinceEpoch_; }

  /// @return `true` if `*this` is strictly earlier than `other`.
  constexpr bool operator<(const TimeStamp& other) const noexcept { return timeSinceEpoch_ < other.timeSinceEpoch_; }

  /// @return `true` if `*this` is earlier than or equal to `other`.
  constexpr bool operator<=(const TimeStamp& other) const noexcept { return timeSinceEpoch_ <= other.timeSinceEpoch_; }

  /// @return `true` if `*this` is strictly later than `other`.
  constexpr bool operator>(const TimeStamp& other) const noexcept { return timeSinceEpoch_ > other.timeSinceEpoch_; }

  /// @return `true` if `*this` is later than or equal to `other`.
  constexpr bool operator>=(const TimeStamp& other) const noexcept { return timeSinceEpoch_ >= other.timeSinceEpoch_; }

  /// Advances `*this` by `other` in place.
  /// @param other Duration to add.
  /// @return Reference to `*this`.
  TimeStamp& operator+=(const Duration& other) noexcept
  {
    timeSinceEpoch_ += other;
    return *this;
  }

  /// Returns a new `TimeStamp` that is `other` later than `*this`.
  /// @param other Duration to add.
  /// @return New `TimeStamp` at `*this + other`.
  constexpr TimeStamp operator+(const Duration& other) const noexcept { return TimeStamp(timeSinceEpoch_ + other); }

  /// Moves `*this` earlier by `other` in place.
  /// @param other Duration to subtract.
  /// @return Reference to `*this`.
  TimeStamp& operator-=(const Duration& other) noexcept
  {
    timeSinceEpoch_ -= other;
    return *this;
  }

  /// Returns a new `TimeStamp` that is `other` earlier than `*this`.
  /// @param other Duration to subtract.
  /// @return New `TimeStamp` at `*this - other`.
  constexpr TimeStamp operator-(const Duration& other) const noexcept { return TimeStamp(timeSinceEpoch_ - other); }

  /// Returns the elapsed time between `*this` and another `TimeStamp`.
  /// @param other Reference point to subtract.
  /// @return Signed `Duration`; negative if `other` is later than `*this`.
  constexpr Duration operator-(const TimeStamp& other) const noexcept { return sinceEpoch() - other.sinceEpoch(); }

public:
  /// Returns a human-readable UTC string (e.g. `"2024-01-15 10:30:00.123456789 UTC"`).
  /// @return Formatted UTC timestamp string.
  [[nodiscard]] std::string toUtcString() const;

  /// Returns a human-readable local-time string using the system time zone.
  /// @return Formatted local timestamp string.
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
