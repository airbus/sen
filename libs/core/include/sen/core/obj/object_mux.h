// === object_mux.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_OBJECT_MUX_H
#define SEN_CORE_OBJ_OBJECT_MUX_H

// sen
#include "sen/core/obj/object_provider.h"

// std
#include <mutex>

namespace sen
{

/// \addtogroup obj
/// @{

// forward declaration
class ObjectMux;

/// An extended ObjectProviderListener that works together with ObjectMux and notifies when
/// objects provided by different ObjectMux sources are added/removed multiple times.
class MuxedProviderListener: public ObjectProviderListener
{
  SEN_NOCOPY_NOMOVE(MuxedProviderListener)

public:
  MuxedProviderListener() = default;
  ~MuxedProviderListener() override;

public:
  /// Called when an object is added that is already present in the ObjectMux's list
  virtual void onExistingObjectsReadded(const ObjectAdditionList& additions) = 0;

  /// Called when an object is removed, but other providers for the ObjectMux also provide it
  virtual void onObjectsRefCountReduced(const ObjectRemovalList& removals) = 0;

private:
  friend class ObjectMux;
  void addMuxedProvider(ObjectMux* provider);
  void removeMuxedProvider(ObjectMux* provider);

private:
  std::vector<ObjectMux*> muxedProviders_;
};

/// Converts multiple object providers into one.
/// Providers might reside in different threads.
/// Note that addition and removal notifications for objects that are matched by overlapping
/// interests might mention different interest IDs.
class ObjectMux final: public ObjectProviderListener, public ObjectProvider
{
public:
  SEN_NOCOPY_NOMOVE(ObjectMux)

public:
  ObjectMux() = default;
  ~ObjectMux() override = default;

public:  // implements ObjectProvider (public methods)
  void addListener(ObjectProviderListener* listener, bool notifyAboutExistingObjects) override;
  void removeListener(ObjectProviderListener* listener, bool notifyAboutExistingObjects) override;
  [[nodiscard]] bool hasListener(ObjectProviderListener* listener) const noexcept override;
  [[nodiscard]] bool hasListeners() const noexcept override;

public:
  void addMuxedListener(MuxedProviderListener* listener, bool notifyAboutExistingObjects);
  void removeMuxedListener(MuxedProviderListener* listener, bool notifyAboutExistingObjects);
  [[nodiscard]] bool hasMuxedListener(MuxedProviderListener* listener) const noexcept;

protected:  // implements ObjectProvider (protected methods)
  void notifyAddedOnExistingObjects(ObjectProviderListener* listener) override;
  void notifyRemovedOnExistingObjects(ObjectProviderListener* listener) override;
  void notifyObjectsAdded(const ObjectAdditionList& additions) override;
  void notifyObjectsRemoved(const ObjectRemovalList& removals) override;

protected:
  void notifyMuxedAddedOnExistingObjects(MuxedProviderListener* listener);
  void notifyMuxedRemovedOnExistingObjects(MuxedProviderListener* listener);
  void notifyExistingObjectsReadded(const ObjectAdditionList& additions);
  void notifyObjectsRefCountReduced(const ObjectRemovalList& removals);

  /// Does nothing by default.
  virtual void muxedListenerAdded(MuxedProviderListener* listener, bool notifyAboutExistingObjects);

  /// Does nothing by default.
  virtual void muxedListenerRemoved(MuxedProviderListener* listener, bool notifyAboutExistingObjects);

  void callOnExistingObjectsReadded(MuxedProviderListener* listener, const ObjectAdditionList& additions) const;
  void callOnObjectsRefCountReduced(MuxedProviderListener* listener, const ObjectRemovalList& removals) const;

protected:  // implements ObjectProviderListener
  void onObjectsAdded(const ObjectAdditionList& additions) override;
  void onObjectsRemoved(const ObjectRemovalList& removals) override;

private:
  using Lock = std::scoped_lock<std::recursive_mutex>;

private:
  friend class MuxedProviderListener;
  void muxedListenerDeleted(MuxedProviderListener* listener);

private:
  std::unordered_map<ObjectId, ObjectAdditionList> presentObjects_;
  mutable std::recursive_mutex listenersMutex_;
  mutable std::recursive_mutex objectsMutex_;

  std::vector<MuxedProviderListener*> muxedListeners_;
};

/// @}

}  // namespace sen

#endif  // SEN_CORE_OBJ_OBJECT_MUX_H
