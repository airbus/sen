// === custom_type.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/custom_type.h"

// sen
#include "sen/core/meta/type.h"

namespace sen
{

CustomType::CustomType(MemberHash hash) noexcept: Type(hash) {}

}  // namespace sen
