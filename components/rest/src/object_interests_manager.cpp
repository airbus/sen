// === object_interests_manager.cpp ====================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "object_interests_manager.h"

// component
#include "locators.h"
#include "types.h"
#include "utils.h"

// generated code
#include "stl/types.stl.h"

// sen
#include "sen/core/base/result.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"

// std
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

using Json = nlohmann::json;

namespace sen::components::rest
{

//--------------------------------------------------------------------------------------------------------------
// Interests
//--------------------------------------------------------------------------------------------------------------

sen::Result<InterestName, InterestError> ObjectInterestsManager::createInterest(sen::kernel::RunApi& runApi,
                                                                                const BusLocator& busLocator,
                                                                                const InterestName& interestName,
                                                                                const std::string& query,
                                                                                InterestCallback&& onObjectRemoved)
{
  // Check interest name uniqueness
  if (interests_.find(interestName) != interests_.cend())
  {
    return sen::Err(InterestError::nameNotUnique);
  }

  auto source = runApi.getSource(busLocator.toBusAddress());
  if (!source)
  {
    return sen::Err(InterestError::invalidBus);
  }

  auto interest = sen::Interest::make(query, runApi.getTypes());
  auto subscription = std::make_shared<sen::Subscription<sen::Object>>();
  subscription->source = source;

  std::ignore = subscription->list.onAdded(
    [this, busLocator, interestName](const auto& iterators)
    {
      for (auto it = iterators.untypedBegin; it != iterators.untypedEnd; ++it)
      {
        notify(Notification {
          NotificationType::objectAdded,
          interestName,
          sen::TimeStamp {std::chrono::system_clock::now().time_since_epoch()},
          toJson(*it->get(), busLocator),
        });
      }
    });
  std::ignore = subscription->list.onRemoved(
    [this, busLocator, onObjectRemoved, interestName = interestName](const auto& iterators)
    {
      for (auto it = iterators.untypedBegin; it != iterators.untypedEnd; ++it)
      {
        auto untypedObj = it->get();
        auto objectId = untypedObj->getId();

        notify(Notification {
          NotificationType::objectRemoved,
          interestName,
          sen::TimeStamp {std::chrono::system_clock::now().time_since_epoch()},
          toJson(*untypedObj, busLocator),
        });

        onObjectRemoved(interestName, objectId);
      }
    });

  subscription->source->addSubscriber(interest, &subscription->list, false);
  interests_.emplace(interestName, InterestSubscription {interest, subscription, busLocator});

  return sen::Ok(interestName);
}

bool ObjectInterestsManager::removeInterest(InterestName interestName)
{
  getLogger()->trace("Remove interest begin");

  auto interestIt = interests_.find(interestName);
  if (interestIt == interests_.end())
  {
    return false;
  }

  auto source = interestIt->second.subscription->source;
  if (!source)
  {
    return false;
  }

  source->removeSubscriber(interestIt->second.interest, &interestIt->second.subscription->list, false);
  interests_.erase(interestIt);

  getLogger()->trace("Remove interest finished");

  return true;
}

[[nodiscard]] std::optional<InterestSubscription> ObjectInterestsManager::findInterest(
  const InterestName& interestName) const
{
  auto interestIt = interests_.find(interestName);
  if (interestIt == interests_.cend())
  {
    return std::nullopt;
  }

  return interestIt->second;
}

[[nodiscard]] std::optional<InterestSummary> ObjectInterestsManager::getInterestSummary(
  const InterestName& interestName)
{
  auto interestIt = interests_.find(interestName);
  if (interestIt == interests_.cend())
  {
    return std::nullopt;
  }

  return InterestSummary {
    interestIt->first,
    interestIt->second.busLocator.session(),
    interestIt->second.busLocator.bus(),
  };
}

[[nodiscard]] InterestsSummary ObjectInterestsManager::getAllInterestsSummary()
{
  InterestsSummary interests;
  for (const auto& interest: interests_)
  {
    interests.emplace_back(InterestSummary {
      interest.first,
      interest.second.busLocator.session(),
      interest.second.busLocator.bus(),
    });
  }
  return interests;
}

void ObjectInterestsManager::releaseAllInterests()
{
  for (const auto& [interestName, interest]: interests_)
  {
    std::ignore = interestName;
    auto source = interest.subscription->source;
    if (source)
    {
      source->removeSubscriber(interest.interest, &interest.subscription->list, false);
    }
  }
  interests_.clear();
}

}  // namespace sen::components::rest
