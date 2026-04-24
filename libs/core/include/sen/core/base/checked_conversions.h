// === checked_conversions.h ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_CHECKED_CONVERSIONS_H
#define SEN_CORE_BASE_CHECKED_CONVERSIONS_H

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/integer_compare.h"
#include "sen/core/base/numbers.h"

// std
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace sen::std_util
{

struct ReportPolicyAssertion
{
  static void report(const std::string_view message)
  {
    std::cerr << message.data() << std::endl;
    SEN_ASSERT(false);
  }
};

struct ReportPolicyTrace
{
  static void report(std::string_view message) { trace(std::string(message)); }
};

struct ReportPolicyLog
{
  static void report(std::string_view message);
};

struct ReportPolicyIgnore
{
  static void report(std::string_view message) { std::ignore = message; }
};

namespace impl
{
/// Safely convert between two different integral types. If the value stored in
/// \p from cannot be represented in \p ToType, the value is truncated.
///
/// @tparam ToType: output type
/// @tparam FromType: input type
/// @tparam ReportPolicy: policy type that specifies how truncation should be reported
///
/// @param[in] from: value that should be converted to ToType
///
/// @return from value represented as ToType, possibly truncated within the range of ToType
template <typename ToType, typename FromType, typename ReportPolicy = ReportPolicyIgnore>
ToType conversionImpl(FromType from)
{
  if constexpr (std::is_same_v<ToType, bool>)
  {
    return static_cast<bool>(from);
  }
  else if constexpr (std::is_floating_point_v<ToType> || std::is_floating_point_v<FromType>)
  {
    if constexpr (std::is_same_v<FromType, ToType>)
    {
      return from;
    }
    else
    {
      if (std::isless(static_cast<float64_t>(from), static_cast<float64_t>(std::numeric_limits<ToType>::lowest())))
      {
        ReportPolicy::report("Needed to truncate `from` as it's value was to small for ToType.");
        return std::numeric_limits<ToType>::lowest();
      }

      if (std::isgreater(static_cast<float64_t>(from), static_cast<float64_t>(std::numeric_limits<ToType>::max())))
      {
        ReportPolicy::report("Needed to truncate `from` as it's value was to big for ToType.");
        return std::numeric_limits<ToType>::max();
      }

      return static_cast<ToType>(from);
    }
  }
  else
  {
    if (sen::std_util::cmp_less(from, std::numeric_limits<ToType>::lowest()))
    {
      ReportPolicy::report("Needed to truncate `from` as it's value was to small for ToType.");
      return std::numeric_limits<ToType>::lowest();
    }

    if (sen::std_util::cmp_greater(from, std::numeric_limits<ToType>::max()))
    {
      ReportPolicy::report("Needed to truncate `from` as it's value was to big for ToType.");
      return std::numeric_limits<ToType>::max();
    }

    return static_cast<ToType>(from);
  }
}

}  // namespace impl

/// Safely convert between two different integral types. If the value stored in
/// \p from cannot be represented in \p ToType, the value is truncated.
///
/// note: by default, this function asserts when a truncation is necessary
///
/// @tparam ToType: output type
/// @tparam ReportPolicy: policy type that specifies how truncation should be reported
/// @tparam FromType: input type
///
/// @param[in] from: value that should be converted to ToType
///
/// @return from value represented as ToType, possibly truncated within the range of ToType
template <typename ToType, typename ReportPolicy = ReportPolicyAssertion, typename FromType>
[[nodiscard]] ToType checkedConversion(FromType from)
{
  return impl::conversionImpl<ToType, FromType, ReportPolicy>(from);
}

/// Safely convert between two different integral types. If the value stored in
/// \p from cannot be represented in \p ToType, the value is truncated.
///
/// note: by default, this function ignores truncation errors
///
/// @tparam ToType: output type
/// @tparam ReportPolicy: policy type that specifies how truncation should be reported
/// @tparam FromType: input type
///
/// @param[in] from: value that should be converted to ToType
///
/// @return from value represented as ToType, possibly truncated within the range of ToType
template <typename ToType, typename ReportPolicy = ReportPolicyIgnore, typename FromType>
[[nodiscard]] ToType ignoredLossyConversion(FromType from)
{
  return impl::conversionImpl<ToType, FromType, ReportPolicy>(from);
}

}  // namespace sen::std_util

#endif  // SEN_CORE_BASE_CHECKED_CONVERSIONS_H
