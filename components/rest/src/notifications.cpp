
// === notifications.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "notifications.h"

// generated code
#include "stl/types.stl.h"

// std
#include <cstddef>
#include <memory>
#include <mutex>
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
  void notify(const Notification& notification)
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    notifications_.push(notification);
  }

  std::optional<Notification> next()
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (notifications_.empty())
    {
      return std::nullopt;
    }

    Notification notification = notifications_.front();
    notifications_.pop();
    return notification;
  }

private:
  std::mutex mutex_;
  std::queue<Notification> notifications_;
};

//--------------------------------------------------------------------------------------------------------------
// ObserverGuard
//--------------------------------------------------------------------------------------------------------------

ObserverGuard::ObserverGuard(std::shared_ptr<NotificationsManager> mgr, Notifier* notifier)
  : mgr_(std::move(mgr)), notifier_(notifier)
{
}

ObserverGuard::~ObserverGuard() { notifier_->unregister(mgr_); }

std::optional<Notification> ObserverGuard::next() { return mgr_->next(); }

//--------------------------------------------------------------------------------------------------------------
// Notifier
//--------------------------------------------------------------------------------------------------------------

void Notifier::notify(const Notification& notification)
{
  const std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& manager: managers_)
  {
    manager->notify(notification);
  }
}

ObserverGuard Notifier::getObserverGuard()
{
  const std::lock_guard<std::mutex> lock(mutex_);

  auto mgr = std::make_shared<NotificationsManager>();
  managers_.insert(mgr);

  return {mgr, this};
}

[[nodiscard]] std::size_t Notifier::getActiveObserversCount()
{
  const std::lock_guard<std::mutex> lock(mutex_);
  return managers_.size();
}

void Notifier::unregister(std::shared_ptr<NotificationsManager> manager)
{
  const std::lock_guard<std::mutex> lock(mutex_);

  managers_.erase(manager);
}

}  // namespace sen::components::rest
