// === integer_compare.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_INTEGER_COMPARE_H
#define SEN_CORE_BASE_INTEGER_COMPARE_H

#if defined(__cpp_lib_integer_comparison_functions)
#  include <utility>
#endif

// std
#include <type_traits>

#if defined(__cpp_lib_integer_comparison_functions)
namespace sen::std_util
{
using std::cmp_equal;
using std::cmp_greater;
using std::cmp_greater_equal;
using std::cmp_less;
using std::cmp_less_equal;
using std::cmp_not_equal;
}  // namespace sen::std_util
#else
namespace sen::std_util
{

// Forward implementation of std::cmp_* from C++20 for compilers that do not yet provide them.
// Impl provided by: https://en.cppreference.com/w/cpp/utility/intcmp.html
// These functions compare integer values without signed/unsigned conversion warnings or UB.

/// Returns `true` if @p t and @p u compare equal, accounting for signed/unsigned differences.
/// @tparam T  Integer type of the left operand.
/// @tparam U  Integer type of the right operand.
template <class T, class U>
// NOLINTNEXTLINE(readability-identifier-naming): name defined by std
constexpr bool cmp_equal(T t, U u) noexcept
{
  if constexpr (std::is_signed_v<T> == std::is_signed_v<U>)
  {
    return t == u;
  }
  else if constexpr (std::is_signed_v<T>)
  {
    // NOLINTNEXTLINE(google-readability-casting): not a c-style cast
    return t >= 0 && std::make_unsigned_t<T>(t) == u;
  }
  else
  {
    // NOLINTNEXTLINE(google-readability-casting): not a c-style cast
    return u >= 0 && std::make_unsigned_t<U>(u) == t;
  }
}

/// Returns `true` if @p t and @p u compare not equal, accounting for signed/unsigned differences.
template <class T, class U>
// NOLINTNEXTLINE(readability-identifier-naming): name defined by std
constexpr bool cmp_not_equal(T t, U u) noexcept
{
  return !cmp_equal(t, u);
}

/// Returns `true` if @p t is strictly less than @p u, accounting for signed/unsigned differences.
template <class T, class U>
// NOLINTNEXTLINE(readability-identifier-naming): name defined by std
constexpr bool cmp_less(T t, U u) noexcept
{
  if constexpr (std::is_signed_v<T> == std::is_signed_v<U>)
  {
    return t < u;
  }
  else if constexpr (std::is_signed_v<T>)
  {
    // NOLINTNEXTLINE(google-readability-casting): not a c-style cast
    return t < 0 || std::make_unsigned_t<T>(t) < u;
  }
  else
  {
    // NOLINTNEXTLINE(google-readability-casting): not a c-style cast
    return u >= 0 && t < std::make_unsigned_t<U>(u);
  }
}

/// Returns `true` if @p t is strictly greater than @p u, accounting for signed/unsigned differences.
template <class T, class U>
// NOLINTNEXTLINE(readability-identifier-naming): name defined by std
constexpr bool cmp_greater(T t, U u) noexcept
{
  return cmp_less(u, t);
}

/// Returns `true` if @p t is less than or equal to @p u, accounting for signed/unsigned differences.
template <class T, class U>
// NOLINTNEXTLINE(readability-identifier-naming): name defined by std
constexpr bool cmp_less_equal(T t, U u) noexcept
{
  return !cmp_less(u, t);
}

/// Returns `true` if @p t is greater than or equal to @p u, accounting for signed/unsigned differences.
template <class T, class U>
// NOLINTNEXTLINE(readability-identifier-naming): name defined by std
constexpr bool cmp_greater_equal(T t, U u) noexcept
{
  return !cmp_less(t, u);
}

}  // namespace sen::std_util
#endif

#endif  // SEN_CORE_BASE_INTEGER_COMPARE_H
