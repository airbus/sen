// === uuid.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/base/uuid.h"

// std
#include <cstddef>
#include <iostream>
#include <ostream>
#include <string>

// --------------------------------------------------------------------------------------------------------------------------
// UUID format https://tools.ietf.org/html/rfc4122
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                          time_low                             |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |       time_mid                |         time_hi_and_version   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |clk_seq_hi_res |  clk_seq_low  |         node (0-1)            |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                         node (2-5)                            |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

// --------------------------------------------------------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------------------------------------------------------

namespace
{

constexpr char emptyGuid[37] = "00000000-0000-0000-0000-000000000000";  // NOSONAR
constexpr char guidEncoder[17] = "0123456789abcdef";                    // NOSONAR

[[nodiscard]] constexpr unsigned char hex2char(const char ch) noexcept
{
  if (ch >= '0' && ch <= '9')
  {
    return static_cast<unsigned char>(ch - '0');
  }

  if (ch >= 'a' && ch <= 'f')
  {
    return static_cast<unsigned char>(10 + ch - 'a');  // NOLINT
  }

  if (ch >= 'A' && ch <= 'F')
  {
    return static_cast<unsigned char>(10 + ch - 'A');  // NOLINT
  }

  return 0;
}

[[nodiscard]] constexpr bool isHex(const char ch) noexcept
{
  return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

}  // namespace

namespace sen
{

// --------------------------------------------------------------------------------------------------------------------------
// Uuid
// --------------------------------------------------------------------------------------------------------------------------

bool Uuid::isValid(const std::string& str) noexcept
{
  bool firstDigit = true;
  size_t hasBraces = 0;
  size_t index = 0;

  if (str.empty())
  {
    return false;
  }

  if (str.front() == '{')
  {
    hasBraces = 1;
  }
  if (hasBraces != 0 && str.back() != '}')
  {
    return false;
  }

  for (size_t i = hasBraces; i < str.size() - hasBraces; ++i)
  {
    if (str[i] == '-')
    {
      continue;
    }

    if (index >= byteCount || !isHex(str[i]))
    {
      return false;
    }

    if (firstDigit)
    {
      firstDigit = false;
    }
    else
    {
      index++;
      firstDigit = true;
    }
  }

  return index >= byteCount;
}

Uuid Uuid::fromString(const std::string& str) noexcept
{
  bool firstDigit = true;
  size_t hasBraces = 0;
  size_t index = 0;

  std::array<uint8_t, byteCount> data {0U};  // NOLINT

  if (str.empty())
  {
    return {};
  }

  if (str.front() == '{')
  {
    hasBraces = 1;
  }
  if (hasBraces != 0 && str.back() != '}')
  {
    return {};
  }

  for (size_t i = hasBraces; i < str.size() - hasBraces; ++i)
  {
    if (str[i] == '-')
    {
      continue;
    }

    if (index >= byteCount || !isHex(str[i]))  // NOLINT
    {
      return {};
    }

    if (firstDigit)
    {
      data[index] = static_cast<uint8_t>(hex2char(str[i]) << 4);  // NOLINT
      firstDigit = false;
    }
    else
    {
      data[index] = static_cast<uint8_t>(data[index] | hex2char(str[i]));  // NOLINT
      index++;
      firstDigit = true;
    }
  }

  if (index < byteCount)
  {
    return {};
  }

  return Uuid {data};
}

std::string Uuid::toString() const
{
  std::string str {emptyGuid};

  std::size_t index = 0U;
  for (std::size_t i = 0U; i < 36U;)  // NOLINT
  {
    if (i == 8U || i == 13U || i == 18U || i == 23U)  // NOLINT
    {
      ++i;
      continue;
    }

    str[i] = guidEncoder[bytes_[index] >> 4 & 0x0f];  // NOLINT
    ++i;
    str[i] = guidEncoder[bytes_[index] & 0x0f];  // NOLINT
    ++index;
    ++i;
  }

  return str;
}

// --------------------------------------------------------------------------------------------------------------------------
// non-member functions
// --------------------------------------------------------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& s, const Uuid& id)
{
  s << id.toString();
  return s;
}

}  // namespace sen
