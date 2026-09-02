// === timestamp.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/base/timestamp.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/result.h"

// extra cstd
// NOLINTNEXTLINE(hicpp-deprecated-headers,modernize-deprecated-headers)
#include "time.h"  // gmtime_r and localtime_r are not part of <ctime>

// std
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <ratio>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>

namespace
{

constexpr std::size_t microsecondsZeroes = 6U;
constexpr std::size_t nanosecondsZeroes = 9U;
constexpr auto format = "%Y-%m-%d %H:%M:%S";
constexpr auto rfc3339Format = "%Y-%m-%dT%H:%M:%S";
constexpr int64_t nanosPerSecond = 1000000000;

/// Floor-divides the total ns count into a (wholeSeconds, subSecondFraction) pair so the
/// sub-second part is always in [0, fractionDen). Plain `%` would yield a negative remainder
/// for pre-epoch timestamps, and `Clock::to_time_t` truncates toward zero rather than toward
/// -infinity; either alone misaligns the printed second from the printed fraction.
struct DecomposedTimestamp
{
  std::time_t wholeSeconds;
  int64_t fraction;  // 0 <= fraction < fractionDen
};

[[nodiscard]] DecomposedTimestamp decompose(sen::Duration timeSinceEpoch, int64_t fractionDen) noexcept
{
  const int64_t totalNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(timeSinceEpoch.toChrono()).count();
  int64_t seconds = totalNanos / nanosPerSecond;
  int64_t remainderNanos = totalNanos % nanosPerSecond;
  if (remainderNanos < 0)
  {
    seconds -= 1;
    remainderNanos += nanosPerSecond;
  }
  const int64_t nanosPerFraction = nanosPerSecond / fractionDen;
  return {static_cast<std::time_t>(seconds), remainderNanos / nanosPerFraction};
}

enum class TimeZoneKind
{
  utc,
  local,
};

[[nodiscard]] std::string formatTimestamp(sen::Duration timeSinceEpoch,
                                          const char* timeFormat,
                                          int fractionalWidth,
                                          int64_t fractionDen,
                                          char fractionSeparator,
                                          std::string_view trailingSuffix,
                                          TimeZoneKind zone) noexcept
{
  const auto [wholeSeconds, fraction] = decompose(timeSinceEpoch, fractionDen);
  tm timeBuffer {};
#ifdef WIN32
  if (zone == TimeZoneKind::utc)
  {
    gmtime_s(&timeBuffer, &wholeSeconds);
  }
  else
  {
    localtime_s(&timeBuffer, &wholeSeconds);
  }
#else
  if (zone == TimeZoneKind::utc)
  {
    std::ignore = gmtime_r(&wholeSeconds, &timeBuffer);
  }
  else
  {
    std::ignore = localtime_r(&wholeSeconds, &timeBuffer);
  }
#endif
  std::stringstream ss;
  ss << std::put_time(&timeBuffer, timeFormat);
  ss << fractionSeparator << std::setw(fractionalWidth) << std::setfill('0') << fraction;
  if (!trailingSuffix.empty())
  {
    ss << trailingSuffix;
  }
  return ss.str();
}

}  // namespace

namespace sen
{

std::string TimeStamp::toUtcString() const
{
  return formatTimestamp(
    timeSinceEpoch_, format, static_cast<int>(microsecondsZeroes), std::micro::den, ' ', {}, TimeZoneKind::utc);
}

std::string TimeStamp::toUtcStringNs() const
{
  return formatTimestamp(
    timeSinceEpoch_, rfc3339Format, static_cast<int>(nanosecondsZeroes), std::nano::den, '.', "Z", TimeZoneKind::utc);
}

std::string TimeStamp::toLocalString() const
{
  return formatTimestamp(
    timeSinceEpoch_, format, static_cast<int>(microsecondsZeroes), std::micro::den, ' ', {}, TimeZoneKind::local);
}

Result<TimeStamp, std::string> TimeStamp::make(const std::string_view iso8601Time)
{
  std::tm t = {};
  std::istringstream ss(iso8601Time.data());
  ss >> std::get_time(&t, format);

  if (ss.fail())
  {
    std::string reason;
    reason.append("error parsing the timestamp '");
    reason.append(iso8601Time);
    reason.append("'. Use the following format '");
    reason.append(format);
    reason.append("'");
    return Err(reason);
  }

#ifdef _WIN32
  _tzset();
  long timezone {0};
  _get_timezone(&timezone);
#endif

  return Ok(TimeStamp(Duration(std::chrono::system_clock::from_time_t(std::mktime(&t) - timezone).time_since_epoch())));
}

}  // namespace sen
