// === source_location.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_SOURCE_LOCATION_H
#define SEN_CORE_BASE_SOURCE_LOCATION_H

#include <string_view>
#include <tuple>  // for std::ignore

namespace sen::impl
{

/// Returns the index of the char following the last '/' or '\'.
/// This is meant to be only used at compile-time.
template <typename T, size_t s>
constexpr size_t getFilenameOffset(const T (&str)[s], size_t i = s - 1) noexcept
{
  return (str[i] == '/' || str[i] == '\\') ? i + 1 : i > 0 ? getFilenameOffset(str, i - 1) : 0;
}

/// Base case, for strings of 1 character.
template <typename T>
constexpr size_t getFilenameOffset(T (&str)[1]) noexcept
{
  std::ignore = str;
  return 0;
}

/// Workaround to force compile-time evaluation in all platforms.
template <typename T, T v>
struct ConstexprValue
{
  static constexpr const T value = v;
};

/// To force compile-time evaluation of exp.
/// NOLINTNEXTLINE
#define SEN_SL_CONSTEXPR_VAL(exp) ::sen::impl::ConstexprValue<decltype(exp), exp>::value

}  // namespace sen::impl

namespace sen
{

/// \addtogroup util
/// @{

/// Represents a location in source code.
struct SourceLocation
{
  std::string_view fileName {"file name not set"};
  int lineNumber {-1};
  std::string_view functionName {"function name not set"};
};

/// Convenience macro including the source location info.
/// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SEN_SL()                                                                                                       \
  ::sen::SourceLocation                                                                                                \
  {                                                                                                                    \
    std::string_view {&__FILE__[SEN_SL_CONSTEXPR_VAL(::sen::impl::getFilenameOffset(__FILE__))]}, __LINE__,            \
      std::string_view(__FUNCTION__)                                                                                   \
  }

/// @}

}  // namespace sen

#endif  // SEN_CORE_BASE_SOURCE_LOCATION_H
