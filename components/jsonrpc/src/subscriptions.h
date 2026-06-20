// === subscriptions.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_JSONRPC_SUBSCRIPTIONS_H
#define SEN_COMPONENTS_JSONRPC_SUBSCRIPTIONS_H

// local
#include "dispatcher.h"
#include "messages.h"

// sen
#include "sen/core/base/result.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/obj/object.h"

// nlohmann
#include "nlohmann/json.hpp"

// std
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

namespace sen::kernel
{
class KernelApi;
}

namespace sen::components::jsonrpc
{

/// Wires kernel guards for each requested member that exists on the instance's class and isn't
/// already wired. `serverWeak` is the owning Server's `weak_from_this()`; the wired lambdas
/// lock it to keep `subs` (which lives inside the Server) valid for the body.
void rewireRequestedSubscriptions(Dispatcher& dispatcher,
                                  sen::kernel::KernelApi& api,
                                  ConnectionId connId,
                                  const std::string& interestName,
                                  std::shared_ptr<sen::Object> object,
                                  ObjectSubs& subs,
                                  std::weak_ptr<sen::Object> serverWeak);

/// Folds the interest's `subscribeBlock` into the per-object request set.
void applySubscribeBlockToObjectSubs(const InterestSubscribeBlock& block, const sen::Object& object, ObjectSubs& subs);

/// Seeds initial values for each property into the pending bundle.
void seedSnapshotsForProperties(Dispatcher& dispatcher,
                                ConnectionId connId,
                                const std::string& interestName,
                                const std::string& objectName,
                                ObjectSubs& subs,
                                const sen::Object& object,
                                const std::unordered_set<std::string>& propertyNames);

}  // namespace sen::components::jsonrpc

#endif  // SEN_COMPONENTS_JSONRPC_SUBSCRIPTIONS_H
