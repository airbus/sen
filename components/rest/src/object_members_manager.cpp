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

  auto memberId = property->getId();
  auto objectId = object->getId();

  auto guard = object->onPropertyChangedUntyped(
    property,
    {kernelApi.getWorkQueue(),
     [this,
      object,
      interest,
      propertyLocator,
      objectId,
      memberId,
      maxUpdateTime = options.maxUpdateTime,
      lastUpdate = TimeStamp(0)](const sen::EventInfo& info, const sen::VarList& args) mutable
     {
       std::ignore = args;

       if (maxUpdateTime.has_value() && (info.creationTime - lastUpdate) <= maxUpdateTime)
       {
         // Dropping update
         return;
       }
       lastUpdate = info.creationTime;

       const auto objectIt = members_.find(objectId);
       if (objectIt == members_.cend())
       {
         SPDLOG_LOGGER_ERROR(getLogger(), "Unexpected condition: object properties not found on property callback");
         return;
       }

       const auto propertyIt = objectIt->second.find(memberId);
       if (propertyIt != objectIt->second.cend())
       {
         try
         {
           auto propertyData = toJson(*object, propertyLocator);
           notify(Notification {NotificationType::property, interest, info.creationTime, propertyData});
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

  members_[objectId].emplace(memberId, std::move(guard));

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

  auto memberId = event->getId();
  auto objectId = object->getId();

  auto guard = object->onEventUntyped(
    event,
    {kernelApi.getWorkQueue(),
     [this, object, objectId, interest, memberId](const sen::EventInfo& info, const sen::VarList& value)
     {
       const auto objectIt = members_.find(objectId);
       if (objectIt == members_.cend())
       {
         SPDLOG_LOGGER_ERROR(getLogger(), "Unexpected condition: object events not found on event callback");
         return;
       }

       const auto eventIt = objectIt->second.find(memberId);
       if (eventIt != objectIt->second.cend())
       {

         notify(Notification {NotificationType::evt, interest, info.creationTime, toJson(*object, value)});
       }
       else
       {
         SPDLOG_LOGGER_ERROR(getLogger(), "Unexpected condition: event not registered");
       }
     }});

  members_[objectId].emplace(memberId, std::move(guard));

  return true;
}

bool ObjectMembersManager::unsubscribe(const sen::ObjectId& objectId, const sen::MemberHash& memberId)
{
  const auto objectIt = members_.find(objectId);
  if (objectIt == members_.cend())
  {
    return false;
  }

  return objectIt->second.erase(memberId) > 0;
}

bool ObjectMembersManager::unsubscribeAll(const sen::ObjectId& objectId)
{
  const auto objectIt = members_.find(objectId);
  if (objectIt == members_.cend())
  {
    return false;
  }

  return members_.erase(objectId) > 0;
}

}  // namespace sen::components::rest
