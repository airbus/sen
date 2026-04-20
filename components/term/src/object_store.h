// === object_store.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_OBJECT_STORE_H
#define SEN_COMPONENTS_TERM_SRC_OBJECT_STORE_H

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
  bool isInfo = true;  /// true for dimmed info style, false for normal text
};

/// Wraps ObjectMux, ObjectList, and source management.
/// Tracks object discovery and produces batched, human-friendly notifications.
class ObjectStore final
{
  SEN_NOCOPY_NOMOVE(ObjectStore)

public:
  explicit ObjectStore(kernel::RunApi& api);

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

  /// Get all named queries with their definitions.
  struct QueryInfo
  {
    std::string name;
    std::string selection;
    std::size_t matchCount = 0;
  };
  [[nodiscard]] std::vector<QueryInfo> getQueries() const;

  /// Get available sources for auto-completion (sessions and buses).
  [[nodiscard]] std::vector<std::string> getAvailableSources() const;

  /// Get the list of all discovered objects.
  [[nodiscard]] const std::list<std::shared_ptr<Object>>& getObjects() const;

  /// Get only the objects that match a named query. Returns an empty vector if the query
  /// doesn't exist. Filters the main object list by the query's Interest conditions.
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

private:
  kernel::RunApi& api_;
  ObjectMux mux_;
  std::unique_ptr<ObjectList<Object>> objects_;
  std::unordered_map<std::string, std::shared_ptr<kernel::SessionInfoProvider>> openSessions_;
  std::unordered_map<std::string, SourceData> openSources_;

  // Named query subscriptions. Each holds an ObjectList with only the matched objects.
  std::map<std::string, std::shared_ptr<Subscription<Object>>> querySubscriptions_;

  // Notification accumulator (populated by callbacks, drained by the UI)
  std::vector<DiscoveryNotification> pendingNotifications_;
  bool suppressNotifications_ = true;  // suppress until first user interaction
  uint64_t generation_ = 0;
  ObjectEventCallback onObjectAdded_;
  ObjectEventCallback onObjectRemoved_;
};

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_OBJECT_STORE_H
