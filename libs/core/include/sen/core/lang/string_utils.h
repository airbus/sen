// === string_utils.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_LANG_STRING_UTILS_H
#define SEN_CORE_LANG_STRING_UTILS_H

// std
#include <string>
#include <string_view>

namespace sen
{

/// Drops '-' and '_', and uppercases the first character (example: "foo_bar" -> "Foobar").
[[nodiscard]] std::string capitalizeAndRemoveSeparators(std::string_view str);

/// Splits on underscores and capitalizes (example: "foo_bar" -> "FooBar").
[[nodiscard]] std::string snakeCaseToPascalCase(std::string_view str);

/// Converts PascalCase to snake_case.
[[nodiscard]] std::string pascalCaseToSnakeCase(std::string_view str);

/// True when str starts with uppercase and contains no whitespace or underscores.
[[nodiscard]] bool isCapitalizedAndNoSeparators(std::string_view str);

}  // namespace sen

#endif  // SEN_CORE_LANG_STRING_UTILS_H
