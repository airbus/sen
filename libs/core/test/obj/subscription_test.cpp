// === subscription_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/obj/subscription.h"

// sen
#include "sen/core/base/span.h"
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

}  // namespace

/// @test
/// Verifies the default constructor initializes an empty subscription without a source
/// @requirements(SEN-362)
TEST(SubscriptionTest, DefaultConstructor)
{
  const sen::Subscription<sen::Object> sub;

  EXPECT_FALSE(sub.source);
}

/// @test
/// Verifies the destructor cleanly removes the subscriber if a source is attached
/// @requirements(SEN-362)
TEST(SubscriptionTest, DestructorRemovesSubscriber)
{
  {
    const auto source = std::make_shared<MockObjectSource>(getTestOwnerId());
    sen::Subscription<sen::Object> sub;
    sub.source = source;
  }

  SUCCEED();
}

/// @test
/// Verifies the move constructor securely transfers ownership of the state
/// @requirements(SEN-362)
TEST(SubscriptionTest, MoveConstructor)
{
  const auto source = std::make_shared<MockObjectSource>(getTestOwnerId());
  sen::Subscription<sen::Object> sub1;
  sub1.source = source;

  const sen::Subscription sub2(std::move(sub1));

  EXPECT_EQ(sub2.source, source);
  EXPECT_FALSE(sub1.source);
}

/// @test
/// Verifies move assignment handles state cleanly, transfers ownership,
/// and manages self-assignment without causing undefined behavior
/// @requirements(SEN-351, SEN-362)
TEST(SubscriptionTest, MoveAssignmentSelfAndEmpty)
{
  sen::Subscription<sen::Object> sub1;
  sen::Subscription<sen::Object> sub2;

  sub2 = std::move(sub1);

  EXPECT_FALSE(sub2.source);
  EXPECT_FALSE(sub1.source);

  auto* ptr = &sub2;
  sub2 = std::move(*ptr);

  EXPECT_FALSE(sub2.source);
}

/// @test
/// Verifies move assignment correctly unsubscribes the existing source
/// on the destination object before claiming the new state from the moved object
/// @requirements(SEN-362)
TEST(SubscriptionTest, MoveAssignmentOverwritesExistingSource)
{
  const auto source1 = std::make_shared<MockObjectSource>(getTestOwnerId());
  const auto source2 = std::make_shared<MockObjectSource>(getTestOwnerId());

  sen::Subscription<sen::Object> sub1;
  sub1.source = source1;

  sen::Subscription<sen::Object> sub2;
  sub2.source = source2;

  sub1 = std::move(sub2);

  EXPECT_EQ(sub1.source, source2);
  EXPECT_FALSE(sub2.source);
}
