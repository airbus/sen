// === client_session.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "client_session.h"

// component
#include "http_session.h"
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

// asio
#include <asio/ip/tcp.hpp>

// std
#include <memory>
#include <string>
#include <utility>

namespace sen::components::rest
{

ClientSession::~ClientSession()
{
  getLogger()->trace("Destroying ClientSession");

  activeNotificationLoops_.clear();
  interestsManager_.releaseAllInterests();

  getLogger()->trace("ClientSession destroyed");
}

[[nodiscard]] std::shared_ptr<NotificationLoop> ClientSession::buildNotificationLoop(
  std::shared_ptr<HttpSession> httpSession,
  std::shared_ptr<asio::ip::tcp::socket> socket)
{
  return activeNotificationLoops_.emplace_back(
    std::make_shared<NotificationLoop>(std::move(httpSession),
                                       std::move(socket),
                                       membersManager_.getObserverGuard(),
                                       invokesManager_.getObserverGuard(),
                                       interestsManager_.getObserverGuard()));
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
                                                                  const std::string& query,
                                                                  bool autoSubscribeProperties,
                                                                  bool autoSubscribeEvents)
{
  return interestsManager_.createInterest(
    runApi,
    busLocator,
    interestName,
    query,
    [this, &runApi, interestName, busLocator, autoSubscribeProperties, autoSubscribeEvents](sen::Object* object)
    {
      return (!autoSubscribeProperties ||
              membersManager_.subscribeAllProperties(runApi, interestName, *object, busLocator)) &&
             (!autoSubscribeEvents || membersManager_.subscribeAllEvents(runApi, interestName, *object, busLocator));
    },
    [this]([[maybe_unused]] const InterestName& interestName, sen::ObjectId objectId)
    {
      // Clean up internal object resources
      membersManager_.unsubscribeAll(objectId);
    });
}

}  // namespace sen::components::rest
