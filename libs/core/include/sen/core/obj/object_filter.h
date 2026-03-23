// === object_filter.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_OBJECT_FILTER_H
#define SEN_CORE_OBJ_OBJECT_FILTER_H

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_provider.h"

// std
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace sen
{

namespace impl
{

class FilteredProvider;

}

/// \addtogroup obj
/// @{

/// Base class for object filters.
class ObjectFilterBase
{
public:
  SEN_NOCOPY_NOMOVE(ObjectFilterBase)

public:
  ObjectFilterBase() = default;
  virtual ~ObjectFilterBase() = default;

public:
  /// Registers a listener for objects that match the given interest.
  /// @param interest             Filter criteria that an object must satisfy.
  /// @param listener             Callback that receives addition/removal notifications.
  /// @param notifyAboutExisting  If `true`, immediately notify the listener about already-present objects.
  virtual void addSubscriber(std::shared_ptr<Interest> interest,
                             ObjectProviderListener* listener,
                             bool notifyAboutExisting) = 0;

  /// Unregisters a listener for a specific interest.
  /// @param interest             The interest the listener was registered with.
  /// @param listener             The listener to remove.
  /// @param notifyAboutExisting  If `true`, send removal notifications for all currently-tracked objects.
  virtual void removeSubscriber(std::shared_ptr<Interest> interest,
                                ObjectProviderListener* listener,
                                bool notifyAboutExisting) = 0;

  /// Unregisters all subscriptions held by a listener, regardless of interest.
  /// @param listener             The listener to remove completely.
  /// @param notifyAboutExisting  If `true`, send removal notifications for all currently-tracked objects.
  virtual void removeSubscriber(ObjectProviderListener* listener, bool notifyAboutExisting) = 0;
};

/// Allows the discovery of objects based on different criteria.
/// @note: This class is not thread safe.
class ObjectFilter: public ObjectFilterBase
{
public:
  /// Snapshot of object state changes produced during a single evaluation cycle.
  struct ObjectSet
  {
    std::unordered_map<ObjectId, std::shared_ptr<Object>> newObjects;      ///< Objects added since the last evaluation.
    std::unordered_map<ObjectId, std::shared_ptr<Object>> currentObjects;  ///< All objects currently tracked.
    std::unordered_set<ObjectId> deletedObjects;  ///< Objects removed since the last evaluation.
  };

public:
  SEN_NOCOPY_NOMOVE(ObjectFilter)

public:
  explicit ObjectFilter(ObjectOwnerId ownerId);
  ~ObjectFilter() override;

public:  // implements ObjectFilterBase
  void addSubscriber(std::shared_ptr<Interest> interest,
                     ObjectProviderListener* listener,
                     bool notifyAboutExisting) override;
  void removeSubscriber(std::shared_ptr<Interest> interest,
                        ObjectProviderListener* listener,
                        bool notifyAboutExisting) override;
  void removeSubscriber(ObjectProviderListener* listener, bool notifyAboutExisting) override;

public:
  /// Returns an existing provider with the given name and interest, or creates a new one.
  /// @param name     Unique identifier for this provider within the filter.
  /// @param interest Filter criteria the provider should apply.
  /// @return Shared pointer to the (possibly newly created) `ObjectProvider`.
  std::shared_ptr<ObjectProvider> getOrCreateNamedProvider(const std::string& name, std::shared_ptr<Interest> interest);

  /// Removes the named provider, releasing its resources. Does nothing if not found.
  /// @param name Name of the provider to remove.
  void removeNamedProvider(std::string_view name);

protected:
  void evaluate(const ObjectSet& objects);

protected:  // notification to subclasses
  [[nodiscard]] const ObjectOwnerId& getOwnerId() const noexcept;

  virtual void subscriberAdded(std::shared_ptr<Interest> interest,
                               ObjectProviderListener* listener,
                               bool notifyAboutExisting);
  virtual void subscriberRemoved(std::shared_ptr<Interest> interest,
                                 ObjectProviderListener* listener,
                                 bool notifyAboutExisting);
  virtual void subscriberRemoved(ObjectProviderListener* listener, bool notifyAboutExisting);
  virtual void objectsAdded(std::shared_ptr<Interest> interest, ObjectAdditionList additions);
  virtual void objectsRemoved(std::shared_ptr<Interest> interest, ObjectRemovalList removals);

private:  // Interface towards FilteredProvider
  friend class impl::FilteredProvider;

  void providerDeleted(impl::FilteredProvider* provider);

private:
  ObjectOwnerId ownerId_;
  std::vector<std::weak_ptr<impl::FilteredProvider>> providers_;
  std::vector<std::shared_ptr<impl::FilteredProvider>> ownedProviders_;
  std::unordered_map<ObjectId, std::shared_ptr<Object>> lastPresentObjects_;
  std::recursive_mutex usageMutex_;
};

/// @}

}  // namespace sen

#endif  // SEN_CORE_OBJ_OBJECT_FILTER_H
