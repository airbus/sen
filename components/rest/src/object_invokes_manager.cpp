// === object_invokes_manager.cpp ======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "object_invokes_manager.h"

// component
#include "types.h"

// generated code
#include "stl/types.stl.h"

// sen
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"

// std
#include <chrono>
#include <optional>

namespace sen::components::rest
{

//--------------------------------------------------------------------------------------------------------------
// Invokes
//--------------------------------------------------------------------------------------------------------------

Invoke ObjectInvokesManager::newInvoke(const InterestName& interest)
{
  InvokeId invokeId = ++lastInvokeId_;

  Invoke invoke {
    invokeId,
    InvokeStatus::pending,
  };
  invokes_[invokeId] = invoke;

  notify(Notification {
    NotificationType::invoke,
    interest,
    sen::TimeStamp {std::chrono::system_clock::now().time_since_epoch()},
    toJson(invoke),
  });

  return invoke;
}

bool ObjectInvokesManager::updateInvoke(const InterestName& interest,
                                        const InvokeId& id,
                                        const MethodResult<Var>& result)
{
  if (auto invokeIt = invokes_.find(id); invokeIt != invokes_.cend())
  {
    invokeIt->second.status = result.isOk() ? InvokeStatus::finished : InvokeStatus::failed;
    invokeIt->second.result = result;

    notify(Notification {
      NotificationType::invoke,
      interest,
      sen::TimeStamp {std::chrono::system_clock::now().time_since_epoch()},
      toJson(invokeIt->second),
    });
    return true;
  }

  return false;
}

void ObjectInvokesManager::releaseInvoke(const InvokeId& id) { invokes_.erase(id); }

void ObjectInvokesManager::releaseAllInvokes() { invokes_.clear(); }

std::optional<Invoke> ObjectInvokesManager::findInvoke(const InvokeId& id)
{
  auto invokeIt = invokes_.find(id);
  if (invokeIt == invokes_.cend())
  {
    return std::nullopt;
  }
  return invokeIt->second;
}

}  // namespace sen::components::rest
