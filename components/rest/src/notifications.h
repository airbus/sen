// === notifications.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_SRC_NOTIFICATIONS_H
#define SEN_COMPONENTS_REST_SRC_NOTIFICATIONS_H

// sen
#include "sen/core/base/compiler_macros.h"

// generated code
#include "stl/types.stl.h"

// std
#include <cstddef>
#include <memory>
#include <optional>
#include <queue>
#include <unordered_set>

namespace sen::components::rest
{

// Forward declarations
class Notifier;

// ObserverGuard is a RAII class that allows observers to
// retrieve the next available notification.
// It is designerd to work in conjunction with 'Notifier' class.
class ObserverGuard
{
  SEN_MOVE_ONLY(ObserverGuard)

public:
  ~ObserverGuard();

public:
  std::optional<Notification> next();

private:
  friend class Notifier;

  ObserverGuard(std::shared_ptr<class NotificationsManager> mgr, Notifier* notifier);

  std::queue<Notification> notifications_;
  std::shared_ptr<class NotificationsManager> mgr_;
  Notifier* notifier_;
};

// Notifier is a base class that provides of an interface to send
// notifications and allows clients to acquire an 'ObserverGuard',
// enabling them to receive such notifications.
class Notifier
{
public:
  void notify(const Notification& notification);

  /// Returns a new observer guard for this notifier.
  [[nodiscard]] ObserverGuard getObserverGuard();

  /// Returns all the active observer guards.
  [[nodiscard]] std::size_t getActiveObserversCount();

private:
  friend class ObserverGuard;

  void unregister(std::shared_ptr<class NotificationsManager> manager);

  std::unordered_set<std::shared_ptr<class NotificationsManager>> managers_;
};

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_NOTIFICATIONS_H
