// === error_reporter.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_LANG_ERROR_REPORTER_H
#define SEN_CORE_LANG_ERROR_REPORTER_H

#include "code_location.h"

// std
#include <cstddef>
#include <functional>
#include <string>

namespace sen::lang
{

/// \addtogroup lang
/// @{

/// Utility class for reporting errors in the source code
class ErrorReporter
{
public:
  /// A function that reports the error to some sink.
  using ReportFunc = std::function<void(const CodeLocation&, const std::string&, std::size_t, bool isWarning)>;

  /// Reports an error using the defined function.
  /// @param loc: the place where the error is located.
  /// @param message: human-readable error message.
  /// @param width: (optional) width of the error (in characters).
  /// @param isWarning (optional) true if we should mark the problem as a warning.
  static void report(const CodeLocation& loc,
                     const std::string& message,
                     std::size_t width = 0U,
                     bool isWarning = false);
};

/// @}

}  // namespace sen::lang

#endif  // SEN_CORE_LANG_ERROR_REPORTER_H
