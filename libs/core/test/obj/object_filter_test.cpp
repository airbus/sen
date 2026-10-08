// === object_filter_test.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/result.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/obj/detail/remote_object.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_filter.h"
#include "sen/core/obj/object_provider.h"

// generated code
#include "stl/example_class.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{

struct TestOwner final: example_class::ExampleClassBase
{
  explicit TestOwner(std::string name): ExampleClassBase(std::move(name)) { sen::impl::getWorkQueue(this)->enable(); }
  using ExampleClassBase::commit;
};

class TestObjectFilter: public sen::ObjectFilter
{
public:
  using ObjectFilter::evaluate;
  using ObjectFilter::getOwnerId;
  using ObjectFilter::ObjectFilter;
  using ObjectFilter::objectsAdded;
  using ObjectFilter::objectsRemoved;
  using ObjectFilter::subscriberAdded;
  using ObjectFilter::subscriberRemoved;
};

class MockListener: public sen::ObjectProviderListener
{
public:
  void onObjectsAdded(const sen::ObjectAdditionList& additions) override
  {
    for (const auto& add: additions)
    {
      if (const auto* instance = sen::getObjectInstance(add))
      {
        added.push_back(instance->getId());
      }
    }
  }

  void onObjectsRemoved(const sen::ObjectRemovalList& removals) override
  {
    for (const auto& rem: removals)
    {
      removed.push_back(rem.objectid);
    }
  }

  std::vector<sen::ObjectId> added;    // NOLINT(misc-non-private-member-variables-in-classes)
  std::vector<sen::ObjectId> removed;  // NOLINT(misc-non-private-member-variables-in-classes)
};

class DroppingListener: public MockListener
{
public:
  std::shared_ptr<sen::ObjectProvider> providerToDrop;  // NOLINT(misc-non-private-member-variables-in-classes)

  void onObjectsRemoved(const sen::ObjectRemovalList& removals) override
  {
    MockListener::onObjectsRemoved(removals);
    providerToDrop.reset();
  }
};

sen::ObjectOwnerId getTestOwnerId() { return sen::ObjectOwnerId {1U}; }

std::shared_ptr<sen::Interest> createEmptyInterest()
{
  return sen::Interest::make("SELECT * FROM session.bus", sen::CustomTypeRegistry());
}

std::shared_ptr<sen::Interest> createInterestWithRegistry(const std::string& query)
{
  sen::CustomTypeRegistry types;
  types.add(example_class::ExampleClassInterface::meta());
  return sen::Interest::make(query, types);
}

sen::impl::RemoteObjectInfo createTestProxyInfo()
{
  return sen::impl::RemoteObjectInfo {example_class::ExampleClassInterface::meta(),
                                      "TestProxy",
                                      sen::ObjectId {88U},
                                      nullptr,
                                      [](auto...) -> sen::Result<sen::impl::CallId, std::string>
                                      { return sen::Ok(sen::impl::CallId {1U}); },
                                      "localTestProxy",
                                      nullptr,
                                      sen::ObjectOwnerId {1U},
                                      {},
                                      std::nullopt};
}

}  // namespace

/// @test
/// Returns the same provider instance for repeated getOrCreateNamedProvider calls and a fresh
/// instance after removeNamedProvider.
/// @requirements(SEN-363)
TEST(ObjectFilter, NamedProvider_CreateAndRemove)
{
  TestObjectFilter filter(getTestOwnerId());
  const auto interest = createEmptyInterest();

  const auto provider1 = filter.getOrCreateNamedProvider("MyProvider", interest);
  EXPECT_NE(provider1, nullptr);

  const auto provider2 = filter.getOrCreateNamedProvider("MyProvider", interest);
  EXPECT_EQ(provider1, provider2);

  filter.removeNamedProvider("MyProvider");

  const auto provider3 = filter.getOrCreateNamedProvider("MyProvider", interest);
  EXPECT_NE(provider1, provider3);
}

/// @test
/// Throws when a second provider name is requested for an interest that already has a named
/// provider.
/// @requirements(SEN-363)
TEST(ObjectFilter, NamedProvider_ConflictThrows)
{
  TestObjectFilter filter(getTestOwnerId());
  const auto interest = createEmptyInterest();

  const auto provider1 = filter.getOrCreateNamedProvider("ProviderA", interest);
  EXPECT_NE(provider1, nullptr);

  EXPECT_ANY_THROW({ filter.getOrCreateNamedProvider("ProviderB", interest); });
}

/// @test
/// Returns a provider when getOrCreateNamedProvider is called with an interest that already
/// has an unnamed provider from addSubscriber.
/// @requirements(SEN-363)
TEST(ObjectFilter, NamedProvider_AssignsNameToExistingUnnamedProvider)
{
  TestObjectFilter filter(getTestOwnerId());
  const auto interest = createEmptyInterest();
  MockListener listener;

  filter.addSubscriber(interest, &listener, false);

  const auto provider = filter.getOrCreateNamedProvider("AssignedName", interest);
  EXPECT_NE(provider, nullptr);
}

/// @test
/// Notifies a subscribed listener about a matching new object and stops notifying after
/// removeSubscriber.
/// @requirements(SEN-363)
TEST(ObjectFilter, Subscriber_AddAndRemove)
{
  TestObjectFilter filter(getTestOwnerId());
  auto interest = createEmptyInterest();
  MockListener listener;

  filter.addSubscriber(interest, &listener, false);

  auto obj = std::make_shared<TestOwner>("TestObj1");
  TestObjectFilter::ObjectSet set;
  set.newObjects[obj->getId()] = obj;
  set.currentObjects[obj->getId()] = obj;

  filter.evaluate(set);

  EXPECT_EQ(listener.added.size(), 1U);
  EXPECT_EQ(listener.added[0], obj->getId());

  filter.removeSubscriber(interest, &listener, false);

  auto obj2 = std::make_shared<TestOwner>("TestObj2");
  TestObjectFilter::ObjectSet set2;
  set2.newObjects[obj2->getId()] = obj2;
  set2.currentObjects[obj->getId()] = obj;
  set2.currentObjects[obj2->getId()] = obj2;

  filter.evaluate(set2);

  EXPECT_EQ(listener.added.size(), 1U);
}

/// @test
/// Removes a subscriber by listener pointer alone, a later matching object produces no
/// notification.
/// @requirements(SEN-363)
TEST(ObjectFilter, Subscriber_RemoveByListener)
{
  TestObjectFilter filter(getTestOwnerId());
  auto interest = createEmptyInterest();
  MockListener listener;

  filter.addSubscriber(interest, &listener, false);
  filter.removeSubscriber(&listener, false);

  auto obj = std::make_shared<TestOwner>("TestObj1");
  TestObjectFilter::ObjectSet set;
  set.newObjects[obj->getId()] = obj;
  set.currentObjects[obj->getId()] = obj;

  filter.evaluate(set);

  EXPECT_TRUE(listener.added.empty());
}

/// @test
/// Reports a new matching object as added and reports it as removed once it appears in the
/// deleted set.
/// @requirements(SEN-363)
TEST(ObjectFilter, Evaluate_AddsAndRemoves)
{
  TestObjectFilter filter(getTestOwnerId());
  auto interest = createEmptyInterest();
  MockListener listener;

  filter.addSubscriber(interest, &listener, false);

  auto obj = std::make_shared<TestOwner>("TestObj1");

  TestObjectFilter::ObjectSet addSet;
  addSet.newObjects[obj->getId()] = obj;
  addSet.currentObjects[obj->getId()] = obj;
  filter.evaluate(addSet);

  EXPECT_EQ(listener.added.size(), 1U);
  EXPECT_TRUE(listener.removed.empty());

  TestObjectFilter::ObjectSet remSet;
  remSet.currentObjects = {};
  remSet.deletedObjects.insert(obj->getId());
  filter.evaluate(remSet);

  EXPECT_EQ(listener.added.size(), 1U);
  EXPECT_EQ(listener.removed.size(), 1U);
  EXPECT_EQ(listener.removed[0], obj->getId());
}

/// @test
/// Ignores remote proxy objects during evaluate, only native objects produce added notifications.
/// @requirements(SEN-363)
TEST(ObjectFilter, Evaluate_IgnoresNonNativeObjects)
{
  TestObjectFilter filter(getTestOwnerId());
  auto interest = createEmptyInterest();
  MockListener listener;
  filter.addSubscriber(interest, &listener, false);

  auto proxyObj = std::make_shared<example_class::ExampleClassRemoteProxy>(createTestProxyInfo());

  TestObjectFilter::ObjectSet set;
  set.newObjects[proxyObj->getId()] = proxyObj;
  set.currentObjects[proxyObj->getId()] = proxyObj;

  filter.evaluate(set);
  EXPECT_TRUE(listener.added.empty());
}

/// @test
/// Reports an already tracked object only once even when it reappears in newObjects on a later
/// evaluate.
/// @requirements(SEN-363)
TEST(ObjectFilter, Evaluate_IgnoresDuplicateTrackedObjects)
{
  TestObjectFilter filter(getTestOwnerId());
  auto interest = createEmptyInterest();
  MockListener listener;
  filter.addSubscriber(interest, &listener, false);

  auto obj = std::make_shared<TestOwner>("TestObj1");
  TestObjectFilter::ObjectSet set;
  set.newObjects[obj->getId()] = obj;
  set.currentObjects[obj->getId()] = obj;

  filter.evaluate(set);
  EXPECT_EQ(listener.added.size(), 1U);

  filter.evaluate(set);
  EXPECT_EQ(listener.added.size(), 1U);
}

/// @test
/// Skips a deleted object id it never tracked, evaluate neither throws nor reports a removal.
/// @requirements(SEN-363)
TEST(ObjectFilter, Evaluate_IgnoresUntrackedDeletedObjects)
{
  TestObjectFilter filter(getTestOwnerId());
  auto interest = createEmptyInterest();
  MockListener listener;
  filter.addSubscriber(interest, &listener, false);

  TestObjectFilter::ObjectSet set;
  set.deletedObjects.insert(sen::ObjectId {9999U});

  EXPECT_NO_THROW({ filter.evaluate(set); });
  EXPECT_TRUE(listener.removed.empty());
}

/// @test
/// Matches an object against a class condition resolved through the type registry and reports it
/// as added.
/// @requirements(SEN-363)
TEST(ObjectFilter, Evaluate_TypeCondition_ClassType)
{
  TestObjectFilter filter(getTestOwnerId());
  auto interest = createInterestWithRegistry("SELECT example_class.ExampleClass FROM session.bus");
  MockListener listener;

  filter.addSubscriber(interest, &listener, false);

  auto obj = std::make_shared<TestOwner>("TestObj1");

  TestObjectFilter::ObjectSet set;
  set.newObjects[obj->getId()] = obj;
  set.currentObjects[obj->getId()] = obj;
  filter.evaluate(set);

  EXPECT_EQ(listener.added.size(), 1U);
}

/// @test
/// Falls back to a string comparison of the class name when the queried type is absent from the
/// registry and still reports the match.
/// @requirements(SEN-363)
TEST(ObjectFilter, Evaluate_TypeCondition_StringCondition)
{
  TestObjectFilter filter(getTestOwnerId());
  sen::CustomTypeRegistry emptyTypes;
  auto interest = sen::Interest::make("SELECT example_class.ExampleClass FROM session.bus", emptyTypes);
  MockListener listener;

  filter.addSubscriber(interest, &listener, false);

  auto obj = std::make_shared<TestOwner>("TestObj1");

  TestObjectFilter::ObjectSet set;
  set.newObjects[obj->getId()] = obj;
  set.currentObjects[obj->getId()] = obj;
  filter.evaluate(set);

  EXPECT_EQ(listener.added.size(), 1U);
}

/// @test
/// Re-evaluates a property-based query condition on each cycle, the object is added once prop2
/// rises above the threshold and removed when it falls back below.
/// @requirements(SEN-363)
TEST(ObjectFilter, Evaluate_QueryCondition_UpdatesOnPropertyChange)
{
  TestObjectFilter filter(getTestOwnerId());
  auto interest = createInterestWithRegistry("SELECT * FROM session.bus WHERE prop2 > 10");

  MockListener listener;
  filter.addSubscriber(interest, &listener, false);

  auto obj = std::make_shared<TestOwner>("TestObj1");
  obj->setNextProp2(5U);
  obj->commit(sen::TimeStamp {100});

  TestObjectFilter::ObjectSet set;
  set.newObjects[obj->getId()] = obj;
  set.currentObjects[obj->getId()] = obj;
  filter.evaluate(set);

  EXPECT_TRUE(listener.added.empty());

  obj->setNextProp2(15U);
  obj->commit(sen::TimeStamp {200});

  TestObjectFilter::ObjectSet evalSet;
  evalSet.currentObjects[obj->getId()] = obj;
  filter.evaluate(evalSet);

  EXPECT_EQ(listener.added.size(), 1U);
  EXPECT_EQ(listener.added[0], obj->getId());

  obj->setNextProp2(5U);
  obj->commit(sen::TimeStamp {300});

  filter.evaluate(evalSet);

  EXPECT_EQ(listener.removed.size(), 1U);
  EXPECT_EQ(listener.removed[0], obj->getId());
}

/// @test
/// Evaluates a query that filters on the id keyword without throwing, both when the object is new
/// and on re-evaluation.
/// @requirements(SEN-363)
TEST(ObjectFilter, Evaluate_QueryCondition_IdKeyword)
{
  TestObjectFilter filter(getTestOwnerId());
  auto interest = createInterestWithRegistry("SELECT * FROM session.bus WHERE id = 123");

  MockListener listener;
  filter.addSubscriber(interest, &listener, false);

  auto objMatch = std::make_shared<TestOwner>("TestObj1");

  TestObjectFilter::ObjectSet set;
  set.newObjects[objMatch->getId()] = objMatch;
  set.currentObjects[objMatch->getId()] = objMatch;

  EXPECT_NO_THROW({ filter.evaluate(set); });

  set.newObjects.clear();
  EXPECT_NO_THROW({ filter.evaluate(set); });
}

/// @test
/// Treats a query over a non-existent property as matching nothing, evaluate completes and the
/// listener sees no additions.
/// @requirements(SEN-363)
TEST(ObjectFilter, Evaluate_InvalidQueryHandledSafely)
{
  TestObjectFilter filter(getTestOwnerId());
  auto interest = createInterestWithRegistry("SELECT * FROM session.bus WHERE nonExistentProperty > 10");

  MockListener listener;
  filter.addSubscriber(interest, &listener, false);

  auto obj = std::make_shared<TestOwner>("TestObj1");
  TestObjectFilter::ObjectSet set;
  set.newObjects[obj->getId()] = obj;
  set.currentObjects[obj->getId()] = obj;

  filter.evaluate(set);

  EXPECT_TRUE(listener.added.empty());
}

/// @test
/// Returns the owner id given at construction and accepts calls to the empty base implementations
/// of the subscriber and object hooks.
/// @requirements(SEN-363)
TEST(ObjectFilter, BaseVirtualMethods)
{
  TestObjectFilter filter(getTestOwnerId());
  const auto interest = createEmptyInterest();
  MockListener listener;

  EXPECT_EQ(filter.getOwnerId().get(), getTestOwnerId().get());

  filter.subscriberAdded(interest, &listener, true);
  filter.subscriberRemoved(interest, &listener, true);
  filter.subscriberRemoved(&listener, true);
  filter.objectsAdded(interest, {});
  filter.objectsRemoved(interest, {});
}

/// @test
/// Notifies a listener attached to a named provider with notify-existing enabled about objects
/// the provider already matched.
/// @requirements(SEN-363)
TEST(ObjectFilter, NamedProvider_NotifyAddedOnExistingObjects)
{
  TestObjectFilter filter(getTestOwnerId());
  const auto interest = createEmptyInterest();

  const auto provider = filter.getOrCreateNamedProvider("LateJoinerProvider", interest);
  ASSERT_NE(provider, nullptr);

  auto obj = std::make_shared<TestOwner>("TestObj1");
  TestObjectFilter::ObjectSet set;
  set.newObjects[obj->getId()] = obj;
  set.currentObjects[obj->getId()] = obj;

  filter.evaluate(set);

  MockListener lateListener;
  provider->addListener(&lateListener, true);

  ASSERT_EQ(lateListener.added.size(), 1U);
  EXPECT_EQ(lateListener.added[0], obj->getId());

  provider->removeListener(&lateListener, false);
}

/// @test
/// Matches objects by the name keyword in a query, only the object with the queried name is
/// reported as added.
/// @requirements(SEN-363)
TEST(ObjectFilter, Evaluate_QueryCondition_NameKeyword)
{
  TestObjectFilter filter(getTestOwnerId());
  auto interest = createInterestWithRegistry(R"(SELECT * FROM session.bus WHERE name = "TestObj1")");

  MockListener listener;
  filter.addSubscriber(interest, &listener, false);

  auto objMatch = std::make_shared<TestOwner>("TestObj1");
  auto objNoMatch = std::make_shared<TestOwner>("TestObj2");

  TestObjectFilter::ObjectSet set;
  set.newObjects[objMatch->getId()] = objMatch;
  set.newObjects[objNoMatch->getId()] = objNoMatch;
  set.currentObjects[objMatch->getId()] = objMatch;
  set.currentObjects[objNoMatch->getId()] = objNoMatch;

  EXPECT_NO_THROW({ filter.evaluate(set); });

  ASSERT_EQ(listener.added.size(), 1U);
  EXPECT_EQ(listener.added[0], objMatch->getId());

  EXPECT_TRUE(listener.removed.empty());
}

/// @test
/// Keeps the cached match verdict for an object whose queried properties did not change in the
/// cycle, notifications fire only on actual match transitions.
/// @requirements(SEN-363)
TEST(ObjectFilter, Evaluate_ReturnsCachedResultWhenUnchanged)
{
  TestObjectFilter filter(getTestOwnerId());
  auto interest = createInterestWithRegistry("SELECT * FROM session.bus WHERE prop2 > 10");

  MockListener listener;
  filter.addSubscriber(interest, &listener, false);

  auto obj = std::make_shared<TestOwner>("TestObj1");

  obj->setNextProp2(20U);
  obj->commit(sen::TimeStamp {100});

  TestObjectFilter::ObjectSet set;
  set.newObjects[obj->getId()] = obj;
  set.currentObjects[obj->getId()] = obj;

  filter.evaluate(set);

  EXPECT_EQ(listener.added.size(), 1U);
  EXPECT_EQ(listener.added[0], obj->getId());
  EXPECT_TRUE(listener.removed.empty());

  listener.added.clear();
  listener.removed.clear();

  set.newObjects.clear();
  filter.evaluate(set);

  EXPECT_TRUE(listener.added.empty());
  EXPECT_TRUE(listener.removed.empty());

  obj->setNextProp2(5U);
  obj->commit(sen::TimeStamp {200});

  filter.evaluate(set);

  EXPECT_TRUE(listener.added.empty());
  EXPECT_EQ(listener.removed.size(), 1U);
  EXPECT_EQ(listener.removed[0], obj->getId());

  listener.added.clear();
  listener.removed.clear();

  filter.evaluate(set);

  EXPECT_TRUE(listener.added.empty());
  EXPECT_TRUE(listener.removed.empty());
}

/// @test
/// Removes a named provider that is not first in the provider list without throwing.
/// @requirements(SEN-363)
TEST(ObjectFilter, NamedProvider_RemoveIteratesCorrectly)
{
  TestObjectFilter filter(getTestOwnerId());

  const auto interestA = sen::Interest::make("SELECT * FROM s1.b1", sen::CustomTypeRegistry());
  const auto interestB = sen::Interest::make("SELECT * FROM s2.b2", sen::CustomTypeRegistry());

  auto firstProvider = filter.getOrCreateNamedProvider("FirstProvider", interestA);
  auto targetProvider = filter.getOrCreateNamedProvider("TargetProvider", interestB);

  EXPECT_NO_THROW({ filter.removeNamedProvider("TargetProvider"); });
}

/// @test
/// Survives a provider being destroyed from a removal notification raised while removeSubscriber
/// iterates its copy of the provider list.
/// @requirements(SEN-363)
TEST(ObjectFilter, Subscriber_RemoveHandlesExpiredProvider)
{
  TestObjectFilter filter(getTestOwnerId());

  auto interestTrigger = sen::Interest::make("SELECT * FROM s1.b1", sen::CustomTypeRegistry());
  auto interestToDrop = sen::Interest::make("SELECT * FROM s2.b2", sen::CustomTypeRegistry());

  DroppingListener listener;

  auto triggerProvider = filter.getOrCreateNamedProvider("Trigger", interestTrigger);

  listener.providerToDrop = filter.getOrCreateNamedProvider("ToDrop", interestToDrop);

  triggerProvider->addListener(&listener, false);

  auto obj = std::make_shared<TestOwner>("TestObj1");
  TestObjectFilter::ObjectSet set;
  set.newObjects[obj->getId()] = obj;
  set.currentObjects[obj->getId()] = obj;
  filter.evaluate(set);

  filter.removeSubscriber(&listener, true);

  EXPECT_EQ(listener.providerToDrop, nullptr);
}

/// @test
/// Survives a provider being destroyed from a removal notification raised while removeSubscriber
/// for a specific interest iterates its copy of the provider list.
/// @requirements(SEN-363)
TEST(ObjectFilter, Subscriber_RemoveWithInterestHandlesExpiredProvider)
{
  TestObjectFilter filter(getTestOwnerId());

  auto interestTrigger = sen::Interest::make("SELECT * FROM s1.b1", sen::CustomTypeRegistry());
  auto interestToDrop = sen::Interest::make("SELECT * FROM s2.b2", sen::CustomTypeRegistry());

  DroppingListener listener;

  auto triggerProvider = filter.getOrCreateNamedProvider("Trigger", interestTrigger);

  listener.providerToDrop = filter.getOrCreateNamedProvider("ToDrop", interestToDrop);

  triggerProvider->addListener(&listener, false);

  auto obj = std::make_shared<TestOwner>("TestObj1");
  TestObjectFilter::ObjectSet set;
  set.newObjects[obj->getId()] = obj;
  set.currentObjects[obj->getId()] = obj;
  filter.evaluate(set);

  filter.removeSubscriber(interestTrigger, &listener, true);

  EXPECT_EQ(listener.providerToDrop, nullptr);
}

/// @test
/// Seeds a newly-created provider's listener with the filter's current match set when
/// addSubscriber runs with notifyAboutExisting=true.
TEST(ObjectFilter, Subscriber_NotifyAboutExistingTrueSeedsNewProvider)
{
  TestObjectFilter filter(getTestOwnerId());
  const auto interest = createEmptyInterest();

  auto obj = std::make_shared<TestOwner>("TestObj1");
  TestObjectFilter::ObjectSet set;
  set.newObjects[obj->getId()] = obj;
  set.currentObjects[obj->getId()] = obj;
  filter.evaluate(set);

  MockListener listener;
  filter.addSubscriber(interest, &listener, true);

  // Unsubscribe before asserting: an ASSERT_ returns, and the filter would outlive a
  // listener it still holds a pointer to.
  const auto added = listener.added;
  filter.removeSubscriber(interest, &listener, false);

  ASSERT_EQ(added.size(), 1U);
  EXPECT_EQ(added[0], obj->getId());
}

/// @test
/// Does not broadcast existing matches when addSubscriber runs with notifyAboutExisting=false.
TEST(ObjectFilter, Subscriber_NotifyAboutExistingFalseStaysSilent)
{
  TestObjectFilter filter(getTestOwnerId());
  const auto interest = createEmptyInterest();

  auto obj = std::make_shared<TestOwner>("TestObj1");
  TestObjectFilter::ObjectSet set;
  set.newObjects[obj->getId()] = obj;
  set.currentObjects[obj->getId()] = obj;
  filter.evaluate(set);

  MockListener listener;
  filter.addSubscriber(interest, &listener, false);

  const auto added = listener.added;
  filter.removeSubscriber(interest, &listener, false);

  EXPECT_TRUE(added.empty());
}

/// @test
/// Reports no active listeners on a fresh filter, true after addSubscriber, and false again after
/// removeSubscriber.
/// @requirements(SEN-362)
TEST(ObjectFilter, HasActiveListeners)
{
  TestObjectFilter filter(getTestOwnerId());
  const auto interest = createEmptyInterest();
  MockListener listener;

  EXPECT_FALSE(filter.hasActiveListeners());

  filter.addSubscriber(interest, &listener, false);
  EXPECT_TRUE(filter.hasActiveListeners());

  filter.removeSubscriber(interest, &listener, false);
  EXPECT_FALSE(filter.hasActiveListeners());
}
