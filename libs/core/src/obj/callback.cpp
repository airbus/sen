// === callback.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/obj/callback.h"

// sen
#include "sen/core/base/move_only_function.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/native_object.h"

// std
#include <utility>

namespace sen::impl
{

CallbackBase::CallbackBase(NativeObject* obj)
{
  data_->queue = getWorkQueue(obj);
  data_->caller = obj->weak_from_this();
}

void CallbackLock::pushAnswer(sen::std_util::move_only_function<void()>&& answer, bool force) const
{
  if (isValid())
  {
    data_->queue->push(std::move(answer), force);
  }
}

}  // namespace sen::impl
