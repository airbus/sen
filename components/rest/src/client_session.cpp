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

// generated code
#include "stl/options.stl.h"
#include "stl/types.stl.h"

// sen
#include "sen/core/base/result.h"
#include "sen/core/io/util.h"
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

namespace sen::components::rest
{

ClientSession::~ClientSession()
{
  const std::lock_guard<std::mutex> lock(mutex_);
  invokes_.releaseAllInvokes();
  interests_.releaseAllInterests();
}

[[nodiscard]] std::string ClientSession::encodeToken() const
{
  ClientAuth auth {clientId_};
  auto&& authVar = sen::toVariant(auth);

  return encodeJWT(sen::toJson(authVar));
}

[[nodiscard]] const std::string& ClientSession::getClientId() const { return clientId_; }

[[nodiscard]] Invoke ClientSession::newInvoke()
{
  const std::lock_guard<std::mutex> lock(mutex_);
  return invokes_.newInvoke();
}

[[nodiscard]] std::optional<Invoke> ClientSession::findInvoke(const InvokeId& id)
{
  const std::lock_guard<std::mutex> lock(mutex_);
  return invokes_.findInvoke(id);
}

bool ClientSession::updateInvoke(const InvokeId& id, const sen::MethodResult<Var>& result)
{
  const std::lock_guard<std::mutex> lock(mutex_);
  return invokes_.updateInvoke(id, result);
}

void ClientSession::releaseInvoke(const InvokeId& id)
{
  const std::lock_guard<std::mutex> lock(mutex_);
  return invokes_.releaseInvoke(id);
}

Result<InterestName, InterestError> ClientSession::createInterest(sen::kernel::RunApi& runApi,
                                                                  const BusLocator& busLocator,
                                                                  const InterestName& interestName,
                                                                  const std::string& query)
{
  const std::lock_guard<std::mutex> lock(mutex_);
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

bool ClientSession::removeInterest(InterestName interestName)
{
  const std::lock_guard<std::mutex> lock(mutex_);
  return interests_.removeInterest(interestName) == interests_.interests_.cend();
}

std::optional<InterestSubscription> ClientSession::findInterest(const InterestName& interestName)
{
  const std::lock_guard<std::mutex> lock(mutex_);
  return interests_.findInterest(interestName);
}

std::optional<InterestSummary> ClientSession::getInterestSummary(const InterestName& interestName)
{
  const std::lock_guard<std::mutex> lock(mutex_);
  return interests_.getInterestSummary(interestName);
}

InterestsSummary ClientSession::getAllInterestsSummary()
{
  const std::lock_guard<std::mutex> lock(mutex_);
  return interests_.getAllInterestsSummary();
}

bool ClientSession::subscribeEvent(const sen::kernel::KernelApi& kernelApi,
                                   std::shared_ptr<sen::Object> object,
                                   const EventLocator& eventLocator)
{
  const std::lock_guard<std::mutex> lock(mutex_);
  return members_.subscribeEvent(kernelApi, object, eventLocator);
}

bool ClientSession::subscribeProperty(const sen::kernel::KernelApi& kernelApi,
                                      std::shared_ptr<sen::Object> object,
                                      const PropertyLocator& propertyLocator,
                                      const SubscriptionOptions& options)
{
  const std::lock_guard<std::mutex> lock(mutex_);
  return members_.subscribeProperty(kernelApi, object, propertyLocator, options);
}

bool ClientSession::unsubscribeMember(sen::ObjectId objectId, const sen::MemberHash& memberId)
{
  const std::lock_guard<std::mutex> lock(mutex_);
  return members_.unsubscribe(objectId, memberId);
}

bool ClientSession::unsubscribeAllMembers(sen::ObjectId objectId)
{
  const std::lock_guard<std::mutex> lock(mutex_);
  return members_.unsubscribeAll(objectId);
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
