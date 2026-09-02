// === server.cpp ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "server.h"

// local
#include "dispatcher.h"
#include "messages.h"
#include "subscriptions.h"
#include "topology_service.h"
#include "util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/span.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/meta/type_traits.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/subscription.h"
#include "sen/gen/json.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/source_info.h"

// generated
#include "stl/jsonrpc.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/sen/kernel/type_specs.stl.h"

// spdlog
#include <spdlog/spdlog.h>

// std
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace sen::components::jsonrpc
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

[[noreturn]] void unreachableHandRouted(const char* methodName)
{
  // Hand-routed in Dispatcher; reaching the typed Server override means the routing table
  // is broken. Abort instead of bubbling an internalError that hides the bug.
  SPDLOG_LOGGER_ERROR(
    getLogger(), "{}: hand-routed in Dispatcher; typed Server override should be unreachable. Aborting.", methodName);
  SEN_ASSERT(false && "Server typed setProperty/invoke override is unreachable; check Dispatcher routing");
  std::abort();
}

/// Wire-faithful encoding of a typed Sen STL value.
template <typename T>
[[nodiscard]] nlohmann::json senValueToJson(const T& value)
{
  return varToWireJson(sen::toVariant(value), *sen::MetaTypeTrait<T>::meta());
}

/// Renders one custom type's schema fragment and inserts the parsed top-level body into
/// `typeSchemas` keyed by qualified name. Errors are logged and swallowed; never break
/// the wire-delivery path. Returns silently if the type is already in `knownSchemas`.
void bundleSchemaIfNew(const sen::CustomType& customType,
                       sen::gen::JsonGenerator& generator,
                       std::unordered_set<std::string>& knownSchemas,
                       nlohmann::json& typeSchemas)
{
  const std::string qualifiedName {customType.getQualifiedName()};
  if (!knownSchemas.insert(qualifiedName).second)
  {
    return;
  }
  try
  {
    typeSchemas[qualifiedName] = nlohmann::json::parse(generator.renderTypeSchema(customType));
  }
  catch (const std::exception&)
  {
    // Drop the schema for this type; spec still goes through.
  }
}

/// Bundles `customType`'s spec into `types` if `knownTypes` hasn't seen it before, then walks
/// every dependent custom type and bundles each unseen one. When `shipSchemas` is true, also
/// renders + accumulates each previously-unschemad type into `typeSchemas` (independent dedup
/// via `knownSchemas`). Native types are silently skipped.
void bundleTypeAndDependencies(const sen::CustomType& customType,
                               std::unordered_set<std::string>& knownTypes,
                               nlohmann::json& types,
                               bool shipSchemas,
                               sen::gen::JsonGenerator* generator,
                               std::unordered_set<std::string>& knownSchemas,
                               nlohmann::json& typeSchemas)
{
  const std::string qualifiedName {customType.getQualifiedName()};
  if (knownTypes.insert(qualifiedName).second)
  {
    types.push_back(senValueToJson(makeWireCustomTypeSpec(customType)));
  }
  if (shipSchemas && generator != nullptr)
  {
    bundleSchemaIfNew(customType, *generator, knownSchemas, typeSchemas);
  }

  std::function<void(sen::ConstTypeHandle<>)> visitor =
    [&knownTypes, &types, shipSchemas, generator, &knownSchemas, &typeSchemas](sen::ConstTypeHandle<> dep)
  {
    if (const auto* depCustom = dep->asCustomType(); depCustom != nullptr)
    {
      const std::string depName {depCustom->getQualifiedName()};
      if (knownTypes.insert(depName).second)
      {
        types.push_back(senValueToJson(makeWireCustomTypeSpec(*depCustom)));
      }
      if (shipSchemas && generator != nullptr)
      {
        bundleSchemaIfNew(*depCustom, *generator, knownSchemas, typeSchemas);
      }
    }
  };
  sen::iterateOverDependentTypes(customType, visitor);
}

// createInterest closure helpers

template <typename Range>
[[nodiscard]] nlohmann::json toNameArray(const Range& objects)
{
  auto array = nlohmann::json::array();
  for (auto* obj: objects)
  {
    if (obj != nullptr)
    {
      array.push_back(obj->getName());
    }
  }
  return array;
}

/// `[{objectName, qualifiedClassName}, ...]` for the STL `AddedObjectEntry` shape.
template <typename Range>
[[nodiscard]] nlohmann::json toAddedEntries(const Range& objects)
{
  auto array = nlohmann::json::array();
  for (auto* obj: objects)
  {
    if (obj != nullptr)
    {
      array.push_back(nlohmann::json {{"objectName", obj->getName()},
                                      {"qualifiedClassName", std::string {obj->getClass()->getQualifiedName()}}});
    }
  }
  return array;
}

/// `interestUpdate` body for an `onAdded` batch: object names+classes plus `CustomTypeSpec`s for
/// each not-yet-seen custom type referenced by their classes. When `shipSchemas` is true, also
/// fills `typeSchemas` (serialized as a string field) with each previously-unschemad type.
/// Mutates `knownTypes` and (when shipSchemas) `knownSchemas`.
template <typename Range>
[[nodiscard]] nlohmann::json buildAddedNotification(std::string_view interestName,
                                                    const Range& addedObjects,
                                                    std::unordered_set<std::string>& knownTypes,
                                                    bool shipSchemas,
                                                    sen::gen::JsonGenerator* generator,
                                                    std::unordered_set<std::string>& knownSchemas)
{
  auto types = nlohmann::json::array();
  auto typeSchemas = nlohmann::json::object();
  for (auto* obj: addedObjects)
  {
    if (obj != nullptr)
    {
      bundleTypeAndDependencies(*obj->getClass(), knownTypes, types, shipSchemas, generator, knownSchemas, typeSchemas);
    }
  }
  return nlohmann::json {{"interestName", interestName},
                         {"added", toAddedEntries(addedObjects)},
                         {"removed", nlohmann::json::array()},
                         {"types", std::move(types)},
                         {"typeSchemas", typeSchemas.empty() ? std::string {} : typeSchemas.dump()}};
}

/// `interestUpdate` body for an `onRemoved` batch: just the removed objects' names.
template <typename Range>
[[nodiscard]] nlohmann::json buildRemovedNotification(std::string_view interestName, const Range& removedObjects)
{
  return nlohmann::json {{"interestName", interestName},
                         {"added", nlohmann::json::array()},
                         {"removed", toNameArray(removedObjects)},
                         {"types", nlohmann::json::array()},
                         {"typeSchemas", std::string {}}};
}

void checkObjectSubsInvariants([[maybe_unused]] const ObjectSubs& subs)
{
#if defined(DEBUG)
  for (const auto& p: subs.explicitProperties)
  {
    SEN_DEBUG_ASSERT(subs.requestedProperties.count(p) != 0U);
  }
  for (const auto& e: subs.explicitEvents)
  {
    SEN_DEBUG_ASSERT(subs.requestedEvents.count(e) != 0U);
  }
  for (const auto& kv: subs.activePropertyGuards)
  {
    SEN_DEBUG_ASSERT(subs.requestedProperties.count(kv.first) != 0U);
  }
  for (const auto& kv: subs.activeEventGuards)
  {
    SEN_DEBUG_ASSERT(subs.requestedEvents.count(kv.first) != 0U);
  }
  for (const auto& kv: subs.dirtyProperties)
  {
    SEN_DEBUG_ASSERT(subs.requestedProperties.count(kv.first) != 0U);
  }
#endif
}

/// onAdded phase 1: fold the interest's standing subscribe block (if any) into each new object's
/// per-object request set so phase 2 sees a populated set when it builds the snapshot.
void applySubscribeBlockToAddedBatch(InterestEntry& interestEntry, const sen::ObjectList<sen::Object>::Iterators& it)
{
  if (!interestEntry.subscribeBlock.has_value())
  {
    return;
  }
  for (auto* obj: it.typed())
  {
    if (obj == nullptr)
    {
      continue;
    }
    auto& subs = interestEntry.objectSubs[std::string {obj->getName()}];
    applySubscribeBlockToObjectSubs(*interestEntry.subscribeBlock, *obj, subs);
  }
}

/// onAdded phase 2: builds the `interestUpdate.added` envelope and seeds `currentValues` for
/// subscribed properties. The seed is required because Sen notifications are delta-only.
/// Pushed reliably so structural traffic cannot be dropped.
void emitAddedInterestUpdate(Dispatcher& dispatcher,
                             Server& server,
                             const InterestEntry& interestEntry,
                             ConnectionId connId,
                             const std::string& interestName,
                             const sen::ObjectList<sen::Object>::Iterators& it)
{
  auto envelope = buildAddedNotification(interestName,
                                         it.typed(),
                                         server.knownTypes(),
                                         interestEntry.withSchemas,
                                         &dispatcher.jsonGenerator(),
                                         server.knownSchemas());
  for (auto& entry: envelope["added"])
  {
    entry["currentValues"] = nlohmann::json::array();
    const auto name = entry["objectName"].get<std::string>();
    const auto subsIt = interestEntry.objectSubs.find(name);
    if (subsIt == interestEntry.objectSubs.end() || subsIt->second.requestedProperties.empty())
    {
      continue;
    }
    auto sharedObject = findObject(interestEntry, name);
    if (!sharedObject)
    {
      continue;
    }
    auto& currentValues = entry["currentValues"];
    for (const auto& propertyName: subsIt->second.requestedProperties)
    {
      const sen::Property* property = sharedObject->getClass()->searchPropertyByName(propertyName);
      if (property == nullptr)
      {
        continue;
      }
      currentValues.push_back(nlohmann::json {
        {"propertyName", propertyName},
        {"value", varToWireJson(sharedObject->getPropertyUntyped(property), *property->getType()).dump()}});
    }
  }
  dispatcher.pushNotification(connId, "interestUpdate", std::move(envelope), /*reliable=*/true);
}

void rewireAddedBatch(Dispatcher& dispatcher,
                      sen::kernel::KernelApi& api,
                      InterestEntry& interestEntry,
                      ConnectionId connId,
                      const std::string& interestName,
                      const sen::ObjectList<sen::Object>::Iterators& it,
                      std::weak_ptr<sen::Object> serverWeak)
{
  for (auto* obj: it.typed())
  {
    if (obj == nullptr)
    {
      continue;
    }
    auto sharedObject = findObject(interestEntry, obj->getName());
    if (!sharedObject)
    {
      continue;
    }
    const auto subsIt = interestEntry.objectSubs.find(obj->getName());
    if (subsIt == interestEntry.objectSubs.end())
    {
      continue;
    }
    rewireRequestedSubscriptions(
      dispatcher, api, connId, interestName, std::move(sharedObject), subsIt->second, serverWeak);
    checkObjectSubsInvariants(subsIt->second);
  }
}

void dropActiveGuardsForRemovedBatch(InterestEntry& interestEntry, const sen::ObjectList<sen::Object>::Iterators& it)
{
  for (auto* obj: it.typed())
  {
    if (obj == nullptr)
    {
      continue;
    }
    if (auto subsIt = interestEntry.objectSubs.find(obj->getName()); subsIt != interestEntry.objectSubs.end())
    {
      subsIt->second.activePropertyGuards.clear();
      subsIt->second.activeEventGuards.clear();
      checkObjectSubsInvariants(subsIt->second);
    }
  }
}

[[nodiscard]] ParsedMemberSelector toParsedMemberSelector(const MaybeMemberSelector& selector)
{
  if (!selector.has_value())
  {
    return ParsedMemberSelector {};
  }
  return std::visit(
    [](const auto& arm) -> ParsedMemberSelector
    {
      using T = std::decay_t<decltype(arm)>;
      if constexpr (std::is_same_v<T, WildcardSelection>)
      {
        return ParsedMemberSelector {ParsedMemberSelector::Kind::wildcard, {}};
      }
      else
      {
        ParsedMemberSelector out {ParsedMemberSelector::Kind::named, {}};
        out.names.assign(arm.memberNames.begin(), arm.memberNames.end());
        return out;
      }
    },
    *selector);
}

[[nodiscard]] sen::Duration toIntervalOrThrow(const MaybeRateHz& rate, std::string_view methodName)
{
  if (!rate.has_value())
  {
    return sen::Duration {};
  }
  if (*rate <= 0.0)
  {
    std::string message {methodName};
    message += ": 'maxRateHz' must be a number > 0";
    throw JsonRpcException(JsonRpcErrorCode::invalidParams, std::move(message));
  }
  return sen::Duration::fromHertz(*rate);
}

[[nodiscard]] std::optional<InterestSubscribeBlock> toInterestSubscribeBlock(const MaybeSubscribeBlock& block)
{
  if (!block.has_value())
  {
    return std::nullopt;
  }
  InterestSubscribeBlock out;
  out.properties = toParsedMemberSelector(block->properties);
  out.events = toParsedMemberSelector(block->events);
  if (block->maxRateHz.has_value())
  {
    if (*block->maxRateHz <= 0.0)
    {
      throw JsonRpcException(JsonRpcErrorCode::invalidParams,
                             "createInterest.subscribe: 'maxRateHz' must be a number > 0");
    }
    out.interval = sen::Duration::fromHertz(*block->maxRateHz);
  }
  return out;
}

/// Per-connection ceiling on live interests. Each interest holds a subscription and (because
/// `getSource` is get-or-create) a bus participant in this process, and the interest count is
/// the one resource a remote peer controls that nothing else bounds. Generous: the web
/// explorer's busiest sessions sit in the low dozens.
constexpr std::size_t maxInterestsPerConnection = 256;

void maybeEraseObjectSubs(InterestEntry& entry, std::unordered_map<std::string, ObjectSubs>::iterator subsIt)
{
  if (const auto& subs = subsIt->second; subs.wildcardActive || !subs.requestedProperties.empty() ||
                                         !subs.requestedEvents.empty() || !subs.explicitProperties.empty() ||
                                         !subs.explicitEvents.empty())
  {
    return;
  }

  entry.objectSubs.erase(subsIt);
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Server
//--------------------------------------------------------------------------------------------------------------

Server::Server(std::string name,
               Identity identity,
               ConnectionId connectionId,
               Dispatcher& dispatcher,
               kernel::RunApi& api)
  : JsonRpcServerBase(std::move(name)), dispatcher_(dispatcher), api_(api), connectionId_(connectionId)
{
  setNextIdentity(identity);  // [static_no_config] member: not a ctor positional
}

Server::~Server()
{
  // Detach quietly: closures on each Subscription capture references into the InterestEntry
  // that's about to be destroyed.
  for (auto& [_, entry]: interests_)
  {
    if (entry.subscription)
    {
      entry.subscription->release(/*notifyAboutExisting=*/false);
    }
  }
}

std::string Server::pingImpl() const { return "pong"; }

::sen::kernel::StringList Server::getTypesImpl() const
{
  ::sen::kernel::StringList names;
  names.reserve(api_.getTypes().getAll().size());
  for (const auto& [qualifiedName, _]: api_.getTypes().getAll())
  {
    names.push_back(qualifiedName);
  }
  return names;
}

TypeLookupResult Server::getTypeImpl(const std::string& qualifiedName, const MaybeFlag& withSchema) const
{
  const auto handle = api_.getTypes().get(qualifiedName);
  if (!handle.has_value())
  {
    throw JsonRpcException(JsonRpcErrorCode::unknownType, "getType: unknown type: " + qualifiedName);
  }
  const auto* customType = (*handle)->asCustomType();
  if (customType == nullptr)
  {
    throw JsonRpcException(JsonRpcErrorCode::unknownType, "getType: not a custom type: " + qualifiedName);
  }

  TypeLookupResult result;
  result.spec = makeWireCustomTypeSpec(*customType);
  if (withSchema.has_value() && *withSchema)
  {
    result.schema = dispatcher_.jsonGenerator().renderTypeSchema(*customType);
  }
  return result;
}

SessionInfoList Server::listTopologyImpl() const { return dispatcher_.topology().snapshot(); }

void Server::subscribeTopologyImpl() { dispatcher_.topology().subscribe(connectionId_); }

void Server::unsubscribeTopologyImpl() { dispatcher_.topology().unsubscribe(connectionId_); }

void Server::createInterestImpl(const std::string& interestName,
                                const std::string& query,
                                const MaybeSubscribeBlock& subscribe,
                                const MaybeFlag& withSchemas)
{
  if (interestName.empty())
  {
    throw JsonRpcException(JsonRpcErrorCode::invalidParams, "createInterest: 'interestName' must be non-empty");
  }
  if (interests_.count(interestName) != 0U)
  {
    throw JsonRpcException(JsonRpcErrorCode::invalidParams, "createInterest: interest already exists: " + interestName);
  }
  if (interests_.size() >= maxInterestsPerConnection)
  {
    throw JsonRpcException(JsonRpcErrorCode::invalidParams,
                           "createInterest: connection holds " + std::to_string(interests_.size()) +
                             " interests (limit " + std::to_string(maxInterestsPerConnection) +
                             "); release unused interests first");
  }

  auto subscribeBlock = toInterestSubscribeBlock(subscribe);
  const bool shipSchemas = withSchemas.has_value() && *withSchemas;

  std::shared_ptr<sen::Interest> interest;
  try
  {
    interest = sen::Interest::make(query, api_.getTypes());
  }
  catch (const std::exception& e)
  {
    throw JsonRpcException(JsonRpcErrorCode::invalidParams, "createInterest: invalid query", std::string {e.what()});
  }

  const auto& busCondition = interest->getBusCondition();
  if (!busCondition.has_value())
  {
    throw JsonRpcException(JsonRpcErrorCode::invalidParams, "createInterest: query must specify a bus");
  }

  // getSource is get-or-create (Runner::getOrCreateLocalParticipant): it cannot fail for a
  // well-formed address, and a query against a bus nothing has published on yet is VALID --
  // the bus comes into being here, the subscription attaches to it empty, and objects flow
  // when a publisher joins the same address. Reconnecting clients that re-declare interests
  // before the server's domain has rebuilt depend on exactly this; do not "validate" bus
  // existence here. (The flip side -- a typo'd bus name yields a forever-empty interest, not
  // an error -- is deliberate; `listTopology` is the discovery tool.)
  auto source = api_.getSource(sen::kernel::BusAddress {busCondition->sessionName, busCondition->busName});

  auto subscription = std::make_shared<sen::Subscription<sen::Object>>();

  auto [iter, inserted] =
    interests_.emplace(interestName, InterestEntry {{}, std::move(subscribeBlock), subscription, {}, shipSchemas});
  SEN_ASSERT(inserted);
  auto& interestEntry = iter->second;

  // Callbacks run on the run() thread. References into the InterestEntry are safe because both
  // `releaseInterestImpl` and `~Server` detach the subscription before destroying the entry.
  auto& dispatcher = dispatcher_;
  auto& api = api_;
  auto* server = this;
  const auto connId = connectionId_;
  std::ignore = subscription->list.onAdded(
    [&dispatcher, &api, server, &interestEntry, connId, interestName](const sen::ObjectList<sen::Object>::Iterators& it)
    {
      for (const auto& obj: it.untyped())
      {
        if (obj)
        {
          interestEntry.objectsByName.insert_or_assign(obj->getName(), obj);
        }
      }
      // Order: block application populates the request set; notification precedes guard wiring
      // so the client never sees `propertyChanged` for an unannounced object.
      applySubscribeBlockToAddedBatch(interestEntry, it);
      emitAddedInterestUpdate(dispatcher, *server, interestEntry, connId, interestName, it);
      rewireAddedBatch(dispatcher, api, interestEntry, connId, interestName, it, server->weak_from_this());
    });
  std::ignore = subscription->list.onRemoved(
    [&dispatcher, &interestEntry, connId, interestName](const sen::ObjectList<sen::Object>::Iterators& it)
    {
      for (const auto& obj: it.untyped())
      {
        if (obj)
        {
          interestEntry.objectsByName.erase(obj->getName());
        }
      }
      dropActiveGuardsForRemovedBatch(interestEntry, it);
      dispatcher.pushNotification(
        connId, "interestUpdate", buildRemovedNotification(interestName, it.typed()), /*reliable=*/true);
    });

  // attachTo() may fire onAdded synchronously, so callbacks must be registered first.
  // notifyAboutExisting=true so the client gets a snapshot of already-present objects.
  subscription->attachTo(std::move(source), interest, true);
}

void Server::releaseInterestImpl(const std::string& interestName)
{
  const auto it = interests_.find(interestName);
  if (it == interests_.end())
  {
    throw JsonRpcException(JsonRpcErrorCode::unknownInterest, "releaseInterest: unknown interest: " + interestName);
  }
  // Detach before erasing - see `~Server`.
  it->second.subscription->release(/*notifyAboutExisting=*/false);
  interests_.erase(it);
}

ObjectInfos Server::listObjectsImpl(const std::string& interestName) const
{
  const auto it = interests_.find(interestName);
  if (it == interests_.end())
  {
    throw JsonRpcException(JsonRpcErrorCode::unknownInterest, "listObjects: unknown interest: " + interestName);
  }
  ObjectInfos result;
  for (const auto& obj: it->second.subscription->list.getUntypedObjects())
  {
    if (obj)
    {
      result.push_back(ObjectInfo {std::string {obj->getName()}, std::string {obj->getClass()->getQualifiedName()}});
    }
  }
  return result;
}

std::string Server::getPropertyImpl(const std::string& interestName,
                                    const std::string& objectName,
                                    const std::string& propertyName) const
{
  const auto it = interests_.find(interestName);
  if (it == interests_.end())
  {
    throw JsonRpcException(JsonRpcErrorCode::unknownInterest, "getProperty: unknown interest: " + interestName);
  }

  auto object = requireObjectInInterest(it->second, "getProperty", objectName);
  const sen::Property& property = requireProperty(*object, "getProperty", propertyName);
  return varToWireJson(object->getPropertyUntyped(&property), *property.getType()).dump();
}

namespace
{
// Read every requested property of one object into the per-object buckets. Property names not
// found on the object's class chain land in `errors`; reads that throw are reported the same way
// so a bad accessor on one property never aborts the whole batch.
void readObjectIntoState(const sen::Object& object, sen::Span<const std::string> propertyNames, ObjectState& out)
{
  for (const std::string& name: propertyNames)
  {
    const auto* property = object.getClass()->searchPropertyByName(name);
    if (property == nullptr)
    {
      out.errors.push_back(PropertyError {name, "unknown property"});
      continue;
    }
    try
    {
      auto encoded = varToWireJson(object.getPropertyUntyped(property), *property->getType()).dump();
      out.properties.push_back(PropertyValuePair {name, std::move(encoded)});
    }
    catch (const std::exception& e)
    {
      out.errors.push_back(PropertyError {name, e.what()});
    }
  }
}

// Collect every property name reachable from the class hierarchy. Preserves declaration order
// from `getProperties(includeParents)` and de-duplicates overrides between parent and child so
// the caller sees one entry per distinct name.
std::vector<std::string> allPropertyNamesFor(const sen::ClassType& classType)
{
  const auto props = classType.getProperties(sen::ClassType::SearchMode::includeParents);
  std::vector<std::string> names;
  names.reserve(props.size());
  std::unordered_set<std::string_view> seen;
  seen.reserve(props.size());
  for (const auto& prop: props)
  {
    const std::string_view name = prop->getName();
    if (seen.emplace(name).second)
    {
      names.emplace_back(name);
    }
  }
  return names;
}
}  // namespace

ObjectStateList Server::getObjectsBatchStateImpl(const std::string& interestName,
                                                 const MaybeStringList& objectNames,
                                                 const MaybeStringList& propertyNames) const
{
  // Bounds the dispatcher-hold time. Checked before resolution so an unbounded objectNames
  // list or a huge live match-set is rejected without paying the per-object walk.
  constexpr std::size_t maxObjectsPerCall = 512;
  if (objectNames.has_value() && objectNames.value().size() > maxObjectsPerCall)
  {
    throw JsonRpcException(JsonRpcErrorCode::invalidParams,
                           "getObjectsBatchState: objectNames size " + std::to_string(objectNames.value().size()) +
                             " exceeds maximum " + std::to_string(maxObjectsPerCall) + "; page the request");
  }

  const auto it = interests_.find(interestName);
  if (it == interests_.end())
  {
    throw JsonRpcException(JsonRpcErrorCode::unknownInterest,
                           "getObjectsBatchState: unknown interest: " + interestName);
  }
  const InterestEntry& entry = it->second;

  // Resolve target object list. When objectNames is omitted, walk the live match set in its
  // natural order. When provided, iterate the caller's list so the response order mirrors it,
  // silently dropping names that aren't in the match set (caller compares requested vs returned).
  std::vector<std::shared_ptr<sen::Object>> targets;
  if (objectNames.has_value())
  {
    targets.reserve(objectNames.value().size());
    for (const std::string& name: objectNames.value())
    {
      if (auto object = findObject(entry, name))
      {
        targets.push_back(std::move(object));
      }
    }
  }
  else
  {
    const auto& live = entry.subscription->list.getUntypedObjects();
    if (live.size() > maxObjectsPerCall)
    {
      throw JsonRpcException(JsonRpcErrorCode::invalidParams,
                             "getObjectsBatchState: match set has " + std::to_string(live.size()) +
                               " objects, exceeds maximum " + std::to_string(maxObjectsPerCall) +
                               "; pass objectNames to page the request");
    }
    targets.reserve(live.size());
    for (const auto& object: live)
    {
      if (object)
      {
        targets.push_back(object);
      }
    }
  }

  // Hoist the explicit property-name list out of the per-object loop (one copy total instead of
  // one per object). For the implicit case, cache the inherited-property walk per class so a
  // batch of N homogeneous-class objects costs one walk, not N.
  std::vector<std::string> explicitNames;
  if (propertyNames.has_value())
  {
    explicitNames.assign(propertyNames.value().begin(), propertyNames.value().end());
  }
  std::unordered_map<const sen::ClassType*, std::vector<std::string>> namesByClass;

  ObjectStateList result;
  result.reserve(targets.size());
  for (const auto& object: targets)
  {
    const std::vector<std::string>* resolvedNames = nullptr;
    if (propertyNames.has_value())
    {
      resolvedNames = &explicitNames;
    }
    else
    {
      const sen::ClassType* classType = object->getClass().type();
      auto cached = namesByClass.find(classType);
      if (cached == namesByClass.end())
      {
        cached = namesByClass.emplace(classType, allPropertyNamesFor(*classType)).first;
      }
      resolvedNames = &cached->second;
    }
    ObjectState state;
    state.objectName = object->getName();
    state.qualifiedClassName = std::string {object->getClass()->getQualifiedName()};
    state.properties.reserve(resolvedNames->size());
    readObjectIntoState(*object, *resolvedNames, state);
    result.push_back(std::move(state));
  }
  return result;
}

void Server::setPropertyImpl(const std::string& /*interestName*/,
                             const std::string& /*objectName*/,
                             const std::string& /*propertyName*/,
                             const std::string& /*value*/)
{
  // Hand-routed in Dispatcher::handleSetProperty; this typed override is required by the
  // codegen-generated abstract base and is unreachable at runtime.
  unreachableHandRouted("setProperty");
}

void Server::subscribePropertyImpl(const std::string& interestName,
                                   const std::string& objectName,
                                   const std::string& propertyName,
                                   const MaybeRateHz& maxRateHz)
{
  auto& entry = requireInterest(*this, "subscribeProperty", interestName);

  auto object = findObject(entry, objectName);
  if (object)
  {
    static_cast<void>(requireProperty(*object, "subscribeProperty", propertyName));
  }

  auto& objectSubs = entry.objectSubs[objectName];
  const bool wasAlreadyWired = objectSubs.activePropertyGuards.count(propertyName) != 0U;
  objectSubs.requestedProperties.insert(propertyName);
  objectSubs.explicitProperties.insert(propertyName);
  if (maxRateHz.has_value())
  {
    objectSubs.interval = toIntervalOrThrow(maxRateHz, "subscribeProperty");
  }
  if (object)
  {
    rewireRequestedSubscriptions(dispatcher_, api_, connectionId_, interestName, object, objectSubs, weak_from_this());
    if (!wasAlreadyWired)
    {
      seedSnapshotsForProperties(
        dispatcher_, connectionId_, interestName, objectName, objectSubs, object, {propertyName});
    }
  }
  checkObjectSubsInvariants(objectSubs);
}

void Server::unsubscribePropertyImpl(const std::string& interestName,
                                     const std::string& objectName,
                                     const std::string& propertyName)
{
  auto interestIt = interests_.find(interestName);
  if (interestIt == interests_.end())
  {
    return;
  }
  auto& entry = interestIt->second;
  auto subsIt = entry.objectSubs.find(objectName);
  if (subsIt == entry.objectSubs.end())
  {
    return;
  }
  auto& subs = subsIt->second;
  subs.explicitProperties.erase(propertyName);

  if (!subs.wildcardActive)
  {
    subs.requestedProperties.erase(propertyName);
    subs.activePropertyGuards.erase(propertyName);
    subs.dirtyProperties.erase(propertyName);
  }

  checkObjectSubsInvariants(subs);
  maybeEraseObjectSubs(entry, subsIt);
}

void Server::subscribeEventImpl(const std::string& interestName,
                                const std::string& objectName,
                                const std::string& eventName)
{
  auto& entry = requireInterest(*this, "subscribeEvent", interestName);

  auto object = findObject(entry, objectName);
  if (object)
  {
    static_cast<void>(requireEvent(*object, "subscribeEvent", eventName));
  }

  auto& objectSubs = entry.objectSubs[objectName];
  objectSubs.requestedEvents.insert(eventName);
  objectSubs.explicitEvents.insert(eventName);
  if (object)
  {
    rewireRequestedSubscriptions(
      dispatcher_, api_, connectionId_, interestName, std::move(object), objectSubs, weak_from_this());
  }
  checkObjectSubsInvariants(objectSubs);
}

void Server::unsubscribeEventImpl(const std::string& interestName,
                                  const std::string& objectName,
                                  const std::string& eventName)
{
  auto interestIt = interests_.find(interestName);
  if (interestIt == interests_.end())
  {
    return;
  }

  auto& entry = interestIt->second;
  auto subsIt = entry.objectSubs.find(objectName);
  if (subsIt == entry.objectSubs.end())
  {
    return;
  }

  auto& subs = subsIt->second;
  subs.explicitEvents.erase(eventName);
  if (!subs.wildcardActive)
  {
    subs.requestedEvents.erase(eventName);
    subs.activeEventGuards.erase(eventName);
  }
  checkObjectSubsInvariants(subs);
  maybeEraseObjectSubs(entry, subsIt);
}

void Server::subscribeAllImpl(const std::string& interestName,
                              const std::string& objectName,
                              const MaybeRateHz& maxRateHz)
{
  auto& entry = requireInterest(*this, "subscribeAll", interestName);

  auto& objectSubs = entry.objectSubs[objectName];

  // Capture currently-wired set so we can seed snapshots only for newly wired properties.
  std::unordered_set<std::string> previouslyWiredProperties;
  previouslyWiredProperties.reserve(objectSubs.activePropertyGuards.size());
  for (const auto& [propName, _]: objectSubs.activePropertyGuards)
  {
    previouslyWiredProperties.insert(propName);
  }

  auto object = findObject(entry, objectName);
  if (!object)
  {
    // Wildcard expansion needs the object's class; there is no durable "wildcard intent" to
    // replay on re-add. Per-name subscribe* still has sticky semantics.
    throw JsonRpcException(JsonRpcErrorCode::objectNotInInterest,
                           "subscribeAll: object not in interest: " + objectName);
  }

  // Set the explicit rate before the block fan-out: the block seeds `interval` only when it is
  // zero, so the explicit value (if any) takes precedence and an absent rate is preserved.
  if (maxRateHz.has_value())
  {
    objectSubs.interval = toIntervalOrThrow(maxRateHz, "subscribeAll");
  }

  // Mark the wildcard intent so subsequent unsubscribeAll knows what to peel back.
  objectSubs.wildcardActive = true;

  // Synthetic wildcard block fans out against the object's class (parents included).
  InterestSubscribeBlock block {};
  block.properties = ParsedMemberSelector {ParsedMemberSelector::Kind::wildcard, {}};
  block.events = ParsedMemberSelector {ParsedMemberSelector::Kind::wildcard, {}};
  applySubscribeBlockToObjectSubs(block, *object, objectSubs);
  rewireRequestedSubscriptions(dispatcher_, api_, connectionId_, interestName, object, objectSubs, weak_from_this());

  std::unordered_set<std::string> newlyWired;
  for (const auto& propName: objectSubs.requestedProperties)
  {
    if (previouslyWiredProperties.count(propName) == 0U)
    {
      newlyWired.insert(propName);
    }
  }
  seedSnapshotsForProperties(dispatcher_, connectionId_, interestName, objectName, objectSubs, object, newlyWired);
  checkObjectSubsInvariants(objectSubs);
}

void Server::unsubscribeAllImpl(const std::string& interestName, const std::string& objectName)
{
  auto interestIt = interests_.find(interestName);
  if (interestIt == interests_.end())
  {
    return;
  }
  auto& entry = interestIt->second;
  auto subsIt = entry.objectSubs.find(objectName);
  if (subsIt == entry.objectSubs.end())
  {
    return;
  }
  auto& subs = subsIt->second;
  if (!subs.wildcardActive)
  {
    // No wildcard to undo; idempotent no-op.
    return;
  }
  subs.wildcardActive = false;

  // Drop the wildcard-only members (those not in the explicit set). Active guards & pending
  // values for them are torn down too.
  std::vector<std::string> dropProperties;
  for (const auto& name: subs.requestedProperties)
  {
    if (subs.explicitProperties.count(name) == 0U)
    {
      dropProperties.push_back(name);
    }
  }
  for (const auto& name: dropProperties)
  {
    subs.requestedProperties.erase(name);
    subs.activePropertyGuards.erase(name);
    subs.dirtyProperties.erase(name);
  }
  std::vector<std::string> dropEvents;
  for (const auto& name: subs.requestedEvents)
  {
    if (subs.explicitEvents.count(name) == 0U)
    {
      dropEvents.push_back(name);
    }
  }
  for (const auto& name: dropEvents)
  {
    subs.requestedEvents.erase(name);
    subs.activeEventGuards.erase(name);
  }

  checkObjectSubsInvariants(subs);
  maybeEraseObjectSubs(entry, subsIt);
}

std::string Server::invokeImpl(const std::string& /*interestName*/,
                               const std::string& /*objectName*/,
                               const std::string& /*methodName*/,
                               const std::string& /*argsJson*/)
{
  // Hand-routed in Dispatcher::handleInvoke; this typed override is required by the
  // codegen-generated abstract base and is unreachable at runtime.
  unreachableHandRouted("invoke");
}

}  // namespace sen::components::jsonrpc
