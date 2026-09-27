// === object_store_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "app.h"
#include "completer.h"
#include "log_router.h"
#include "object_store.h"
#include "scope.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/test_kernel.h"

// google test
#include <gmock/gmock.h>
#include <gtest/gtest.h>

// std
#include <algorithm>
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

//--------------------------------------------------------------------------------------------------------------
// The paths that succeed
//--------------------------------------------------------------------------------------------------------------
//
// Every case above this line asserts a refusal or an empty fresh store. Nothing opened a source, created
// a query or drained a notification, so getGeneration -- the counter the completer's whole list refresh
// is gated on -- was only ever read at zero, and both ends of that mechanism were unmeasured.

TEST_F(ObjectStoreTest, DiscoveryReportsTheSessionTheKernelIsRunning)
{
  auto available = store->getAvailableSources();
  EXPECT_THAT(available, ::testing::Contains("local")) << "the kernel's own session was not discovered";
}

TEST_F(ObjectStoreTest, OpeningASourceListsItAndSaysSo)
{
  ASSERT_TRUE(store->openSource("local.probe").isOk());

  EXPECT_TRUE(store->isSourceOpen("local.probe"));
  EXPECT_THAT(store->getOpenSources(), ::testing::Contains("local.probe"));

  auto notices = store->drainNotifications();
  EXPECT_FALSE(notices.empty()) << "opening a source said nothing";
  EXPECT_TRUE(store->drainNotifications().empty()) << "draining did not consume the notices";
}

TEST_F(ObjectStoreTest, TheGenerationCountsObjectTrafficAndNotCommands)
{
  // Two separate signals, and it is worth pinning which is which. generation_ moves when objects arrive
  // or leave; a command that changes the store's shape is picked up because CommandEngine::execute marks
  // the completer's lists dirty afterwards. Bumping the generation here instead would make every `open`
  // rebuild the object index as well, which is the expensive half.
  const auto before = store->getGeneration();
  ASSERT_TRUE(store->openSource("local.probe").isOk());
  EXPECT_EQ(store->getGeneration(), before) << "opening a source moved the object-traffic counter";

  ASSERT_TRUE(store->createQuery("probes", "SELECT * FROM local.probe").isOk());
  EXPECT_EQ(store->getGeneration(), before) << "creating a query moved the object-traffic counter";
}

TEST_F(ObjectStoreTest, ClosingASourceTakesItOffTheList)
{
  ASSERT_TRUE(store->openSource("local.probe").isOk());
  ASSERT_TRUE(store->isSourceOpen("local.probe"));

  ASSERT_TRUE(store->closeSource("local.probe").isOk());
  EXPECT_FALSE(store->isSourceOpen("local.probe"));
  EXPECT_THAT(store->getOpenSources(), ::testing::Not(::testing::Contains("local.probe")));
}

TEST_F(ObjectStoreTest, AValidQueryIsCreatedListedAndRemovable)
{
  ASSERT_TRUE(store->createQuery("probes", "SELECT * FROM local.probe").isOk());

  auto queries = store->getQueries();
  auto named = std::find_if(queries.begin(), queries.end(), [](const auto& q) { return q.name == "probes"; });
  ASSERT_NE(named, queries.end()) << "the query was created and does not appear in the list";
  EXPECT_EQ(named->selection, "SELECT * FROM local.probe");

  ASSERT_TRUE(store->removeQuery("probes").isOk());
  auto after = store->getQueries();
  EXPECT_TRUE(std::none_of(after.begin(), after.end(), [](const auto& q) { return q.name == "probes"; }));
}

TEST_F(ObjectStoreTest, TwoQueriesWithTheSameSelectionAreRefused)
{
  // This is the guard that used to skip the internal ".all" provider before comparing selections, which
  // is how identical queries piled up, each holding a subscription nothing could release.
  ASSERT_TRUE(store->createQuery("first", "SELECT * FROM local.probe").isOk());
  EXPECT_TRUE(store->createQuery("second", "SELECT * FROM local.probe").isError())
    << "a second query with the same definition was accepted";
}

//--------------------------------------------------------------------------------------------------------------
// Completer::update, which no test had ever called
//--------------------------------------------------------------------------------------------------------------
//
// It lives here because this is the only fixture with a live RunApi, which is what an ObjectStore needs.
// Every completer case elsewhere either injects private state through a test door or drives the
// incremental onObjectAdded path with the dirty flag cleared by hand, so the full-rebuild branch, the
// scope rebuild, and the sources/queries refresh had never run at all.

TEST_F(ObjectStoreTest, CompleterUpdateRebuildsFromTheStore)
{
  App app([](const std::string&) {});
  LogRouter router(app);
  Completer completer;
  Scope scope;

  ASSERT_TRUE(store->openSource("local.probe").isOk());
  ASSERT_TRUE(store->createQuery("probes", "SELECT * FROM local.probe").isOk());

  // The call itself is the thing under test: before this, nothing in the suite executed it.
  completer.update(scope, *store, router);

  auto sources = completer.complete("close ", 6);
  EXPECT_THAT(sources.candidates, ::testing::Not(::testing::IsEmpty()))
    << "the refresh did not pick up the open source";
  EXPECT_TRUE(std::any_of(
    sources.candidates.begin(), sources.candidates.end(), [](const Completion& c) { return c.text == "local.probe"; }))
    << "the open source was not offered to `close`";

  auto queries = completer.complete("query rm ", 9);
  EXPECT_TRUE(std::any_of(
    queries.candidates.begin(), queries.candidates.end(), [](const Completion& c) { return c.text == "probes"; }))
    << "the query was not offered to `query rm`";
}

TEST_F(ObjectStoreTest, CompleterUpdateNoticesASourceClosing)
{
  App app([](const std::string&) {});
  LogRouter router(app);
  Completer completer;
  Scope scope;

  ASSERT_TRUE(store->openSource("local.probe").isOk());
  completer.update(scope, *store, router);
  ASSERT_TRUE(std::any_of(completer.complete("close ", 6).candidates.begin(),
                          completer.complete("close ", 6).candidates.end(),
                          [](const Completion& c) { return c.text == "local.probe"; }));

  ASSERT_TRUE(store->closeSource("local.probe").isOk());
  completer.markListsDirty();
  completer.update(scope, *store, router);

  auto after = completer.complete("close ", 6);
  EXPECT_FALSE(std::any_of(
    after.candidates.begin(), after.candidates.end(), [](const Completion& c) { return c.text == "local.probe"; }))
    << "a closed source is still offered, so the refresh is one-way";
}

}  // namespace
}  // namespace sen::components::term
