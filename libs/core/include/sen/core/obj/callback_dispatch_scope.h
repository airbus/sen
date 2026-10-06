// === callback_dispatch_scope.h =======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_CALLBACK_DISPATCH_SCOPE_H
#define SEN_CORE_OBJ_CALLBACK_DISPATCH_SCOPE_H

// sen
#include "sen/core/base/compiler_macros.h"

namespace sen::impl
{

[[nodiscard]] bool isInsideCallbackDispatch() noexcept;

/// RAII class flagging that the current thread is executing a user callback
class CallbackDispatchScope
{
  SEN_MOVE_ONLY(CallbackDispatchScope)

public:
  CallbackDispatchScope() noexcept;
  ~CallbackDispatchScope();
};

}  // namespace sen::impl

#endif
