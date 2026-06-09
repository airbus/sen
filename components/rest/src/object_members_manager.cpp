// === object_members_manager.cpp ======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "object_members_manager.h"

// component
#include "locators.h"
#include "sen/core/meta/type.h"
#include "types.h"
#include "utils.h"

// generated code
#include "stl/options.stl.h"
#include "stl/types.stl.h"

// sen
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/object.h"
#include "sen/kernel/component_api.h"

// json
#include "nlohmann/json.hpp"

// spdlog
#include <spdlog/spdlog.h>

// std
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

using Json = nlohmann::json;

namespace sen::components::rest
{

//--------------------------------------------------------------------------------------------------------------
// Properties
//--------------------------------------------------------------------------------------------------------------

bool ObjectMembersManager::subscribeProperty(const sen::kernel::KernelApi& kernelApi,
                                             const InterestName& interest,
                                             std::shared_ptr<sen::Object> object,
                                             const PropertyLocator& propertyLocator,
                                             const SubscriptionOptions& options)
{
  const sen::Property* property = object->getClass()->searchPropertyByName(propertyLocator.property());
  if (!property)
  {
    return false;
  }

  auto propertyId = property->getId();
  auto objectId = object->getId();

  auto guard = object->onPropertyChangedUntyped(
    property,
    {kernelApi.getWorkQueue(),
     [this,
      object,
      interest,
      propertyLocator,
      objectId,
      propertyId,
      maxUpdateTime = options.maxUpdateTime,
      lastUpdate = TimeStamp(0)](const sen::EventInfo& info, const sen::VarList& args) mutable
     {
       std::ignore = args;

       if (maxUpdateTime.has_value() && (info.creationTime - lastUpdate) <= maxUpdateTime.asOptional())
       {
         // Dropping update
         return;
       }
       lastUpdate = info.creationTime;

       const auto objectIt = properties_.find(objectId);
       if (objectIt == properties_.cend())
       {
         SPDLOG_LOGGER_ERROR(getLogger(), "Unexpected condition: object properties not found on property callback");
         return;
       }

       const auto propertyIt = objectIt->second.find(propertyId);
       if (propertyIt != objectIt->second.cend())
       {
         try
         {
           notify(
             Notification {NotificationType::property, interest, info.creationTime, toJson(*object, propertyLocator)});
         }
         catch (std::exception& e)
         {
           SPDLOG_LOGGER_ERROR(getLogger(), "Error on property notification: " + std::string(e.what()));
         }
       }
       else
       {
         SPDLOG_LOGGER_ERROR(getLogger(), "Unexpected condition: property subscription failed");
       }
     }});

  properties_[objectId].emplace(propertyId, std::move(guard));

  return true;
}

bool ObjectMembersManager::subscribeEvent(const sen::kernel::KernelApi& kernelApi,
                                          const InterestName& interest,
                                          std::shared_ptr<sen::Object> object,
                                          const EventLocator& eventLocator)
{

  const sen::Event* event = object->getClass()->searchEventByName(static_cast<std::string_view>(eventLocator.event()));
  if (!event)
  {
    return false;
  }

  auto eventId = event->getId();
  auto objectId = object->getId();

  auto guard = object->onEventUntyped(
    event,
    {kernelApi.getWorkQueue(),
     [this, object, objectId, interest, eventId, eventLocator](const sen::EventInfo& info, const sen::VarList& value)
     {
       const auto objectIt = events_.find(objectId);
       if (objectIt == events_.cend())
       {
         SPDLOG_LOGGER_ERROR(getLogger(), "Unexpected condition: object events not found on event callback");
         return;
       }

       const auto eventIt = objectIt->second.find(eventId);
       if (eventIt != objectIt->second.cend())
       {
         notify(
           Notification {NotificationType::evt, interest, info.creationTime, toJson(*object, value, eventLocator)});
       }
       else
       {
         SPDLOG_LOGGER_ERROR(getLogger(), "Unexpected condition: event not registered");
       }
     }});

  events_[objectId].emplace(eventId, std::move(guard));

  return true;
}

bool ObjectMembersManager::unsubscribeProperty(const sen::ObjectId& objectId, const MemberHash& propertyId)
{
  const auto objectIt = properties_.find(objectId);
  if (objectIt == properties_.cend())
  {
    return false;
  }

  return objectIt->second.erase(propertyId) > 0;
}

bool ObjectMembersManager::unsubscribeEvent(const sen::ObjectId& objectId, const MemberHash& eventId)
{
  const auto objectIt = events_.find(objectId);
  if (objectIt == properties_.cend())
  {
    return false;
  }

  return objectIt->second.erase(eventId) > 0;
}

void ObjectMembersManager::unsubscribeAll(const sen::ObjectId& objectId)
{
  if (const auto objectPropertyIt = properties_.find(objectId); objectPropertyIt != properties_.cend())
  {
    properties_.erase(objectId);
  }

  if (const auto objectEventIt = events_.find(objectId); objectEventIt != events_.cend())
  {
    events_.erase(objectId);
  }
}

bool ObjectMembersManager::isSubscribedTo(const sen::ObjectId& objectId, const MemberHash& memberId)
{
  const auto objectPropertyIt = properties_.find(objectId);
  const auto objectEventIt = events_.find(objectId);

  bool isFoundInProperties = objectPropertyIt != properties_.cend() &&
                             objectPropertyIt->second.find(memberId) != objectPropertyIt->second.cend();
  bool isFoundInEvents =
    objectEventIt != events_.cend() && objectEventIt->second.find(memberId) != objectEventIt->second.cend();

  return isFoundInProperties || isFoundInEvents;
}

std::vector<sen::MemberHash> ObjectMembersManager::getPropertyIds(const sen::ObjectId& objectId)
{
  std::vector<sen::MemberHash> propertyIds;
  const auto objectIt = properties_.find(objectId);

  if (objectIt != properties_.cend())
  {
    for (const auto& prop: objectIt->second)
    {
      propertyIds.push_back(prop.first);
    }
  }

  return propertyIds;
}

std::vector<sen::MemberHash> ObjectMembersManager::getEventIds(const sen::ObjectId& objectId)
{
  std::vector<sen::MemberHash> eventIds;
  const auto objectIt = events_.find(objectId);

  if (objectIt != events_.cend())
  {
    for (const auto& event: objectIt->second)
    {
      eventIds.push_back(event.first);
    }
  }

  return eventIds;
}

bool ObjectMembersManager::subscribeAllProperties(const sen::kernel::KernelApi& kernelApi,
                                                  const InterestName& interest,
                                                  sen::Object& object,
                                                  const BusLocator& busLocator)
{
  for (auto const& property: object.getClass()->getProperties(sen::ClassType::SearchMode::includeParents))
  {
    auto propertyId = property->getId();
    auto objectId = object.getId();

    auto propertyLocatorOpt = PropertyLocator::build(busLocator, object.getName(), std::string(property->getName()));
    if (propertyLocatorOpt.isError())
    {
      return false;
    }

    const auto& propertyLocator = propertyLocatorOpt.getValue();

    auto guard = object.onPropertyChangedUntyped(
      property.get(),
      {kernelApi.getWorkQueue(),
       [this, &object, interest, propertyLocator](const sen::EventInfo& info,
                                                  [[maybe_unused]] const sen::VarList& args) mutable
       {
         notify(
           Notification {NotificationType::property, interest, info.creationTime, toJson(object, propertyLocator)});
       }});

    properties_[objectId].emplace(propertyId, std::move(guard));
  }

  return true;
}

bool ObjectMembersManager::subscribeAllEvents(const sen::kernel::KernelApi& kernelApi,
                                              const InterestName& interest,
                                              sen::Object& object,
                                              const BusLocator& busLocator)
{
  for (const auto& event: object.getClass()->getEvents(sen::ClassType::SearchMode::includeParents))
  {
    auto eventId = event->getId();
    auto objectId = object.getId();

    auto eventLocatorOpt = EventLocator::build(busLocator, object.getName(), std::string(event->getName()));
    if (eventLocatorOpt.isError())
    {
      return false;
    }

    const auto& eventLocator = eventLocatorOpt.getValue();

    auto guard = object.onEventUntyped(
      event.get(),
      {kernelApi.getWorkQueue(),
       [this, &object, interest, eventLocator](const sen::EventInfo& info, const sen::VarList& value)
       {
         notify(Notification {NotificationType::evt, interest, info.creationTime, toJson(object, value, eventLocator)});
       }});

    events_[objectId].emplace(eventId, std::move(guard));
  }

  return true;
}

}  // namespace sen::components::rest
