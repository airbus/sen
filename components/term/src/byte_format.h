// === byte_format.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_BYTE_FORMAT_H
#define SEN_COMPONENTS_TERM_SRC_BYTE_FORMAT_H

// sen
#include "sen/core/base/checked_conversions.h"

// std
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace sen::components::term::byte_format
{

constexpr double bytesPerKilobyte = 1024.0;
constexpr double bytesPerMegabyte = bytesPerKilobyte * bytesPerKilobyte;
constexpr double bytesPerGigabyte = bytesPerKilobyte * bytesPerMegabyte;

/// Format a byte count as a human-readable size with adaptive unit (B / KB / MB / GB).
inline std::string formatBytes(std::size_t bytes)
{
  using sen::std_util::checkedConversion;
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1);
  if (checkedConversion<double>(bytes) < bytesPerKilobyte)
  {
    oss << bytes << " B";
  }
  else if (checkedConversion<double>(bytes) < bytesPerMegabyte)
  {
    oss << checkedConversion<double>(bytes) / bytesPerKilobyte << " KB";
  }
  else if (checkedConversion<double>(bytes) < bytesPerGigabyte)
  {
    oss << checkedConversion<double>(bytes) / bytesPerMegabyte << " MB";
  }
  else
  {
    oss << checkedConversion<double>(bytes) / bytesPerGigabyte << " GB";
  }
  return oss.str();
}

/// Format a byte rate as a compact human-readable throughput (B/s, KB/s, MB/s).
inline std::string formatRate(double bytesPerSec)
{
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1);
  if (bytesPerSec < bytesPerKilobyte)
  {
    oss << bytesPerSec << " B/s";
  }
  else if (bytesPerSec < bytesPerMegabyte)
  {
    oss << bytesPerSec / bytesPerKilobyte << " KB/s";
  }
  else
  {
    oss << bytesPerSec / bytesPerMegabyte << " MB/s";
  }
  return oss.str();
}

}  // namespace sen::components::term::byte_format

#endif
