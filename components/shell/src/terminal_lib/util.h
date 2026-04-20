// === util.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_SHELL_SRC_TERMINAL_LIB_UTIL_H
#define SEN_COMPONENTS_SHELL_SRC_TERMINAL_LIB_UTIL_H

#include "sen/core/meta/method.h"
#include "sen/core/meta/var.h"

// asio
#include <asio/ip/host_name.hpp>

// std
#include <cstdarg>
#include <string>

namespace sen::components::shell
{

inline void trimRight(std::string& str, std::string_view whitespaces = " ");

inline void trimLeft(std::string& str, std::string_view whitespaces = " ")
{
  str.erase(0, str.find_first_not_of(whitespaces));
}

inline void trim(std::string& str, std::string_view whitespaces = " ")
{
  trimRight(str, whitespaces);
  trimLeft(str, whitespaces);
}

[[nodiscard]] inline std::string fromFormat(const char* fmt, ...);  // NOLINT

[[nodiscard]] inline std::string fromArgList(const char* fmt, va_list args);

void parseArgv(const Method* method, std::string_view buffer, VarList& result);

[[nodiscard]] inline std::string getHostName() { return asio::ip::host_name(); }

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

#ifndef WIN32
inline int _vscprintf(const char* format, va_list args)  // NOLINT
{
  int retVal;
  va_list argCopy;  // NOLINT
  va_copy(argCopy, args);
  retVal = vsnprintf(nullptr, 0, format, argCopy);
  va_end(argCopy);
  return retVal;
}

#endif

inline const char* formattedArgList(const char* fmt, va_list args)
{
  if (fmt == nullptr)
  {
    return nullptr;
  }

  size_t bufLen = _vscprintf(fmt, args) + 1U;
  auto* buffer = new char[bufLen];  // NOLINT

  if (auto ret = vsnprintf(buffer, bufLen, fmt, args); ret >= 0)
  {
    *(buffer + ret) = '\0';  // NOLINT
  }

  return buffer;
}

inline void trimRight(std::string& str, std::string_view whitespaces)
{
  std::string::size_type pos = str.find_last_not_of(whitespaces);
  if (pos != std::string::npos)
  {
    str.erase(pos + 1);
  }
  else
  {
    str.clear();
  }
}

inline std::string fromArgList(const char* fmt, va_list args)
{
  const char* buffer = formattedArgList(fmt, args);
  if (buffer == nullptr)
  {
    return {};
  }

  std::string result(buffer);
  delete[] buffer;  // NOLINT
  return result;
}

inline std::string fromFormat(const char* fmt, ...)  // NOLINT
{
  va_list args;  // NOLINT
  va_start(args, fmt);
  std::string result = fromArgList(fmt, args);
  va_end(args);
  return result;
}

}  // namespace sen::components::shell

#endif  // SEN_COMPONENTS_SHELL_SRC_TERMINAL_LIB_UTIL_H
