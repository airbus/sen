// === subscription_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/obj/subscription.h"

// sen
#include "sen/core/base/span.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_provider.h"
#include "sen/core/obj/object_source.h"

// google test
#include <gtest/gtest.h>

// std
#include <memory>
#include <utility>

namespace
{

sen::ObjectOwnerId getTestOwnerId() { return sen::ObjectOwnerId {1U}; }

class MockObjectSource final: public sen::ObjectSource
{
public:
  explicit MockObjectSource(const sen::ObjectOwnerId id): ObjectSource(id) {}

  bool add(const sen::Span<NativeObjectPtr>& /*instances*/) override { return true; }
  void remove(const sen::Span<NativeObjectPtr>& /*instances*/) override {}

protected:
  void flushOutputs() override {}
  void drainInputs() override {}
};

/// Builds a trivial interest for testing subscription lifecycle.
std::shared_ptr<sen::Interest> makeTestInterest()
{
  static sen::CustomTypeRegistry types;
  return sen::Interest::make("SELECT * FROM test.bus", types);
}

}  // namespace

/// @test
/// Starts without a source when default-constructed.
/// @requirements(SEN-362)
TEST(SubscriptionTest, DefaultConstructor)
{
  const sen::Subscription<sen::Object> sub;

  EXPECT_FALSE(sub.getSource());
}

/// @test
/// Survives destruction while still attached to a source.
/// @requirements(SEN-362)
TEST(SubscriptionTest, DestructorRemovesSubscriber)
{
  {
    const auto source = std::make_shared<MockObjectSource>(getTestOwnerId());
    sen::Subscription<sen::Object> sub;
    sub.attachTo(source, makeTestInterest(), false);
    EXPECT_TRUE(sub.getSource());
  }

  SUCCEED();
}

/// @test
/// Detaches from its source on release, getSource reports empty afterwards.
/// @requirements(SEN-362)
TEST(SubscriptionTest, ReleaseDetachesSource)
{
  const auto source = std::make_shared<MockObjectSource>(getTestOwnerId());
  sen::Subscription<sen::Object> sub;
  sub.attachTo(source, makeTestInterest(), false);

  EXPECT_TRUE(sub.getSource());
  sub.release(false);
  EXPECT_FALSE(sub.getSource());
}

/// @test
/// Transfers the attached source to the new subscription on move construction.
/// @requirements(SEN-362)
TEST(SubscriptionTest, MoveConstructor)
{
  const auto source = std::make_shared<MockObjectSource>(getTestOwnerId());
  sen::Subscription<sen::Object> sub1;
  sub1.attachTo(source, makeTestInterest(), false);

  const sen::Subscription sub2(std::move(sub1));

  EXPECT_EQ(sub2.getSource(), source);
}

/// @test
/// Keeps the subscription sourceless when move-assigning from an empty subscription and when
/// move-assigning it to itself.
/// @requirements(SEN-351, SEN-362)
TEST(SubscriptionTest, MoveAssignmentSelfAndEmpty)
{
  sen::Subscription<sen::Object> sub1;
  sen::Subscription<sen::Object> sub2;

  sub2 = std::move(sub1);

  EXPECT_FALSE(sub2.getSource());

  auto* ptr = &sub2;
  sub2 = std::move(*ptr);

  EXPECT_FALSE(sub2.getSource());
}

/// @test
/// Replaces the destination's existing source on move assignment, the destination ends attached
/// to the moved-in source.
/// @requirements(SEN-362)
TEST(SubscriptionTest, MoveAssignmentOverwritesExistingSource)
{
  const auto source1 = std::make_shared<MockObjectSource>(getTestOwnerId());
  const auto source2 = std::make_shared<MockObjectSource>(getTestOwnerId());

  sen::Subscription<sen::Object> sub1;
  sub1.attachTo(source1, makeTestInterest(), false);

  sen::Subscription<sen::Object> sub2;
  sub2.attachTo(source2, makeTestInterest(), false);

  sub1 = std::move(sub2);

  EXPECT_EQ(sub1.getSource(), source2);
}
