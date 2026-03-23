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

/// Identity conversion â€” returns @p s unchanged (no-op for `std::string` inputs).
/// @param s  Source string.
/// @return A copy of @p s as a `std::string`.
inline std::string fromU8string(const std::string& s) { return s; }

/// Move-conversion â€” moves @p s into the result (no-op for `std::string` inputs).
/// @param s  Source string to move from.
/// @return @p s moved into a `std::string`.
inline std::string fromU8string(std::string&& s) { return std::move(s); }

#if defined(__cpp_lib_char8_t)
/// Converts a `std::u8string` to a `std::string` by reinterpreting the UTF-8 code units as `char`.
/// @param s  Source UTF-8 string.
/// @return A `std::string` with the same byte content.
inline std::string fromU8string(const std::u8string& s) { return std::string(s.begin(), s.end()); }
#endif

/// Identity cast â€” returns a `const char*` unchanged.
/// @param cp  Pointer to a null-terminated character sequence.
/// @return @p cp unchanged.
inline const char* castToCharPtr(const char* cp) { return cp; }

#if defined(__cpp_lib_char8_t)
/// Reinterprets a `const char8_t*` as `const char*`.
/// Safe because `char` may alias any byte type per the C++ standard.
/// @param cp  Pointer to a null-terminated UTF-8 code-unit sequence.
/// @return @p cp reinterpreted as `const char*`.
inline const char* castToCharPtr(const char8_t* cp)
{
  // Safe, as lvalues of type char may be used to access values of other types
  return reinterpret_cast<const char*>(cp);  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

#endif

}  // namespace sen::std_util

#endif  // SEN_CORE_BASE_U8STRING_UTIL_H
