// === object_filter.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/obj/object_filter.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/lang/vm.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/obj/detail/property_flags.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_provider.h"

// std
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace sen::impl
{

//--------------------------------------------------------------------------------------------------------------
// FilteredProvider
//--------------------------------------------------------------------------------------------------------------

class FilteredProvider final: public ObjectProvider
{
  SEN_NOCOPY_NOMOVE(FilteredProvider)

public:
  FilteredProvider(ObjectFilter* owner, std::shared_ptr<Interest> interest)
    : owner_(owner), interest_(std::move(interest))
  {
  }

  ~FilteredProvider() override
  {
    notifyRemovedOnExistingObjectsForAllListeners();
    owner_->providerDeleted(this);
  }

public:
  [[nodiscard]] const Interest& getInterest() const noexcept { return *interest_; }

  void startTracking(const std::unordered_map<ObjectId, std::shared_ptr<Object>>& objects)
  {
    for (auto& [id, obj]: objects)
    {
      auto* native = obj->asNativeObject();
      if (native == nullptr)
      {
        continue;
      }

      if (trackedObjects_.find(id) != trackedObjects_.end())
      {
        continue;
      }

      // we only track the object if it has the right type
      if (checkTypeCondition(native))
      {
        // save the object and create an environment for it
        trackedObjects_.try_emplace(id, createTrackedObject(native));
      }
    }
  }

  void stopTrackingAndNotify(const std::unordered_set<ObjectId>& objects)
  {
    // new removals computed in this pass
    std::vector<ObjectRemoval> newRemovals;
    newRemovals.reserve(objects.size());

    for (auto& objectId: objects)
    {
      auto trackedObjectItr = trackedObjects_.find(objectId);

      if (trackedObjectItr == trackedObjects_.end())
      {
        continue;
      }

      // if the user was notified, mark it for removal
      if (trackedObjectItr->second.notified)
      {
        auto additionItr = currentAdditions_.find(objectId);
        if (additionItr != currentAdditions_.end())
        {
          newRemovals.push_back(makeRemoval(additionItr->second));
          currentAdditions_.erase(additionItr);
        }
      }

      trackedObjects_.erase(trackedObjectItr);
    }

    if (!newRemovals.empty())
    {
      notifyObjectsRemoved(newRemovals);
    }
  }

  void evaluateTrackedObjects()
  {
    // clear it because we will recompute this here
    currentAdditions_.clear();

    // reserve space to avoid re-allocations
    currentAdditions_.reserve(trackedObjects_.size());
    newAdditionsCache_.reserve(trackedObjects_.size());
    newRemovalsCache_.reserve(trackedObjects_.size());

    for (auto& [trackedObjId, trackedObj]: trackedObjects_)
    {
      if (shouldBePresent(trackedObj))
      {
        // store it
        auto [additionsItr, done] =
          currentAdditions_.try_emplace(trackedObjId,
                                        ObjectInstanceDiscovery {trackedObj.instance->shared_from_this(),
                                                                 trackedObj.instance->getId(),
                                                                 interest_->getId(),
                                                                 owner_->getOwnerId()});

        // if not present, make it so
        if (!trackedObj.notified)
        {
          newAdditionsCache_.push_back(additionsItr->second);
          trackedObj.notified = true;
        }
      }
      else
      {
        // the object should not be present. We don't include it into the addedObjects list.
        if (trackedObj.notified)
        {
          // not passing and present, we need to remove it
          newRemovalsCache_.emplace_back(
            ObjectRemoval {interest_->getId(), trackedObj.instance->getId(), owner_->getOwnerId()});

          // invalidate the index in the tracked object data
          trackedObj.notified = false;
        }
      }
    }

    // notify users about the new ones
    if (!newAdditionsCache_.empty())
    {
      notifyObjectsAdded(newAdditionsCache_);
      newAdditionsCache_.clear();
    }

    // notify users about the gone ones
    if (!newRemovalsCache_.empty())
    {
      notifyObjectsRemoved(newRemovalsCache_);
      newRemovalsCache_.clear();
    }
  }

  [[nodiscard]] bool hasName() const noexcept { return !name_.empty(); }

  [[nodiscard]] const std::string& getName() const noexcept { return name_; }

  void setName(std::string_view name) { name_ = name; }

  void clearName() { name_.clear(); }

protected:  // implements ObjectProvider
  void notifyAddedOnExistingObjects(ObjectProviderListener* listener) override
  {
    if (currentAdditions_.empty())
    {
      return;
    }

    std::vector<ObjectAddition> additions;
    additions.reserve(currentAdditions_.size());

    for (const auto& [id, addition]: currentAdditions_)
    {
      additions.push_back(addition);
    }

    callOnObjectsAdded(listener, additions);
  }

  void notifyRemovedOnExistingObjects(ObjectProviderListener* listener) override
  {
    if (currentAdditions_.empty())
    {
      return;
    }

    std::vector<ObjectRemoval> removals;
    removals.reserve(currentAdditions_.size());

    for (const auto& [first, second]: currentAdditions_)
    {
      removals.push_back(makeRemoval(second));
    }

    callOnObjectsRemoved(listener, removals);
  }

  void listenerAdded(ObjectProviderListener* listener, bool notifyAboutExistingObjects) override
  {
    owner_->subscriberAdded(interest_, listener, notifyAboutExistingObjects);
  }

  void listenerRemoved(ObjectProviderListener* listener, bool notifyAboutExistingObjects) override
  {
    owner_->subscriberRemoved(interest_, listener, notifyAboutExistingObjects);
  }

  void notifyObjectsAdded(const ObjectAdditionList& additions) override
  {
    ObjectProvider::notifyObjectsAdded(additions);
    owner_->objectsAdded(interest_, additions);
  }

  void notifyObjectsRemoved(const ObjectRemovalList& removals) override
  {
    ObjectProvider::notifyObjectsRemoved(removals);
    owner_->objectsRemoved(interest_, removals);
  }

private:
  struct VarInfo
  {
    const Property* property = nullptr;
    std::vector<uint16_t> fieldIndexes;
  };

  struct TrackedObject
  {
    Object* instance = nullptr;
    std::vector<lang::ValueGetter> environment;
    std::vector<const impl::PropertyFlags*> dependantProps;
    bool notified = false;
    bool alreadyEvaluatedOnce = false;
  };

private:
  [[nodiscard]] bool checkTypeCondition(const Object* object) const
  {
    const auto& typeCondition = interest_->getTypeCondition();

    // empty type means any type
    if (std::holds_alternative<std::monostate>(typeCondition))
    {
      return true;
    }

    if (std::holds_alternative<ConstTypeHandle<ClassType>>(typeCondition))
    {
      auto classType = std::get<ConstTypeHandle<ClassType>>(typeCondition);
      return object->getClass()->isSameOrInheritsFrom(*classType);
    }

    const auto& className = std::get<std::string>(typeCondition);
    return object->getClass()->isSameOrInheritsFrom(className);
  }

  [[nodiscard]] bool shouldBePresent(TrackedObject& track) const
  {
    if (invalidQuery_)
    {
      return false;
    }

    const auto& queryCode = interest_->getQueryCode();
    if (!queryCode.isValid())
    {
      return true;
    }

    if (!track.alreadyEvaluatedOnce || needsReEvaluation(track))
    {
      track.alreadyEvaluatedOnce = true;
      lang::VM vm;
      return std::get<bool>(vm.interpret(queryCode, track.environment).getValue());
    }

    // keep the same previous result
    return track.notified;
  }

  [[nodiscard]] bool needsReEvaluation(const TrackedObject& track) const noexcept
  {
    return std::any_of(track.dependantProps.begin(),
                       track.dependantProps.end(),
                       [](const auto* prop) { return prop->changedInLastCycle(); });
  }

  [[nodiscard]] TrackedObject createTrackedObject(NativeObject* instance)
  {
    auto& queryCode = interest_->getQueryCode();

    TrackedObject result;
    result.notified = false;
    result.alreadyEvaluatedOnce = false;
    result.instance = instance;

    if (!queryCode.isValid() || queryCode.getVariables().empty())
    {
      return result;
    }

    auto vars = queryCode.getVariables();
    result.dependantProps.reserve(vars.size());
    result.environment.reserve(vars.size());

    VarInfoList varInfoList;

    try
    {
      varInfoList = interest_->getOrComputeVarInfoList(instance->getClass().type());
    }
    catch (const std::exception& err)
    {
      std::ignore = err;
      invalidQuery_ = true;
      return result;
    }

    SEN_ASSERT(varInfoList.size() == vars.size());

    for (std::size_t varIndex = 0U; varIndex < vars.size(); ++varIndex)
    {
      const auto& var = vars[varIndex];

      // add built-in variables
      if (var == "name")
      {
        result.environment.emplace_back([instance]() { return instance->getName(); });
        continue;
      }
      if (var == "id")
      {
        result.environment.emplace_back([instance]() { return instance->getId().get(); });
        continue;
      }

      auto& varInfo = varInfoList.at(varIndex);
      auto fieldData = instance->senImplGetFieldValueGetter(varInfo.property->getId(), varInfo.fieldIndexes);
      result.environment.emplace_back(fieldData.getterFunc);

      if (fieldData.flags)
      {
        result.dependantProps.push_back(fieldData.flags);
      }
    }

    return result;
  }

private:
  ObjectFilter* owner_;
  std::shared_ptr<Interest> interest_;
  std::string name_;
  bool invalidQuery_ = false;
  std::unordered_map<ObjectId, ObjectAddition> currentAdditions_;
  std::unordered_map<ObjectId, TrackedObject> trackedObjects_;
  std::vector<ObjectAddition> newAdditionsCache_;
  std::vector<ObjectRemoval> newRemovalsCache_;
};

}  // namespace sen::impl

//--------------------------------------------------------------------------------------------------------------
// ObjectFilter
//--------------------------------------------------------------------------------------------------------------

namespace sen
{

ObjectFilter::ObjectFilter(ObjectOwnerId ownerId): ownerId_(ownerId) {}

// defined here to allow FilteredProvider destructor to run
ObjectFilter::~ObjectFilter() = default;

std::shared_ptr<ObjectProvider> ObjectFilter::getOrCreateNamedProvider(const std::string& name,
                                                                       std::shared_ptr<Interest> interest)
{
  std::scoped_lock<std::recursive_mutex> lock(usageMutex_);

  // check if the provider for this interest already exists
  for (const auto& providerw: providers_)
  {
    if (const auto provider = providerw.lock(); provider)
    {
      if (provider->getInterest() == *interest)
      {
        if (provider->hasName())
        {
          if (provider->getName() != name)
          {
            std::string err;
            err.append("there is already a source with the same interest specification but named '");
            err.append(provider->getName());
            err.append("' instead of '");
            err.append(name);
            err.append("'");
            throwRuntimeError(err);
          }
          return provider->shared_from_this();
        }
        provider->setName(name);
        return provider->shared_from_this();
      }
    }
  }

  // not present, so we need to create one
  auto ptr = std::make_shared<impl::FilteredProvider>(this, std::move(interest));
  providers_.push_back(ptr);

  ptr->setName(name);
  ptr->startTracking(lastPresentObjects_);
  ptr->evaluateTrackedObjects();

  return ptr;
}

void ObjectFilter::removeNamedProvider(std::string_view name)
{
  std::scoped_lock<std::recursive_mutex> lock(usageMutex_);

  for (std::size_t i = 0U; i < providers_.size(); ++i)
  {
    if (const auto provider = providers_[i].lock(); provider)
    {
      if (provider->hasName() && provider->getName() == name)
      {
        provider->clearName();

        // delete the provider if there are no listeners
        if (!provider->hasListeners())
        {
          providers_.erase(providers_.begin() + i);  // NOLINT(bugprone-narrowing-conversions)
        }
        return;
      }
    }
  }
}

void ObjectFilter::providerDeleted(impl::FilteredProvider* provider)
{
  std::scoped_lock<std::recursive_mutex> lock(usageMutex_);
  providers_.erase(std::remove_if(providers_.begin(),
                                  providers_.end(),
                                  [provider](auto providerw)
                                  {
                                    auto providerinner = providerw.lock();
                                    return providerinner.get() == provider;
                                  }),
                   providers_.end());
}

void ObjectFilter::addSubscriber(std::shared_ptr<Interest> interest,
                                 ObjectProviderListener* listener,
                                 bool notifyAboutExisting)
{
  std::scoped_lock<std::recursive_mutex> lock(usageMutex_);

  // check if the provider for this interest already exists
  for (const auto& provider: ownedProviders_)
  {
    if (provider->getInterest() == *interest)
    {
      provider->addListener(listener, notifyAboutExisting);
      return;
    }
  }

  // create the provider
  ownedProviders_.push_back(std::make_shared<impl::FilteredProvider>(this, interest));
  providers_.push_back(ownedProviders_.back());
  auto provider = providers_.back().lock();
  provider->addListener(listener, true);

  if (notifyAboutExisting)
  {
    provider->startTracking(lastPresentObjects_);
    provider->evaluateTrackedObjects();
  }
}

void ObjectFilter::removeSubscriber(std::shared_ptr<Interest> interest,
                                    ObjectProviderListener* listener,
                                    bool notifyAboutExisting)
{
  std::scoped_lock<std::recursive_mutex> lock(usageMutex_);

  // explicit copy to prevent modifications
  auto providers = providers_;
  for (const auto& providerw: providers)
  {
    auto provider = providerw.lock();
    if (!provider)
    {
      continue;
    }
    if (provider->getInterest() == *interest)
    {
      provider->removeListener(listener, notifyAboutExisting);
    }
  }

  // delete the provider if there are no listeners and is not named
  ownedProviders_.erase(
    std::remove_if(ownedProviders_.begin(),
                   ownedProviders_.end(),
                   [](const auto& elem) -> bool { return !elem->hasName() && !elem->hasListeners(); }),
    ownedProviders_.end());
}

void ObjectFilter::removeSubscriber(ObjectProviderListener* listener, bool notifyAboutExisting)
{
  std::scoped_lock<std::recursive_mutex> lock(usageMutex_);

  // explicit copy to prevent modifications
  auto providers = providers_;
  for (const auto& providerw: providers)
  {
    auto provider = providerw.lock();
    if (!provider)
    {
      continue;
    }
    provider->removeListener(listener, notifyAboutExisting);
  }

  ownedProviders_.erase(
    std::remove_if(
      ownedProviders_.begin(), ownedProviders_.end(), [](const auto& elem) -> bool { return !elem->hasListeners(); }),
    ownedProviders_.end());
}

void ObjectFilter::evaluate(const ObjectSet& objects)
{
  std::scoped_lock<std::recursive_mutex> lock(usageMutex_);

  lastPresentObjects_ = objects.currentObjects;

  // check the removed objects
  if (!objects.deletedObjects.empty())
  {
    // explicit copy to prevent modifications
    auto providers = providers_;
    for (const auto& providerw: providers)
    {
      if (const auto provider = providerw.lock(); provider)
      {
        provider->stopTrackingAndNotify(objects.deletedObjects);
      }
    }
  }

  // check the added objects
  if (!objects.newObjects.empty())
  {
    for (const auto& providerw: providers_)
    {
      if (const auto provider = providerw.lock(); provider)
      {
        provider->startTracking(objects.newObjects);
      }
    }

    lastPresentObjects_.insert(objects.newObjects.begin(), objects.newObjects.end());
  }

  // explicit copy to prevent modifications
  auto providers = providers_;
  for (const auto& provider: providers)
  {
    if (auto lockedProvider = provider.lock(); lockedProvider)
    {
      lockedProvider->evaluateTrackedObjects();
    }
  }
}

void ObjectFilter::subscriberAdded(std::shared_ptr<Interest> interest,
                                   ObjectProviderListener* listener,
                                   bool notifyAboutExisting)
{
  std::ignore = interest;
  std::ignore = listener;
  std::ignore = notifyAboutExisting;
}

void ObjectFilter::subscriberRemoved(std::shared_ptr<Interest> interest,  // NOSONAR
                                     ObjectProviderListener* listener,
                                     bool notifyAboutExisting)
{
  std::ignore = interest;
  std::ignore = listener;
  std::ignore = notifyAboutExisting;
}

void ObjectFilter::subscriberRemoved(ObjectProviderListener* listener, bool notifyAboutExisting)
{
  std::ignore = listener;
  std::ignore = notifyAboutExisting;
}

void ObjectFilter::objectsAdded(std::shared_ptr<Interest> interest, ObjectAdditionList additions)
{
  std::ignore = interest;
  std::ignore = additions;
}

void ObjectFilter::objectsRemoved(std::shared_ptr<Interest> interest, ObjectRemovalList removals)
{
  std::ignore = interest;
  std::ignore = removals;
}

const ObjectOwnerId& ObjectFilter::getOwnerId() const noexcept { return ownerId_; }

}  // namespace sen
