// === notifications_test.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "notifications.h"

// generated code
#include "stl/types.stl.h"

// google test
#include <gtest/gtest.h>

using sen::components::rest::Notification;
using sen::components::rest::NotificationType;
using sen::components::rest::Notifier;
using sen::components::rest::ObserverGuard;

/// @test
/// Check observers consume notifications at the same order than got notified
/// @requirements(SEN-1061)
TEST(Rest, notification_order)
{
  Notifier notifier;
  ObserverGuard guard = notifier.getObserverGuard();

  notifier.notify(Notification {NotificationType::evt});
  notifier.notify(Notification {NotificationType::invoke});
  notifier.notify(Notification {NotificationType::objectAdded});
  notifier.notify(Notification {NotificationType::objectRemoved});

  {
    auto notification = guard.next();
    ASSERT_TRUE(notification.has_value());
    ASSERT_EQ(notification.value().type, NotificationType::evt);
  }

  {
    auto notification = guard.next();
    ASSERT_TRUE(notification.has_value());
    ASSERT_EQ(notification.value().type, NotificationType::invoke);
  }
  {
    auto notification = guard.next();
    ASSERT_TRUE(notification.has_value());
    ASSERT_EQ(notification.value().type, NotificationType::objectAdded);
  }
  {
    auto notification = guard.next();
    ASSERT_TRUE(notification.has_value());
    ASSERT_EQ(notification.value().type, NotificationType::objectRemoved);
  }

  {
    auto notification = guard.next();
    ASSERT_FALSE(notification.has_value());
  }
}

/// @test
/// Check observers RAII
/// @requirements(SEN-1061)
TEST(Rest, notification_raii)
{
  Notifier notifier;
  {
    ObserverGuard guard1 = notifier.getObserverGuard();
    ASSERT_EQ(notifier.getActiveObserversCount(), 1);
  }
  ASSERT_EQ(notifier.getActiveObserversCount(), 0);

  {
    ObserverGuard guard1 = notifier.getObserverGuard();
    ASSERT_EQ(notifier.getActiveObserversCount(), 1);

    ObserverGuard guard2 = notifier.getObserverGuard();
    ASSERT_EQ(notifier.getActiveObserversCount(), 2);
  }
  ASSERT_EQ(notifier.getActiveObserversCount(), 0);
}

/// @test
/// Check observers work concurrently
/// @requirements(SEN-1061)
TEST(Rest, notification_concurrency)
{
  Notifier notifier;
  ObserverGuard guard = notifier.getObserverGuard();
  ObserverGuard guard2 = notifier.getObserverGuard();

  notifier.notify(Notification {NotificationType::invoke});

  {
    auto notification = guard.next();
    ASSERT_TRUE(notification.has_value());
    ASSERT_EQ(notification.value().type, NotificationType::invoke);

    notification = guard.next();
    ASSERT_FALSE(notification.has_value());
  }
  {
    auto notification = guard2.next();
    ASSERT_TRUE(notification.has_value());
    ASSERT_EQ(notification.value().type, NotificationType::invoke);

    notification = guard2.next();
    ASSERT_FALSE(notification.has_value());
  }
}
