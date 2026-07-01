// === remote_interest_handler.h =======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_BUS_REMOTE_INTEREST_HANDLER_H
#define SEN_LIBS_KERNEL_SRC_BUS_REMOTE_INTEREST_HANDLER_H

// kernel
#include "containers.h"

// sen
#include "sen/core/obj/interest.h"

// std
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sen::kernel::impl
{

// forward declarations
class RemoteParticipant;
class ObjectUpdate;

/// The interest handler manages the local and remote interests registration
class InterestsHandler
{
  SEN_MOVE_ONLY(InterestsHandler)

public:
  InterestsHandler() = default;
  ~InterestsHandler() = default;

public:
  void addInterest(InterestId interestId) { interestToCounterMap_[interestId]++; }
  void removeInterest(InterestId interestId);
  [[nodiscard]] bool isRegistered(InterestId interestId) const noexcept
  {
    return interestToCounterMap_.find(interestId) != interestToCounterMap_.end();
  }

  void onInterestRemoved(std::function<void(InterestId)>&& callback) { onRemovedCallback_ = std::move(callback); }

private:
  std::unordered_map<InterestId, u32> interestToCounterMap_;
  std::function<void(InterestId)> onRemovedCallback_;
};

struct RemoteInterestsHandler
{
  mutable std::shared_mutex handlerMutex_;                            // NOLINT
  BiMultiMap<RemoteParticipant*, ObjectUpdate*> remotesUpdatesBMMap;  // NOLINT
  BiMultiMap<RemoteParticipant*, InterestId> remotesInterestsBMMap;   // NOLINT
  BiMultiMap<InterestId, ObjectUpdate*> interestsUpdatesBMMap;        // NOLINT

  void addSubscriber(InterestId interestId, RemoteParticipant* remote, bool newInterest = false);
  std::vector<InterestId> removeSubscriber(InterestId interestId, RemoteParticipant* remote);
  std::vector<InterestId> removeSubscriber(RemoteParticipant* remote);
  void addObject(InterestId interestId, ObjectUpdate* update);
  void removeObject(ObjectUpdate* update);
  std::vector<ObjectUpdate*> removeInterest(InterestId interestId);
  void removeRejectedObject(RemoteParticipant* remote, ObjectUpdate* update);
  [[nodiscard]] RemoteParticipant* getFirstRemoteParticipant();

  template <typename Fn>
  void forEachRemoteUpdate(Fn&& fun);

  template <typename Fn>
  void forEachRemoteWithUpdate(ObjectUpdate* update, Fn&& fun);

  /// Removes object updates associated to a certain interest. There can be more than one interest mapped to a certain
  /// update. Returns true if the update was completely removed from the interest handler
  [[nodiscard]] bool removeObjectUpdateByInterest(ObjectUpdate* update, InterestId interestId);
  void clear();

private:
  [[nodiscard]] bool remoteHasInterestInUpdate(RemoteParticipant* remote, ObjectUpdate* update) const;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------//
// InterestsHandler
//---------------------------------------------------------------------------------------------//

inline void InterestsHandler::removeInterest(InterestId interestId)
{
  if (!isRegistered(interestId))
  {
    return;
  }

  if (interestToCounterMap_[interestId] == 1)
  {
    interestToCounterMap_.erase(interestId);
    onRemovedCallback_(interestId);
  }
  else
  {
    interestToCounterMap_[interestId]--;
  }
}

//---------------------------------------------------------------------------------------------//
// RemoteInterestsHandler
//---------------------------------------------------------------------------------------------//

inline void RemoteInterestsHandler::addSubscriber(InterestId interestId, RemoteParticipant* remote, bool newInterest)
{
  std::unique_lock lock(handlerMutex_);

  remotesInterestsBMMap.add(interestId, remote);

  if (newInterest)
  {
    return;
  }

  interestsUpdatesBMMap.forEach(interestId,
                                [this, remote](ObjectUpdate* update) { remotesUpdatesBMMap.add(remote, update); });
}

inline std::vector<InterestId> RemoteInterestsHandler::removeSubscriber(InterestId interestId,
                                                                        RemoteParticipant* remote)
{
  std::unique_lock lock(handlerMutex_);

  auto [orphanRemotes, orphanInterests] = remotesInterestsBMMap.remove(remote, interestId);
  if (!orphanRemotes.empty())
  {
    remotesUpdatesBMMap.remove(remote);
  }
  else
  {
    if (const auto* updates = interestsUpdatesBMMap.tryGet(interestId))
    {
      for (auto* update: *updates)
      {
        if (!remoteHasInterestInUpdate(remote, update))
        {
          remotesUpdatesBMMap.remove(remote, update);
        }
      }
    }
  }
  return orphanInterests;
}

inline std::vector<InterestId> RemoteInterestsHandler::removeSubscriber(RemoteParticipant* remote)
{
  std::unique_lock lock(handlerMutex_);

  auto& orphanInterests = remotesInterestsBMMap.remove(remote).second;
  remotesUpdatesBMMap.remove(remote);
  return orphanInterests;
}

inline void RemoteInterestsHandler::addObject(InterestId interestId, ObjectUpdate* update)
{
  std::unique_lock lock(handlerMutex_);

  interestsUpdatesBMMap.add(interestId, update);
  remotesInterestsBMMap.forEach(interestId,
                                [this, update](RemoteParticipant* remote) { remotesUpdatesBMMap.add(remote, update); });
}

inline void RemoteInterestsHandler::removeObject(ObjectUpdate* update)
{
  std::unique_lock lock(handlerMutex_);

  interestsUpdatesBMMap.remove(update);
  remotesUpdatesBMMap.remove(update);
}

inline std::vector<ObjectUpdate*> RemoteInterestsHandler::removeInterest(InterestId interestId)
{
  std::unique_lock lock(handlerMutex_);

  // TODO(SEN-1737): review orphan management in the BiMultiMap
  // Clear any orphans that may be left from previous removals (call to remove does not always clear them).
  interestsUpdatesBMMap.clearOrphans();
  return interestsUpdatesBMMap.remove(interestId).second;
}

inline void RemoteInterestsHandler::removeRejectedObject(RemoteParticipant* remote, ObjectUpdate* update)
{
  std::unique_lock lock(handlerMutex_);

  remotesUpdatesBMMap.remove(remote, update);
}

inline RemoteParticipant* RemoteInterestsHandler::getFirstRemoteParticipant()
{
  std::shared_lock lock(handlerMutex_);

  for (auto itr = remotesInterestsBMMap.begin<InterestId>();  // NOLINT(modernize-loop-convert) NOSONAR
       itr != remotesInterestsBMMap.end<InterestId>();
       ++itr)
  {
    if (!itr->second.empty())
    {
      return itr->second.front();
    }
  }

  return nullptr;
}

template <typename Fn>
inline void RemoteInterestsHandler::forEachRemoteUpdate(Fn&& fun)
{
  std::shared_lock lock(handlerMutex_);

  remotesUpdatesBMMap.forEach(std::forward<Fn>(fun));
}

template <typename Fn>
inline void RemoteInterestsHandler::forEachRemoteWithUpdate(ObjectUpdate* update, Fn&& fun)
{
  std::shared_lock lock(handlerMutex_);

  remotesUpdatesBMMap.forEach(update, std::forward<Fn>(fun));
}

inline bool RemoteInterestsHandler::remoteHasInterestInUpdate(RemoteParticipant* remote, ObjectUpdate* update) const
{
  const auto* remoteInterests = remotesInterestsBMMap.tryGet(remote);
  const auto* updateInterests = interestsUpdatesBMMap.tryGet(update);
  if (remoteInterests == nullptr || updateInterests == nullptr)
  {
    return false;
  }

  for (const auto interestId: *remoteInterests)
  {
    if (updateInterests->contains(interestId))
    {
      return true;
    }
  }

  return false;
}

inline bool RemoteInterestsHandler::removeObjectUpdateByInterest(ObjectUpdate* update, InterestId interestId)
{
  std::unique_lock lock(handlerMutex_);

  // remove the deleted interest/update link
  interestsUpdatesBMMap.remove(interestId, update);

  std::vector<RemoteParticipant*> remotesToRemove;
  remotesToRemove.reserve(10U);  // estimated reserve
  remotesUpdatesBMMap.forEach(update,
                              [this, update, &remotesToRemove](RemoteParticipant* remote)
                              {
                                if (!remoteHasInterestInUpdate(remote, update))
                                {
                                  // safe deletion of the update from the remote (if we still find the update in the
                                  // derived remote interest set, this means that this remote used another interest,
                                  // therefore we should not remove the update from this remote)
                                  remotesToRemove.push_back(remote);
                                }
                              });

  // remove pending remotes from the map
  for (auto* remote: remotesToRemove)
  {
    remotesUpdatesBMMap.remove(remote, update);
  }

  // return true if the update was completely removed from the handler because no other interests linked to it were
  // found
  return !interestsUpdatesBMMap.contains(update);
}

inline void RemoteInterestsHandler::clear()
{
  std::unique_lock lock(handlerMutex_);

  remotesUpdatesBMMap.clear();
  remotesInterestsBMMap.clear();
  interestsUpdatesBMMap.clear();
}

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_BUS_REMOTE_INTEREST_HANDLER_H
