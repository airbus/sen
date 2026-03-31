// === client_session.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "client_session.h"

// component
#include "jwt.h"
#include "locators.h"
#include "notifications.h"
#include "object_interests_manager.h"
#include "types.h"
#include "utils.h"

// generated code
#include "stl/types.stl.h"

// sen
#include "sen/core/base/result.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/object.h"
#include "sen/kernel/component_api.h"

// std
#include <string>

namespace sen::components::rest
{

ClientSession::~ClientSession()
{
  getLogger()->trace("Destroying ClientSession");
  interests_.releaseAllInterests();
  getLogger()->trace("ClientSession destroyed");
}

[[nodiscard]] std::string ClientSession::encodeToken() const
{
  ClientAuth auth {clientId_};
  auto&& authVar = sen::toVariant(auth);

  return encodeJWT(sen::toJson(authVar));
}

[[nodiscard]] const std::string& ClientSession::getClientId() const { return clientId_; }

Result<InterestName, InterestError> ClientSession::createInterest(sen::kernel::RunApi& runApi,
                                                                  const BusLocator& busLocator,
                                                                  const InterestName& interestName,
                                                                  const std::string& query)
{
  return interests_.createInterest(runApi,
                                   busLocator,
                                   interestName,
                                   query,
                                   [this]([[maybe_unused]] const InterestName& interestName, sen::ObjectId objectId)
                                   {
                                     // Clean up internal object resources
                                     members_.unsubscribeAll(objectId);
                                   });
}

ObserverGuard ClientSession::getObserverGuard(NotifierType guardType)
{
  switch (guardType)
  {
    case NotifierType::interestsNotifier:
      return interests_.getObserverGuard();
    case NotifierType::membersNotifier:
      return members_.getObserverGuard();
    case NotifierType::invokesNotifier:
    default:
      return invokes_.getObserverGuard();
  }
}

}  // namespace sen::components::rest
