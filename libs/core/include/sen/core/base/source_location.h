// === source_location.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_SOURCE_LOCATION_H
#define SEN_CORE_BASE_SOURCE_LOCATION_H

// std
#include <cstddef>
#include <string_view>

namespace sen::impl
{

/// Returns the index of the char following the last '/' or '\'.
/// This is meant to be only used at compile-time.
constexpr size_t getFilenameOffset(const std::string_view str) noexcept
{
  const size_t pos = str.find_last_of("/\\");
  return pos == std::string_view::npos ? 0 : pos + 1;
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

#ifdef WIN32
#  define SEN_PRETTY_FUNCTION_NAME __FUNCSIG__
#else
#  define SEN_PRETTY_FUNCTION_NAME __PRETTY_FUNCTION__
#endif

/// Convenience macro including the source location info.
/// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SEN_SL()                                                                                                       \
  ::sen::SourceLocation                                                                                                \
  {                                                                                                                    \
    std::string_view {&__FILE__[SEN_SL_CONSTEXPR_VAL(::sen::impl::getFilenameOffset(__FILE__))]}, __LINE__,            \
      std::string_view(SEN_PRETTY_FUNCTION_NAME)                                                                       \
  }

/// @}

}  // namespace sen

#endif  // SEN_CORE_BASE_SOURCE_LOCATION_H
