// === object_store.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "object_store.h"

// component
#include "util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/result.h"
#include "sen/core/io/util.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_list.h"
#include "sen/kernel/component_api.h"

// std
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <list>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace sen::components::term
{

namespace
{

/// The suffix of the internal provider that carries every object on a bus, as opposed to a named
/// query. The test was written out three times, each recomputing the same magic 4.
constexpr std::string_view allProviderSuffix = ".all";

bool isAllProvider(std::string_view providerName)
{
  return providerName.size() >= allProviderSuffix.size() &&
         providerName.compare(
           providerName.size() - allProviderSuffix.size(), allProviderSuffix.size(), allProviderSuffix) == 0;
}

}  // namespace

using sen::std_util::checkedConversion;

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

constexpr std::size_t tierOneMax = 5;
constexpr std::size_t tierTwoMax = 50;

/// Strip the component prefix (first segment) from an object's localName.
/// "showcaseComponent.local.demo.showcase" -> "local.demo.showcase"
std::string stripComponentPrefix(std::string_view localName)
{
  auto dot = localName.find('.');
  return (dot != std::string_view::npos) ? std::string(localName.substr(dot + 1)) : std::string(localName);
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Construction
//--------------------------------------------------------------------------------------------------------------

ObjectStore::ObjectStore(kernel::RunApi& api): api_(api)
{
  objects_ = std::make_unique<ObjectList<Object>>(mux_, 10U);

  std::ignore = objects_->onAdded(
    [this](const auto& iterators)
    {
      // This runs from drainInputs, before the work function, so the frame's own boundary in
      // component.cpp cannot see it. What arrives is a peer's data, and a throw here reached
      // std::terminate and took every component in the process with it.
      try
      {
        auto count = checkedConversion<std::size_t>(std::distance(iterators.untypedBegin, iterators.untypedEnd));
        if (count == 0)
        {
          return;
        }

        ++generation_;
        if (onObjectAdded_)
        {
          for (auto itr = iterators.untypedBegin; itr != iterators.untypedEnd; ++itr)
          {
            onObjectAdded_(*itr);
          }
        }

        if (suppressNotifications_)
        {
          return;
        }

        if (count <= tierOneMax)
        {
          for (auto itr = iterators.untypedBegin; itr != iterators.untypedEnd; ++itr)
          {
            const auto& obj = *itr;
            auto typeName = std::string(obj->getClass()->getName());
            auto objName = stripComponentPrefix(obj->getLocalName());
            bool isRemote = obj->asProxyObject() != nullptr && obj->asProxyObject()->isRemote();
            std::string origin = isRemote ? "Remote" : "Native";
            std::ostringstream line;
            line << " + " << objName << "  [" << typeName << ", " << origin << "]";
            notify({line.str(), true});
          }
        }
        else if (count <= tierTwoMax)
        {
          notify({" + " + std::to_string(count) + " objects detected", true});
        }
        else
        {
          std::set<std::string> types;
          for (auto itr = iterators.untypedBegin; itr != iterators.untypedEnd; ++itr)
          {
            types.insert(std::string((*itr)->getClass()->getName()));
          }
          notify(
            {" + " + std::to_string(count) + " objects detected (" + std::to_string(types.size()) + " types)", true});
        }
      }
      catch (const std::exception& e)
      {
        getLogger()->error("a discovery notification could not be handled: {}", e.what());
      }
    });

  std::ignore = objects_->onRemoved(
    [this](const auto& iterators)
    {
      // Same boundary as the add handler above, and for the same reason.
      try
      {
        auto count = checkedConversion<std::size_t>(std::distance(iterators.untypedBegin, iterators.untypedEnd));
        if (count == 0)
        {
          return;
        }

        ++generation_;
        if (onObjectRemoved_)
        {
          for (auto itr = iterators.untypedBegin; itr != iterators.untypedEnd; ++itr)
          {
            onObjectRemoved_(*itr);
          }
        }

        if (suppressNotifications_)
        {
          return;
        }

        if (count <= tierOneMax)
        {
          for (auto itr = iterators.untypedBegin; itr != iterators.untypedEnd; ++itr)
          {
            notify({" - " + stripComponentPrefix((*itr)->getLocalName()) + "  [removed]", true});
          }
        }
        else
        {
          notify({" - " + std::to_string(count) + " objects removed", true});
        }
      }
      catch (const std::exception& e)
      {
        getLogger()->error("a discovery notification could not be handled: {}", e.what());
      }
    });
}

//--------------------------------------------------------------------------------------------------------------
// Source management
//--------------------------------------------------------------------------------------------------------------

Result<void, std::string> ObjectStore::openSource(std::string_view sourceName)
{
  suppressNotifications_ = false;

  auto components = impl::split(std::string(sourceName));

  if (components.size() == 1)
  {
    auto sessionName = std::string(sourceName);
    if (openSessions_.find(sessionName) != openSessions_.end())
    {
      return Ok();
    }

    openSessions_.try_emplace(sessionName, api_.getSessionsDiscoverer().makeSessionInfoProvider(sessionName));
    notify({" + source " + sessionName + " opened", true});
    return Ok();
  }

  if (components.size() == 2)
  {
    auto id = std::string(sourceName);
    if (openSources_.find(id) != openSources_.end())
    {
      return Ok();
    }

    if (components[0].empty() || components[1].empty())
    {
      return Err(std::string("invalid source name"));
    }

    try
    {
      auto& sourceData = getOrOpenSource(components[0], components[1]);

      std::string providerName = id + std::string(allProviderSuffix);
      if (sourceData.providers.find(providerName) != sourceData.providers.end())
      {
        return Ok();
      }

      ProviderData pd;
      pd.interest = Interest::make("SELECT * FROM " + id, api_.getTypes());
      pd.provider = sourceData.source->getOrCreateNamedProvider(providerName, pd.interest);
      pd.provider->addListener(&mux_, true);
      sourceData.providers.try_emplace(providerName, std::move(pd));

      notify({" + source " + id + " opened", true});
      return Ok();
    }
    catch (const std::exception& err)
    {
      return Err(std::string(err.what()));
    }
  }

  return Err(std::string("invalid source name (expected session or session.bus)"));
}

Result<void, std::string> ObjectStore::closeSource(std::string_view sourceName)
{
  auto components = impl::split(std::string(sourceName));
  std::vector<std::string> sourcesToClose;
  const auto& sessionName = components.front();

  if (components.size() == 1)
  {
    openSessions_.erase(sessionName);
    for (const auto& [busAddress, sourceData]: openSources_)
    {
      auto busComponents = impl::split(busAddress);
      if (busComponents.front() == sessionName)
      {
        sourcesToClose.push_back(busAddress);
      }
    }
  }
  else if (components.size() == 2)
  {
    auto id = std::string(sourceName);
    if (openSources_.find(id) != openSources_.end())
    {
      sourcesToClose.push_back(id);
    }
  }
  else if (components.size() == 3)
  {
    std::string busId = components[0] + "." + components[1];
    auto sourceItr = openSources_.find(busId);
    if (sourceItr == openSources_.end())
    {
      return Err(busId + " is not open");
    }

    auto queryId = std::string(sourceName);
    auto providerItr = sourceItr->second.providers.find(queryId);
    if (providerItr == sourceItr->second.providers.end())
    {
      return Err(queryId + " not found");
    }

    sourceItr->second.source->removeNamedProvider(queryId);
    providerItr->second.provider->removeListener(&mux_, true);
    sourceItr->second.providers.erase(providerItr);

    if (sourceItr->second.providers.empty())
    {
      sourcesToClose.push_back(busId);
    }
    else
    {
      notify({" - query " + queryId + " closed", true});
      return Ok();
    }
  }
  else
  {
    return Err(std::string("invalid source name"));
  }

  for (const auto& elem: sourcesToClose)
  {
    auto itr = openSources_.find(elem);
    if (itr == openSources_.end())
    {
      continue;
    }

    for (const auto& [providerName, providerData]: itr->second.providers)
    {
      itr->second.source->removeNamedProvider(providerName);
      providerData.provider->removeListener(&mux_, true);
    }
    openSources_.erase(itr);
  }

  if (!sourcesToClose.empty())
  {
    notify({" - source " + std::string(sourceName) + " closed", true});
  }

  return Ok();
}

//--------------------------------------------------------------------------------------------------------------
// Queries
//--------------------------------------------------------------------------------------------------------------

Result<void, std::string> ObjectStore::createQuery(std::string_view name, std::string_view selection)
{
  suppressNotifications_ = false;

  auto nameStr = std::string(name);
  if (nameStr.find_first_of(" .") != std::string::npos)
  {
    return Err(std::string("query name cannot contain spaces or dots"));
  }
  if (nameStr == "all")
  {
    return Err(std::string("'all' is reserved; choose a different query name"));
  }

  std::shared_ptr<Interest> interest;
  try
  {
    interest = Interest::make(selection, api_.getTypes());
  }
  catch (const std::exception& err)
  {
    return Err(std::string("invalid query: ") + err.what());
  }

  if (!interest->getBusCondition().has_value())
  {
    return Err(std::string("query must specify a source (FROM session.bus)"));
  }

  const auto& busCondition = interest->getBusCondition().value();

  // Inside a try, like the provider work below it and like openSource's own call. It used to sit between
  // the two trys, uncovered -- and CommandEngine's constructor calls this for every `query:` entry in
  // the configuration, from inside run(), where nothing above it catches. So a session or bus name a
  // user mistyped in a config file terminated the kernel during startup, while the same name typed at
  // the `query` command was reported politely, because that path goes through execute's try.
  SourceData* sourceDataPtr = nullptr;
  try
  {
    sourceDataPtr = &getOrOpenSource(busCondition.sessionName, busCondition.busName);
  }
  catch (const std::exception& err)
  {
    return Err("cannot open " + busCondition.sessionName + "." + busCondition.busName + ": " + err.what());
  }
  auto& sourceData = *sourceDataPtr;

  std::string providerId = busCondition.sessionName + "." + busCondition.busName + "." + nameStr;

  for (const auto& [existingQuery, existingSelection]: querySelections_)
  {
    if (existingSelection == selection)
    {
      return Err("a query named '" + existingQuery + "' already exists with the same definition");
    }
  }

  for (const auto& [existingName, existingData]: sourceData.providers)
  {
    if (existingName == providerId)
    {
      return Err("query '" + nameStr + "' already exists");
    }
    // Skip the internal ".all" provider when checking for duplicate selections.
    if (isAllProvider(existingName))
    {
      continue;
    }
    if (existingData.interest->getQueryString() == selection)
    {
      return Err("a query named '" + existingName + "' already exists with the same definition");
    }
  }

  // If the internal ".all" provider already has the same interest, reuse it
  // instead of creating a duplicate. This happens when the user creates a
  // "SELECT * FROM bus" query on an already-opened bus.
  std::string allProviderId = busCondition.sessionName + "." + busCondition.busName + std::string(allProviderSuffix);
  auto allIt = sourceData.providers.find(allProviderId);
  bool reusingAll = (allIt != sourceData.providers.end() && allIt->second.interest->getQueryString() == selection);

  try
  {
    if (!reusingAll)
    {
      ProviderData pd;
      pd.interest = interest;
      pd.provider = sourceData.source->getOrCreateNamedProvider(providerId, pd.interest);
      pd.provider->addListener(&mux_, true);
      sourceData.providers.try_emplace(providerId, std::move(pd));
    }

    auto busAddr = busCondition.sessionName + "." + busCondition.busName;
    auto sub = api_.selectFrom<Object>(busAddr, std::string(selection));
    querySubscriptions_.emplace(nameStr, std::move(sub));
    querySelections_.emplace(nameStr, std::string(selection));
  }
  catch (const std::exception& err)
  {
    return Err(std::string(err.what()));
  }

  notify({" + query '" + nameStr + "' created", true});
  return Ok();
}

Result<void, std::string> ObjectStore::removeQuery(std::string_view name)
{
  auto nameStr = std::string(name);

  for (auto& [busId, sourceData]: openSources_)
  {
    for (auto itr = sourceData.providers.begin(); itr != sourceData.providers.end(); ++itr)
    {
      auto dotPos = itr->first.rfind('.');
      if (dotPos != std::string::npos && itr->first.substr(dotPos + 1) == nameStr)
      {
        sourceData.source->removeNamedProvider(itr->first);
        itr->second.provider->removeListener(&mux_, true);
        sourceData.providers.erase(itr);
        querySubscriptions_.erase(nameStr);
        querySelections_.erase(nameStr);
        notify({" - query '" + nameStr + "' removed", true});
        return Ok();
      }
    }
  }

  // No provider of its own: this is a query that reused the internal ".all" provider, which is the case
  // that used to be unremovable. The subscription is the thing to release.
  if (querySelections_.erase(nameStr) != 0U)
  {
    querySubscriptions_.erase(nameStr);
    notify({" - query '" + nameStr + "' removed", true});
    return Ok();
  }

  return Err("query '" + nameStr + "' not found");
}

//--------------------------------------------------------------------------------------------------------------
// Accessors
//--------------------------------------------------------------------------------------------------------------

bool ObjectStore::isSourceOpen(std::string_view sourceName) const
{
  return openSources_.find(std::string(sourceName)) != openSources_.end();
}

std::vector<std::string> ObjectStore::getOpenSources() const
{
  std::vector<std::string> result;
  result.reserve(openSources_.size());
  for (const auto& [name, data]: openSources_)
  {
    result.push_back(name);
  }
  return result;
}

std::vector<std::shared_ptr<Object>> ObjectStore::getQueryObjects(std::string_view queryName) const
{
  auto it = querySubscriptions_.find(std::string(queryName));
  if (it == querySubscriptions_.end())
  {
    return {};
  }
  // The subscription's ObjectList<Object> tracks only the objects matching the query.
  std::vector<std::shared_ptr<Object>> result;
  for (const auto* obj: it->second->list.getObjects())
  {
    result.push_back(std::const_pointer_cast<Object>(obj->shared_from_this()));
  }
  return result;
}

std::vector<ObjectStore::QueryInfo> ObjectStore::getQueries() const
{
  // From the selection record, not from the providers: a query that reused the internal ".all" provider
  // has none of its own, and listing by provider left it invisible to `queries` and to completion while
  // it went on holding a subscription.
  std::vector<QueryInfo> result;
  result.reserve(querySelections_.size());
  for (const auto& [name, selection]: querySelections_)
  {
    result.push_back({name, selection});
  }
  return result;
}

std::vector<std::string> ObjectStore::getAvailableSources() const
{
  std::vector<std::string> result;
  auto sessions = api_.getSessionsDiscoverer().getDetectedSources();
  for (const auto& sessionName: sessions)
  {
    auto itr = openSessions_.find(sessionName);
    if (itr != openSessions_.end())
    {
      auto buses = itr->second->getDetectedSources();
      for (const auto& busName: buses)
      {
        result.push_back(std::string(sessionName).append(".").append(busName));
      }
    }
    else
    {
      result.push_back(sessionName);
    }
  }
  return result;
}

const std::list<std::shared_ptr<Object>>& ObjectStore::getObjects() const { return objects_->getUntypedObjects(); }

std::size_t ObjectStore::getObjectCount() const { return objects_->getUntypedObjects().size(); }

uint64_t ObjectStore::getGeneration() const noexcept { return generation_; }

void ObjectStore::setObjectAddedCallback(ObjectEventCallback callback) { onObjectAdded_ = std::move(callback); }

void ObjectStore::setObjectRemovedCallback(ObjectEventCallback callback) { onObjectRemoved_ = std::move(callback); }

std::vector<DiscoveryNotification> ObjectStore::drainNotifications()
{
  std::vector<DiscoveryNotification> result;
  result.swap(pendingNotifications_);

  if (droppedNotifications_ > 0)
  {
    result.push_back({std::to_string(droppedNotifications_) + " further discovery notices were not shown", false});
    droppedNotifications_ = 0;
  }
  return result;
}

void ObjectStore::notify(DiscoveryNotification notification)
{
  // Discovery pushes one of these per object, and the drain is once per frame. Beyond the bound they are
  // counted and the count is reported, which is what a user needs from a burst of ten thousand anyway.
  constexpr std::size_t maxPendingNotifications = 500;
  if (pendingNotifications_.size() >= maxPendingNotifications)
  {
    ++droppedNotifications_;
    return;
  }
  pendingNotifications_.push_back(std::move(notification));
}

//--------------------------------------------------------------------------------------------------------------
// Internal
//--------------------------------------------------------------------------------------------------------------

ObjectStore::SourceData& ObjectStore::getOrOpenSource(std::string_view sessionName, std::string_view busName)
{
  std::string id = std::string(sessionName) + "." + std::string(busName);

  if (auto itr = openSources_.find(id); itr != openSources_.end())
  {
    return itr->second;
  }

  auto sessionStr = std::string(sessionName);
  if (openSessions_.find(sessionStr) == openSessions_.end())
  {
    openSessions_.try_emplace(sessionStr, api_.getSessionsDiscoverer().makeSessionInfoProvider(sessionStr));
  }

  SourceData sourceData;
  sourceData.source = api_.getSource(id);
  if (!sourceData.source)
  {
    throwRuntimeError("could not open source '" + id + "'");
  }

  openSources_.try_emplace(id, std::move(sourceData));
  return openSources_[id];
}

}  // namespace sen::components::term
