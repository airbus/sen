// === object_store.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_OBJECT_STORE_H
#define SEN_COMPONENTS_TERM_SRC_OBJECT_STORE_H

// sen
// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/move_only_function.h"
#include "sen/core/base/result.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/object_mux.h"
#include "sen/core/obj/object_source.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"

// std
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sen::components::term
{

/// Notification about object discovery changes, ready for display.
struct DiscoveryNotification
{
  std::string message;
  bool isInfo = true;  ///< true for dimmed info style, false for normal text
};

/// Wraps ObjectMux, ObjectList, and source management.
/// Tracks object discovery and produces batched, human-friendly notifications.
class ObjectStore final
{
  SEN_NOCOPY_NOMOVE(ObjectStore)

public:
  explicit ObjectStore(kernel::RunApi& api);
  ~ObjectStore() = default;

  /// Open a source (session or session.bus).
  [[nodiscard]] Result<void, std::string> openSource(std::string_view sourceName);

  /// Close a source (session, session.bus, or session.bus.query).
  [[nodiscard]] Result<void, std::string> closeSource(std::string_view sourceName);

  /// Create a named query.
  [[nodiscard]] Result<void, std::string> createQuery(std::string_view name, std::string_view selection);

  /// Remove a named query.
  [[nodiscard]] Result<void, std::string> removeQuery(std::string_view name);

  /// Check if a source is open.
  [[nodiscard]] bool isSourceOpen(std::string_view sourceName) const;

  /// Get all open source names.
  [[nodiscard]] std::vector<std::string> getOpenSources() const;

  /// The sessions that have been opened, whether or not a bus of theirs has been detected.
  /// `getAvailableSources` reports an open session only through its buses, so it cannot answer this.
  [[nodiscard]] std::vector<std::string> getOpenSessions() const;

  /// Get all named queries with their definitions.
  struct QueryInfo
  {
    std::string name;
    std::string selection;
  };
  [[nodiscard]] std::vector<QueryInfo> getQueries() const;

  /// Get available sources for auto-completion (sessions and buses).
  [[nodiscard]] std::vector<std::string> getAvailableSources() const;

  /// Get the list of all discovered objects.
  [[nodiscard]] const std::list<std::shared_ptr<Object>>& getObjects() const;

  /// The objects matching a named query, or an empty vector if there is no such query. Read from
  /// the query's own subscription, not by filtering the main list: Interest carries a compiled
  /// query for the bus to evaluate and exposes no client-side match, so a second subscription is
  /// currently the only way to get a filtered view.
  [[nodiscard]] std::vector<std::shared_ptr<Object>> getQueryObjects(std::string_view queryName) const;

  /// Get the total object count.
  [[nodiscard]] std::size_t getObjectCount() const;

  /// A generation counter that increments on every object addition or removal.
  /// Use this to detect when cached views (like the ls tree) need rebuilding.
  [[nodiscard]] uint64_t getGeneration() const noexcept;

  /// Per-object add/remove callbacks. Fired once per object during drainInputs(). A downstream
  /// consumer (the Completer) can maintain its indices incrementally instead of doing a full
  /// rebuild, which matters at scale (thousands of objects).
  using ObjectEventCallback = sen::std_util::move_only_function<void(const std::shared_ptr<Object>&)>;
  void setObjectAddedCallback(ObjectEventCallback callback);
  void setObjectRemovedCallback(ObjectEventCallback callback);

  /// Drain pending discovery notifications (call once per update cycle).
  /// Returns notifications accumulated since the last call.
  [[nodiscard]] std::vector<DiscoveryNotification> drainNotifications();

private:
  struct ProviderData
  {
    std::shared_ptr<ObjectProvider> provider;
    std::shared_ptr<Interest> interest;
  };

  struct SourceData
  {
    std::shared_ptr<ObjectSource> source;
    std::map<std::string, ProviderData, std::less<>> providers;
  };

  [[nodiscard]] SourceData& getOrOpenSource(std::string_view sessionName, std::string_view busName);

  /// Queue a discovery notice, or count it as dropped if the queue is at its bound.
  void notify(DiscoveryNotification notification);

private:
  kernel::RunApi& api_;
  // Declared before everything they can be called from, so they are destroyed last. The other
  // order made them the first members to go, and any removal delivered while the sources were
  // still unwinding would have called a destroyed move_only_function.
  ObjectEventCallback onObjectAdded_;
  ObjectEventCallback onObjectRemoved_;

  ObjectMux mux_;
  std::unique_ptr<ObjectList<Object>> objects_;
  std::unordered_map<std::string, std::shared_ptr<kernel::SessionInfoProvider>> openSessions_;
  std::unordered_map<std::string, SourceData> openSources_;

  // Named query subscriptions. Each holds an ObjectList with only the matched objects.
  std::map<std::string, std::shared_ptr<Subscription<Object>>> querySubscriptions_;

  // What each named query selects. The provider map cannot be the only record: a query that reuses the
  // internal ".all" provider creates no provider of its own, so it would not appear in `queries`, could not
  // be removed, and would hold a Subscription tracking every object on the bus for the life of the session.
  std::map<std::string, std::string> querySelections_;

  // Notification accumulator (populated by callbacks, drained by the UI)
  // Bounded, like the other producer queues. Discovery on a large bus pushes one per object and the
  // drain is once per frame, so an unbounded queue here is the same accumulator in a third place.
  std::vector<DiscoveryNotification> pendingNotifications_;
  std::size_t droppedNotifications_ = 0;
  bool suppressNotifications_ = true;  // suppress until first user interaction
  uint64_t generation_ = 0;
};

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_OBJECT_STORE_H
