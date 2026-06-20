// === subscriptions.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "subscriptions.h"

// local
#include "util.h"

// sen
#include "sen/core/meta/event.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/kernel/component_api.h"

// spdlog
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

// std
#include <string>
#include <string_view>
#include <utility>

namespace sen::components::jsonrpc
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

[[nodiscard]] nlohmann::json buildEventTriggeredNotification(std::string_view interestName,
                                                             std::string_view objectName,
                                                             std::string_view eventName,
                                                             const sen::Event& event,
                                                             const sen::VarList& args,
                                                             const sen::TimeStamp& creationTime)
{
  return nlohmann::json {{"interestName", interestName},
                         {"objectName", objectName},
                         {"eventName", eventName},
                         {"args", argListToWireJson(args, event.getArgs()).dump()},
                         {"timestamp", creationTime.toUtcStringNs()}};
}

[[nodiscard]] PropertyGuard wirePropertyGuard(Dispatcher& dispatcher,
                                              sen::impl::WorkQueue* workQueue,
                                              ConnectionId connId,
                                              std::string interestName,
                                              std::string objectName,
                                              std::string propertyName,
                                              std::shared_ptr<sen::Object> object,
                                              const sen::Property* property,
                                              ObjectSubs* subs,
                                              std::weak_ptr<sen::Object> serverWeak)
{
  const bool reliable = property->getTransportMode() == sen::TransportMode::confirmed;
  return PropertyGuard {
    object->onPropertyChangedUntyped(
      property,
      {workQueue,
       [&dispatcher,
        connId,
        interestName = std::move(interestName),
        objectName = std::move(objectName),
        propertyName = std::move(propertyName),
        object,
        property,
        subs,
        serverWeak = std::move(serverWeak)](const sen::EventInfo& /*info*/, const sen::VarList& /*args*/)
       {
         // Pin the Server for the body so the captured subs* (which lives inside its
         // InterestEntry) stays valid even if a queued callback fires after disconnect.
         auto self = serverWeak.lock();
         if (!self)
         {
           return;
         }
         subs->pendingValues[propertyName] = varToWireJson(object->getPropertyUntyped(property), *property->getType());
         // Commits are monotonic, so last-write-wins gives the latest time.
         subs->latestChangeTime = object->getLastCommitTime();
         dispatcher.markDirty(connId, interestName, objectName);
       }}),
    reliable};
}

[[nodiscard]] sen::ConnectionGuard wireEventGuard(Dispatcher& dispatcher,
                                                  sen::impl::WorkQueue* workQueue,
                                                  ConnectionId connId,
                                                  std::string interestName,
                                                  std::string objectName,
                                                  std::string eventName,
                                                  std::shared_ptr<sen::Object> object,
                                                  const sen::Event* event,
                                                  std::weak_ptr<sen::Object> serverWeak)
{
  return object->onEventUntyped(
    event,
    {workQueue,
     [&dispatcher,
      connId,
      interestName = std::move(interestName),
      objectName = std::move(objectName),
      eventName = std::move(eventName),
      object,
      event,
      serverWeak = std::move(serverWeak)](const sen::EventInfo& info, const sen::VarList& args)
     {
       // Server-liveness gate: a queued event for a torn-down Server would push to a stale
       // ConnectionId, risking misdirection if the id has been reused.
       auto self = serverWeak.lock();
       if (!self)
       {
         return;
       }
       const bool reliable = event->getTransportMode() == sen::TransportMode::confirmed;
       dispatcher.pushNotification(
         connId,
         "eventTriggered",
         buildEventTriggeredNotification(interestName, objectName, eventName, *event, args, info.creationTime),
         reliable);
     }});
}

/// Wires a guard for every name in `requested` not yet in `active`. Names the class lacks
/// stay in the requested set so a future instance that has them can pick them up. `ActiveMap`
/// is templated so events keep their bare `ConnectionGuard` map while properties use
/// `PropertyGuard`.
template <typename ActiveMap, typename LookupFn, typename WireFn>
void rewireKind(const std::unordered_set<std::string>& requested,
                ActiveMap& active,
                std::string_view methodName,
                std::string_view memberKind,
                std::string_view objectName,
                std::string_view metaName,
                const std::string& interestName,
                ConnectionId connId,
                LookupFn&& lookup,
                WireFn&& wire)
{
  for (const auto& name: requested)
  {
    if (active.count(name) != 0U)
    {
      continue;
    }
    const auto* member = lookup(name);
    if (member == nullptr)
    {
      SPDLOG_LOGGER_WARN(getLogger(),
                         "{}: {} '{}' not found on object '{}' (class '{}') in interest '{}' on connection {}; "
                         "kept for a future instance",
                         methodName,
                         memberKind,
                         name,
                         objectName,
                         metaName,
                         interestName,
                         connId.get());
      continue;
    }
    active.emplace(name, wire(name, member));
  }
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Subscriptions
//--------------------------------------------------------------------------------------------------------------

void rewireRequestedSubscriptions(Dispatcher& dispatcher,
                                  sen::kernel::KernelApi& api,
                                  ConnectionId connId,
                                  const std::string& interestName,
                                  std::shared_ptr<sen::Object> object,
                                  ObjectSubs& subs,
                                  std::weak_ptr<sen::Object> serverWeak)
{
  auto* workQueue = api.getWorkQueue();
  const auto objectName = std::string {object->getName()};
  auto meta = object->getClass();
  const auto metaName = std::string {meta->getQualifiedName()};

  rewireKind(
    subs.requestedProperties,
    subs.activePropertyGuards,
    "subscribeProperty",
    "property",
    objectName,
    metaName,
    interestName,
    connId,
    [&meta](const std::string& name) { return meta->searchPropertyByName(name); },
    [&](const std::string& name, const sen::Property* property)
    {
      return wirePropertyGuard(
        dispatcher, workQueue, connId, interestName, objectName, name, object, property, &subs, serverWeak);
    });

  rewireKind(
    subs.requestedEvents,
    subs.activeEventGuards,
    "subscribeEvent",
    "event",
    objectName,
    metaName,
    interestName,
    connId,
    [&meta](const std::string& name) { return meta->searchEventByName(name); },
    [&](const std::string& name, const sen::Event* event)
    {
      return wireEventGuard(dispatcher, workQueue, connId, interestName, objectName, name, object, event, serverWeak);
    });
}

void applySubscribeBlockToObjectSubs(const InterestSubscribeBlock& block, const sen::Object& object, ObjectSubs& subs)
{
  if (subs.interval.getNanoseconds() == 0)
  {
    subs.interval = block.interval;
  }

  auto meta = object.getClass();

  switch (block.properties.kind)
  {
    case ParsedMemberSelector::Kind::none:
      break;
    case ParsedMemberSelector::Kind::wildcard:
      for (const auto& property: meta->getProperties(sen::ClassType::SearchMode::includeParents))
      {
        subs.requestedProperties.insert(std::string {property->getName()});
      }
      break;
    case ParsedMemberSelector::Kind::named:
      for (const auto& name: block.properties.names)
      {
        subs.requestedProperties.insert(name);
      }
      break;
  }

  switch (block.events.kind)
  {
    case ParsedMemberSelector::Kind::none:
      break;
    case ParsedMemberSelector::Kind::wildcard:
      for (const auto& event: meta->getEvents(sen::ClassType::SearchMode::includeParents))
      {
        subs.requestedEvents.insert(std::string {event->getName()});
      }
      break;
    case ParsedMemberSelector::Kind::named:
      for (const auto& name: block.events.names)
      {
        subs.requestedEvents.insert(name);
      }
      break;
  }
}

void seedSnapshotsForProperties(Dispatcher& dispatcher,
                                ConnectionId connId,
                                const std::string& interestName,
                                const std::string& objectName,
                                ObjectSubs& subs,
                                const sen::Object& object,
                                const std::unordered_set<std::string>& propertyNames)
{
  bool seeded = false;
  auto meta = object.getClass();
  for (const auto& name: propertyNames)
  {
    const sen::Property* property = meta->searchPropertyByName(name);
    if (property == nullptr)
    {
      continue;
    }
    subs.pendingValues[name] = varToWireJson(object.getPropertyUntyped(property), *property->getType());
    seeded = true;
  }

  if (seeded)
  {
    dispatcher.markDirty(connId, interestName, objectName);
    subs.latestChangeTime = object.getLastCommitTime();
    subs.lastEmitTime.reset();
  }
}

}  // namespace sen::components::jsonrpc
