// === env_substitution.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_ENV_SUBSTITUTION_H
#define SEN_LIBS_KERNEL_SRC_ENV_SUBSTITUTION_H

// std
#include <string>

namespace sen::kernel::impl
{

/// Expands every @env(NAME) and @env(NAME, DEFAULT) found in configuration text.
/// Throws when a variable is unset and carries no default. A backslash escapes the
/// pattern: an odd number of them leaves the text in place and reads no variable.
[[nodiscard]] std::string replaceEnvPattern(const std::string& content);

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_ENV_SUBSTITUTION_H
