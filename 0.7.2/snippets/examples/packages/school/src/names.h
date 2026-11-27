// === names.h =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_EXAMPLES_PACKAGES_SCHOOL_SRC_NAMES_H
#define SEN_EXAMPLES_PACKAGES_SCHOOL_SRC_NAMES_H

// std
#include <string>
#include <tuple>

namespace school
{

// Returns a randomly generated {firstName, surName, fullName} triple.
// The full name is firstName + surName with the first character lowercased
// so it can be used as an object name in Sen.
[[nodiscard]] std::tuple<std::string, std::string, std::string> makeName();

}  // namespace school

#endif  // SEN_EXAMPLES_PACKAGES_SCHOOL_SRC_NAMES_H
