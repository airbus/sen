// === callback_dispatch_scope.cpp =====================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/obj/callback_dispatch_scope.h"

namespace sen::impl
{

thread_local int callbackDispatchDepth = 0;

bool isInsideCallbackDispatch() noexcept { return callbackDispatchDepth > 0; }

CallbackDispatchScope::CallbackDispatchScope() noexcept { ++callbackDispatchDepth; }

CallbackDispatchScope::~CallbackDispatchScope() { --callbackDispatchDepth; }

}  // namespace sen::impl
