// === error_reporter.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/lang/error_reporter.h"

// sen
#include "sen/core/lang/code_location.h"

// std
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string>

namespace sen::lang
{

namespace
{

void report(const CodeLocation& loc, const std::string& message, std::size_t width, bool isWarning)
{
  std::size_t lineNumber = 1U;  // lines are counted from 1
  std::size_t lineStart = 0U;
  std::size_t column = 0U;

  if (width == 0U)
  {
    width = 1U;
  }

  for (std::size_t cursor = 0U; cursor < loc.offset; ++cursor)
  {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    if (loc.src[cursor] == '\n')
    {
      column = 0U;
      lineNumber++;
      lineStart = cursor + 1;
    }
    else
    {
      column++;
    }
  }

  const std::string program(loc.src);

  const auto lineEnd = program.find_first_of('\n', lineStart);
  const auto lineWidth = lineEnd == std::string::npos ? std::string::npos : lineEnd - lineStart;
  const auto line = program.substr(lineStart, lineWidth);

  std::cout << (isWarning ? "Warning: " : "Error: ") << message << '\n';
  constexpr auto lineNumberWidth = 6;
  std::cout << std::setfill(' ') << std::setw(lineNumberWidth) << lineNumber << " | " << line << '\n';

  if (column > 0U)
  {
    std::cout << std::string(lineNumberWidth + column + 3U - width, ' ');
  }

  std::cout << std::string(width, '^') << std::endl;
}

ErrorReporter::ReportFunc getDefaultReportFunc()
{
  static ErrorReporter::ReportFunc func = report;
  return func;
}

}  // namespace

void ErrorReporter::report(const CodeLocation& loc, const std::string& message, std::size_t width, bool isWarning)
{
  getDefaultReportFunc()(loc, message, width, isWarning);
}

}  // namespace sen::lang
