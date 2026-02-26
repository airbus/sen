// === u8string_util.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_U8STRING_UTIL_H
#define SEN_CORE_BASE_U8STRING_UTIL_H

// std
#include <string>

namespace sen::std_util
{

inline std::string fromU8string(const std::string& s) { return s; }

inline std::string fromU8string(std::string&& s) { return std::move(s); }

#if defined(__cpp_lib_char8_t)
inline std::string fromU8string(const std::u8string& s) { return std::string(s.begin(), s.end()); }
#endif

inline const char* castToCharPtr(const char* cp) { return cp; }

#if defined(__cpp_lib_char8_t)
inline const char* castToCharPtr(const char8_t* cp)
{
  // Safe, as lvalues of type char may be used to access values of other types
  return reinterpret_cast<const char*>(cp);  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

#endif

}  // namespace sen::std_util

#endif  // SEN_CORE_BASE_U8STRING_UTIL_H
