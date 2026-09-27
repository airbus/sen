// === object_store_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "object_store.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/test_kernel.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
#include <memory>
#include <string>

namespace sen::components::term
{
namespace
{

//--------------------------------------------------------------------------------------------------------------
// Fixture: spins up a TestKernel, builds an ObjectStore from the live RunApi.
//
// The store is constructed inside the component's run callback so it has a real RunApi reference.
// Tests then call store methods directly, the validation paths exercised here all fail (or succeed
// idempotently) without dispatching work to the kernel, so they're safe to invoke between steps.
//--------------------------------------------------------------------------------------------------------------

class ObjectStoreTest: public ::testing::Test
{
protected:
  void SetUp() override
  {
    testComponent.onRun(
      [this](kernel::RunApi& api) -> kernel::FuncResult
      {
        if (!store)
        {
          store = std::make_unique<ObjectStore>(api);
        }
        return api.execLoop(Duration(std::chrono::milliseconds(10)), []() {});
      });

    testKernel = std::make_unique<kernel::TestKernel>(&testComponent);
    testKernel->step(3);
    ASSERT_NE(store, nullptr);
  }

  void TearDown() override
  {
    // Drop the store before the kernel goes away, its destructor unhooks listeners
    // from the mux, which is fine, but we want a deterministic order.
    store.reset();
    testKernel.reset();
  }

  // A fixture's members are its tests' locals, so the encapsulation the check asks for has no
  // owner to protect them from. Same reading as libs/core's test fixtures.
  kernel::TestComponent testComponent;             // NOLINT(misc-non-private-member-variables-in-classes)
  std::unique_ptr<kernel::TestKernel> testKernel;  // NOLINT(misc-non-private-member-variables-in-classes)
  std::unique_ptr<ObjectStore> store;              // NOLINT(misc-non-private-member-variables-in-classes)
};

//--------------------------------------------------------------------------------------------------------------
// openSource validation
//--------------------------------------------------------------------------------------------------------------

TEST_F(ObjectStoreTest, OpenSourceTooManyComponentsFails)
{
  auto result = store->openSource("a.b.c");
  EXPECT_TRUE(result.isError());
}

TEST_F(ObjectStoreTest, OpenSourceFourComponentsFails)
{
  auto result = store->openSource("a.b.c.d");
  EXPECT_TRUE(result.isError());
}

//--------------------------------------------------------------------------------------------------------------
// createQuery validation
//--------------------------------------------------------------------------------------------------------------

TEST_F(ObjectStoreTest, CreateQueryRejectsNameWithDot)
{
  auto result = store->createQuery("foo.bar", "SELECT * FROM session.bus");
  ASSERT_TRUE(result.isError());
  EXPECT_NE(result.getError().find("dots"), std::string::npos);
}

TEST_F(ObjectStoreTest, CreateQueryRejectsNameWithSpace)
{
  auto result = store->createQuery("foo bar", "SELECT * FROM session.bus");
  ASSERT_TRUE(result.isError());
  EXPECT_NE(result.getError().find("spaces"), std::string::npos);
}

TEST_F(ObjectStoreTest, CreateQueryRejectsInvalidSelection)
{
  auto result = store->createQuery("q", "this is not a valid SELECT statement");
  ASSERT_TRUE(result.isError());
  EXPECT_NE(result.getError().find("invalid query"), std::string::npos);
}

TEST_F(ObjectStoreTest, CreateQueryRejectsSelectionWithoutSource)
{
  // A SELECT with no FROM has no bus condition and should be rejected up-front.
  auto result = store->createQuery("q", "SELECT *");
  ASSERT_TRUE(result.isError());
  // Either "must specify a source" (parsed) or "invalid query" (rejected at parse time).
  EXPECT_TRUE(result.getError().find("source") != std::string::npos ||
              result.getError().find("invalid query") != std::string::npos);
}

//--------------------------------------------------------------------------------------------------------------
// removeQuery
//--------------------------------------------------------------------------------------------------------------

TEST_F(ObjectStoreTest, RemoveQueryUnknownFails)
{
  auto result = store->removeQuery("doesNotExist");
  ASSERT_TRUE(result.isError());
  EXPECT_NE(result.getError().find("not found"), std::string::npos);
}

//--------------------------------------------------------------------------------------------------------------
// closeSource validation
//--------------------------------------------------------------------------------------------------------------

TEST_F(ObjectStoreTest, CloseSourceWithFourComponentsFails)
{
  auto result = store->closeSource("a.b.c.d");
  EXPECT_TRUE(result.isError());
}

TEST_F(ObjectStoreTest, CloseQueryOnUnopenedBusFails)
{
  // 3 components ⇒ closeSource interprets it as "session.bus.query".
  // Bus is not open, so this should error with a "not open" hint.
  auto result = store->closeSource("session.bus.query");
  ASSERT_TRUE(result.isError());
  EXPECT_NE(result.getError().find("not open"), std::string::npos);
}

//--------------------------------------------------------------------------------------------------------------
// Read-only state
//--------------------------------------------------------------------------------------------------------------

TEST_F(ObjectStoreTest, FreshStoreHasNoObjectsOrSources)
{
  EXPECT_EQ(store->getObjectCount(), 0U);
  EXPECT_TRUE(store->getOpenSources().empty());
  EXPECT_TRUE(store->getQueries().empty());
  EXPECT_FALSE(store->isSourceOpen("anything"));
}

TEST_F(ObjectStoreTest, FreshStoreGenerationIsZero) { EXPECT_EQ(store->getGeneration(), 0U); }

TEST_F(ObjectStoreTest, DrainNotificationsEmptyOnFreshStore) { EXPECT_TRUE(store->drainNotifications().empty()); }

}  // namespace
}  // namespace sen::components::term
