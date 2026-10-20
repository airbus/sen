// === base64.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_BASE64_H
#define SEN_CORE_BASE_BASE64_H

// std
#include <string>
#include <string_view>

namespace sen::std_util
{

/// RFC 4648 base64, with padding.
///
/// Here rather than in the one component that first needed it: the encoding is the same everywhere,
/// and an HTTP Authorization header wants it as much as a terminal escape does.
[[nodiscard]] std::string base64Encode(std::string_view input);

}  // namespace sen::std_util

#endif  // SEN_CORE_BASE_BASE64_H
