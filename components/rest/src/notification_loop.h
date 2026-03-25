// === notification_loop.h =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_SRC_NOTIFICATION_LOOP_H
#define SEN_COMPONENTS_REST_SRC_NOTIFICATION_LOOP_H

// component
#include "http_session.h"
#include "notifications.h"

// generated code
#include "sen/core/base/compiler_macros.h"
#include "stl/types.stl.h"

// asio
#include <asio/ip/tcp.hpp>

// std
#include <memory>

namespace sen::components::rest
{

/// NotificationLoop subscribes to all notifications related to a given client session
/// and send them through the TCP socket as a SSE event
class NotificationLoop: public std::enable_shared_from_this<NotificationLoop>
{
  SEN_NOCOPY_NOMOVE(NotificationLoop)

public:
  NotificationLoop(std::shared_ptr<HttpSession> httpSession,
                   std::shared_ptr<asio::ip::tcp::socket> socket,
                   class ClientSession& clientSession);
  ~NotificationLoop();

public:
  /// Start the notification loop until the socket gets broken or closed
  void start();

private:
  void post();
  void run();

  std::shared_ptr<HttpSession> httpSession_;
  std::shared_ptr<asio::ip::tcp::socket> sharedSocket_;

  NotificationType notificationType_ = NotificationType::evt;
  ObserverGuard membersObserver_;
  ObserverGuard invokesObserver_;
  ObserverGuard interestsObserver_;
};

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_NOTIFICATION_LOOP_H
