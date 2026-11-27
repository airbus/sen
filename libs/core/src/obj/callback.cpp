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

WorkQueue* CallbackLock::deliveryQueue() const noexcept
{
  // The runner gives an object its queue when it registers it, and until then the object holds the
  // shared default queue, which never runs. So the queue is read here rather than at construction.
  if (data_->caller.has_value())
  {
    const auto caller = data_->caller.value().lock();
    if (!caller)
    {
      return nullptr;
    }

    auto* native = caller->asNativeObject();
    return native != nullptr ? getWorkQueue(native) : data_->queue;
  }

  return data_->queue;
}

void CallbackLock::pushAnswer(sen::std_util::move_only_function<void()>&& answer, bool force) const
{
  if (!isValid())
  {
    return;
  }

  // isValid() has seen the caller alive, but it can expire between that check and this read.
  if (auto* queue = deliveryQueue(); queue != nullptr)
  {
    queue->push(std::move(answer), force);
  }
}

}  // namespace sen::impl
