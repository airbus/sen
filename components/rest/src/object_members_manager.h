// === object_members_manager.h ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_SRC_OBJECT_MEMBERS_MANAGER_H
#define SEN_COMPONENTS_REST_SRC_OBJECT_MEMBERS_MANAGER_H

// component
#include "locators.h"
#include "notifications.h"
#include "types.h"

// generated code
#include "stl/options.stl.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/type.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/core/obj/object.h"
#include "sen/kernel/component_api.h"

// std
#include <memory>
#include <unordered_map>
#include <vector>

namespace sen::components::rest
{

/// ObjectMembersManager class allows client to subscribe or unsubscribe
/// to property or event updates of Sen objects.
/// It extends the Notifier class to provide notification capabilities when
/// invocations are updated.
/// Note: This class is not reentrant
class ObjectMembersManager: public Notifier
{
  SEN_NOCOPY_NOMOVE(ObjectMembersManager)

public:
  ObjectMembersManager() = default;
  ~ObjectMembersManager() = default;

public:
  /// Subscribes to property updates of a given Sen object.
  bool subscribeProperty(const sen::kernel::KernelApi& kernelApi,
                         const InterestName& interestName,
                         std::shared_ptr<sen::Object> object,
                         const PropertyLocator& propertyLocator,
                         const SubscriptionOptions& options);

  /// Subscribes to event updates of a given Sen object.
  bool subscribeEvent(const sen::kernel::KernelApi& kernelApi,
                      const InterestName& interestName,
                      std::shared_ptr<sen::Object> object,
                      const EventLocator& eventLocator);

  /// Unsubscribes from property updates of a Sen object.
  bool unsubscribeProperty(const sen::ObjectId& objectId, const MemberHash& propertyId);

  /// Unsubscribes from event updates of a Sen object.
  bool unsubscribeEvent(const sen::ObjectId& objectId, const MemberHash& eventId);

  /// Unsubscribes from all events of an object.
  /// Members can be properties or events.
  void unsubscribeAll(const sen::ObjectId& objectId);

  /// Checks whether a client is subscribed to member updates of a Sen object.
  /// Members can be properties or events.
  bool isSubscribedTo(const sen::ObjectId& objectId, const MemberHash& memberId);

  /// Returns a list of object property ids the client is subscribed to
  std::vector<sen::MemberHash> getPropertyIds(const sen::ObjectId& objectId);

  /// Returns a list of object event ids the client is subscribed to
  std::vector<sen::MemberHash> getEventIds(const sen::ObjectId& objectId);

private:
  std::unordered_map<ObjectId, std::unordered_map<sen::MemberHash, ConnectionGuard>> properties_;
  std::unordered_map<ObjectId, std::unordered_map<sen::MemberHash, ConnectionGuard>> events_;
};

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_OBJECT_MEMBERS_MANAGER_H
