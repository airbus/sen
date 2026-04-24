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
#include <ctime>
#include <iomanip>
#include <ratio>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>

namespace
{

using Clock = std::chrono::system_clock;

constexpr std::size_t microsecondsZeroes = 6U;
constexpr auto format = "%Y-%m-%d %H:%M:%S";

}  // namespace

namespace sen
{

std::string TimeStamp::toUtcString() const
{
  auto chronoTime = timeSinceEpoch_.toChrono();
  auto duration = std::chrono::duration_cast<Clock::duration>(chronoTime);

  std::time_t tt = Clock::to_time_t(Clock::time_point(duration));
  tm timeBuffer {};

  std::stringstream ss;

#ifdef WIN32
  gmtime_s(&timeBuffer, &tt);
#else
  std::ignore = gmtime_r(&tt, &timeBuffer);
#endif
  ss << std::put_time(&timeBuffer, format);

  auto microSecs = std::chrono::duration_cast<std::chrono::microseconds>(chronoTime);
  ss << " " << std::setw(microsecondsZeroes) << std::setfill('0') << microSecs.count() % std::micro::den;

  return ss.str();
}

std::string TimeStamp::toLocalString() const
{
  auto chronoTime = timeSinceEpoch_.toChrono();
  auto duration = std::chrono::duration_cast<Clock::duration>(chronoTime);

  std::time_t tt = Clock::to_time_t(Clock::time_point(duration));
  tm timeBuffer {};

  std::stringstream ss;

#ifdef WIN32
  localtime_s(&timeBuffer, &tt);
#else
  localtime_r(&tt, &timeBuffer);
#endif
  ss << std::put_time(&timeBuffer, format);

  auto microSecs = std::chrono::duration_cast<std::chrono::microseconds>(chronoTime);
  ss << " " << std::setw(microsecondsZeroes) << std::setfill('0') << microSecs.count() % std::micro::den;

  return ss.str();
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
