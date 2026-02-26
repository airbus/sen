// === client_session.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_SRC_CLIENT_SESSION_H
#define SEN_COMPONENTS_REST_SRC_CLIENT_SESSION_H

// component
#include "locators.h"
#include "notifications.h"
#include "object_interests_manager.h"
#include "object_invokes_manager.h"
#include "object_members_manager.h"
#include "types.h"

// generated code
#include "stl/options.stl.h"
#include "stl/types.stl.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/result.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/object.h"
#include "sen/kernel/component_api.h"

// std
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace sen::components::rest
{

enum class NotifierType
{
  interestsNotifier,
  invokesNotifier,
  membersNotifier
};

/// ClientSession manages a single client session state and interactions
/// with the Sen resources allocated for the client. It provides centralized
/// access to interests, invokes, events and properties.
class ClientSession final
{
  SEN_NOCOPY_NOMOVE(ClientSession)

public:
  explicit ClientSession(std::string clientId): clientId_(std::move(clientId)) {}

  ~ClientSession();

public:
  /// Encodes the session client ID into a HTTP-friendly token.
  [[nodiscard]] std::string encodeToken() const;

  /// Retrieves the unique client ID for this client session.
  [[nodiscard]] const std::string& getClientId() const;

  /// Creates a new invoke with an unique ID.
  [[nodiscard]] Invoke newInvoke();

  /// Finds and returns the invoke details for a given invoke ID.
  [[nodiscard]] std::optional<Invoke> findInvoke(const InvokeId& id);

  /// Updates the status and result of an existing invoke.
  bool updateInvoke(const InvokeId& id, const sen::MethodResult<sen::Var>& result);

  /// Releases a specific invoke by ID.
  void releaseInvoke(const InvokeId& id);

  /// Creates a new interest.
  sen::Result<InterestName, InterestError> createInterest(sen::kernel::RunApi& runApi,
                                                          const BusLocator& busLocator,
                                                          const InterestName& interestName,
                                                          const std::string& query);

  /// Removes an existing interest and cleans up associated subscriptions.
  bool removeInterest(InterestName interestName);

  /// Finds and returns the interest subscription details for a given interest name.
  [[nodiscard]] std::optional<InterestSubscription> findInterest(const InterestName& interestName);

  /// Retrieves a summary of a specific interest by name.
  [[nodiscard]] std::optional<InterestSummary> getInterestSummary(const InterestName& interestName);

  /// Retrieves a list of summaries for all registered interests.
  [[nodiscard]] InterestsSummary getAllInterestsSummary();

  /// Subscribes to event updates of a Sen object.
  bool subscribeEvent(const sen::kernel::KernelApi& kernelApi,
                      std::shared_ptr<sen::Object> object,
                      const EventLocator& eventLocator);

  /// Subscribes to property updates of a Sen object.
  bool subscribeProperty(const sen::kernel::KernelApi& kernelApi,
                         std::shared_ptr<sen::Object> object,
                         const PropertyLocator& propertyLocator,
                         const SubscriptionOptions& options);

  /// Unsubscribes from a member of a given Sen object.
  /// Members can be properties or events
  bool unsubscribeMember(sen::ObjectId objectId, const sen::MemberHash& memberId);

  /// Unsubscribes from all members of a given Sen object
  /// Members can be properties or events
  bool unsubscribeAllMembers(sen::ObjectId objectId);

  /// Returns a observer guard for a given notifier type
  [[nodiscard]] ObserverGuard getObserverGuard(NotifierType guardType);

private:
  std::string clientId_;
  std::mutex mutex_;

  ObjectInterestsManager interests_;
  ObjectInvokesManager invokes_;
  ObjectMembersManager members_;
};

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_CLIENT_SESSION_H
