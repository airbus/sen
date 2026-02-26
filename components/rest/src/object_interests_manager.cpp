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

// generated code
#include "stl/types.stl.h"

// sen
#include "sen/core/base/result.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_list.h"
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
  auto source = runApi.getSource(busLocator.toBusAddress());
  if (!source)
  {
    return sen::Err(InterestError::invalidBus);
  }

  auto objects = std::make_shared<sen::ObjectList<sen::Object>>();
  auto interest = sen::Interest::make(query, runApi.getTypes());

  // Check interest name uniqueness
  if (interests_.find(interestName) != interests_.cend())
  {
    return sen::Err(InterestError::nameNotUnique);
  }

  std::ignore = objects->onAdded(
    [this, busLocator](const auto& iterators)
    {
      for (auto it = iterators.untypedBegin; it != iterators.untypedEnd; ++it)
      {
        notify(Notification {
          NotificationType::objectAdded,
          sen::TimeStamp {std::chrono::system_clock::now().time_since_epoch()},
          toJson(*it->get(), busLocator),
        });
      }
    });
  std::ignore = objects->onRemoved(
    [this, busLocator, onObjectRemoved, interestName = interestName](const auto& iterators)
    {
      for (auto it = iterators.untypedBegin; it != iterators.untypedEnd; ++it)
      {
        auto untypedObj = it->get();
        auto objectId = untypedObj->getId();

        notify(Notification {
          NotificationType::objectRemoved,
          sen::TimeStamp {std::chrono::system_clock::now().time_since_epoch()},
          toJson(*untypedObj, busLocator),
        });

        onObjectRemoved(interestName, objectId);
      }
    });

  source->addSubscriber(interest, objects.get(), true);
  interests_.emplace(interestName,
                     InterestSubscription {
                       std::move(source),
                       interest,
                       std::move(objects),
                       busLocator,
                     });

  return sen::Ok(interestName);
}

InterestMapIterator ObjectInterestsManager::removeInterest(InterestName interestName)
{
  auto interestIt = interests_.find(interestName);
  if (interestIt == interests_.end())
  {
    return interests_.end();
  }

  auto source = interestIt->second.source;
  if (!source)
  {
    return interests_.end();
  }

  source->removeSubscriber(interestIt->second.interest, interestIt->second.objects.get(), false);
  return interests_.erase(interestIt);
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
  for (auto it = interests_.begin(); it != interests_.end();)
  {
    it = removeInterest(it->first);
  }
  interests_.clear();
}

}  // namespace sen::components::rest
