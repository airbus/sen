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
#include "notification_loop.h"
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

  inline ObjectInterestsManager& interestsManager() { return interests_; }
  inline ObjectInvokesManager& invokesManager() { return invokes_; }
  inline ObjectMembersManager& membersManager() { return members_; }

private:
  friend class SenRouter;
  friend class NotificationLoop;

  /// Encodes the session client ID into a HTTP-friendly token.
  [[nodiscard]] std::string encodeToken() const;

  /// Retrieves the unique client ID for this client session.
  [[nodiscard]] const std::string& getClientId() const;

  /// Creates a new interest.
  sen::Result<InterestName, InterestError> createInterest(sen::kernel::RunApi& runApi,
                                                          const BusLocator& busLocator,
                                                          const InterestName& interestName,
                                                          const std::string& query);

  /// Returns a observer guard for a given notifier type
  [[nodiscard]] ObserverGuard getObserverGuard(NotifierType guardType);

private:
  std::string clientId_;

  ObjectInterestsManager interests_;
  ObjectInvokesManager invokes_;
  ObjectMembersManager members_;
};

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_CLIENT_SESSION_H
