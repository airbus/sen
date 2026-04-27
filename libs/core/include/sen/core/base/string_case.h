// === string_case.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_STRING_CASE_H
#define SEN_CORE_BASE_STRING_CASE_H

// std
#include <string>
#include <string_view>

namespace sen
{

/// Flattens an identifier by dropping '-' and '_' separators and uppercases the first
/// character of the result. On snake_case input like "foo_bar" returns "Foobar", not
/// "FooBar": internal word boundaries are not re-capitalized. Use toPascalCase for the
/// word-splitting behavior. Empty input returns empty.
[[nodiscard]] std::string toUpperCamelCase(std::string_view str);

/// Converts a snake_case identifier to PascalCase by splitting on underscores and capitalizing
/// the first character of each word. "foo_bar" returns "FooBar"; "option_type" returns
/// "OptionType". Non-underscore separators are copied through. Prefer this over
/// toUpperCamelCase when the input is known to be snake_case.
[[nodiscard]] std::string toPascalCase(std::string_view str);

/// Converts an UpperCamelCase identifier to snake_case: each uppercase letter not at the start
/// is preceded by an underscore and lowercased. Non-letters are copied through.
[[nodiscard]] std::string toSnakeCase(std::string_view str);

/// Returns true when str starts with an uppercase letter and contains no whitespace or
/// underscores. Empty input is treated as valid.
[[nodiscard]] bool isUpperCamelCase(std::string_view str);

}  // namespace sen

#endif  // SEN_CORE_BASE_STRING_CASE_H
