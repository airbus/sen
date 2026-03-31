
// === notifications.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "notifications.h"

// components
#include "utils.h"

// generated code
#include "stl/types.stl.h"

// std
#include <cstddef>
#include <memory>
#include <optional>
#include <queue>
#include <utility>

namespace sen::components::rest
{

//--------------------------------------------------------------------------------------------------------------
// NotificationManager
//--------------------------------------------------------------------------------------------------------------

class NotificationsManager
{
public:
  void notify(const Notification& notification) { notifications_.push(notification); }

  std::optional<Notification> next()
  {
    if (notifications_.empty())
    {
      return std::nullopt;
    }

    Notification notification = notifications_.front();
    notifications_.pop();
    return notification;
  }

private:
  std::queue<Notification> notifications_;
};

//--------------------------------------------------------------------------------------------------------------
// ObserverGuard
//--------------------------------------------------------------------------------------------------------------

ObserverGuard::ObserverGuard(std::shared_ptr<NotificationsManager> mgr, Notifier* notifier)
  : mgr_(std::move(mgr)), notifier_(notifier)
{
}

ObserverGuard::~ObserverGuard()
{
  getLogger()->trace("Destroying ObserverGuard");
  notifier_->unregister(mgr_);
}

std::optional<Notification> ObserverGuard::next() { return mgr_->next(); }

//--------------------------------------------------------------------------------------------------------------
// Notifier
//--------------------------------------------------------------------------------------------------------------

void Notifier::notify(const Notification& notification)
{
  for (const auto& manager: managers_)
  {
    manager->notify(notification);
  }
}

ObserverGuard Notifier::getObserverGuard()
{
  auto mgr = std::make_shared<NotificationsManager>();
  managers_.insert(mgr);

  return {mgr, this};
}

[[nodiscard]] std::size_t Notifier::getActiveObserversCount() { return managers_.size(); }

void Notifier::unregister(std::shared_ptr<NotificationsManager> manager) { managers_.erase(manager); }

}  // namespace sen::components::rest
