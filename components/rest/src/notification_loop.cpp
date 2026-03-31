// === notification_loop.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "notification_loop.h"

// component
#include "client_session.h"
#include "http_session.h"
#include "utils.h"

// generated code
#include "stl/types.stl.h"

// asio
#include <asio/buffer.hpp>
#include <asio/error_code.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/post.hpp>
#include <asio/write.hpp>  // NOLINT(misc-include-cleaner)

// std
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace sen::components::rest
{

NotificationLoop::NotificationLoop(std::shared_ptr<HttpSession> httpSession,
                                   std::shared_ptr<asio::ip::tcp::socket> socket,
                                   ClientSession& clientSession)
  : httpSession_(std::move(httpSession))
  , sharedSocket_(std::move(socket))
  , membersObserver_(clientSession.getObserverGuard(NotifierType::membersNotifier))
  , invokesObserver_(clientSession.getObserverGuard(NotifierType::invokesNotifier))
  , interestsObserver_(clientSession.getObserverGuard(NotifierType::interestsNotifier))
{
}

NotificationLoop::~NotificationLoop() { getLogger()->trace("Destroying NotificationLoop"); }

void NotificationLoop::start() { post(); }

void NotificationLoop::post()
{
  auto self = shared_from_this();
  asio::post(httpSession_->getIOContext(), [self]() { self->run(); });
}

void NotificationLoop::run()
{
  std::optional<Notification> notification = std::nullopt;
  switch (notificationType_)
  {
    case NotificationType::evt:
    case NotificationType::property:
      notification = membersObserver_.next();
      break;
    case NotificationType::objectAdded:
    case NotificationType::objectRemoved:
      notification = interestsObserver_.next();
      break;
    case NotificationType::invoke:
    default:
      notification = invokesObserver_.next();
  }

  if (notification.has_value())
  {
    asio::error_code ec;
    std::string sse = toSSE(notification.value());
    asio::write(*sharedSocket_, asio::buffer(sse), ec);  // NOLINT(misc-include-cleaner)
    if (ec)
    {
      return;
    }
  }

  int next = (static_cast<int>(notificationType_) + 1) % static_cast<int>(NotificationType::count);
  notificationType_ = static_cast<NotificationType>(next);

  post();
}

}  // namespace sen::components::rest
