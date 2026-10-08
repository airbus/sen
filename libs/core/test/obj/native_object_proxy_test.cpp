// === native_object_proxy_test.cpp ====================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/native_object.h"

// generated code
#include "stl/example_class.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <memory>
#include <string>
#include <utility>

namespace
{

using sen::EventCallback;
using sen::PropertyCallback;
using sen::TimeStamp;
using sen::VarList;

struct TestOwner final: example_class::ExampleClassBase
{
  explicit TestOwner(std::string name): ExampleClassBase(std::move(name)) {}
  using ExampleClassBase::commit;
  using ExampleClassBase::senImplEventEmitted;
};

struct TestProxy final: example_class::ExampleClassLocalProxy
{
  using ExampleClassLocalProxy::ExampleClassLocalProxy;
  using ExampleClassLocalProxy::senImplRemoveTypedConnection;
};

struct ProxyTestFixture
{
  std::shared_ptr<TestOwner> owner;  // NOLINT(misc-non-private-member-variables-in-classes)
  std::shared_ptr<TestProxy> proxy;  // NOLINT(misc-non-private-member-variables-in-classes)

  ProxyTestFixture()
  {
    owner = std::make_shared<TestOwner>("TestOwner");
    sen::impl::getWorkQueue(owner.get())->enable();
    proxy = std::make_shared<TestProxy>(owner.get(), "localProxy");
  }

  [[nodiscard]] sen::impl::WorkQueue* getQueue() const { return sen::impl::getWorkQueue(owner.get()); }
};

}  // namespace

/// @test
/// Reports itself as local, mirrors the owner's id and name, and casts to a proxy object but not to
/// a remote object.
/// @requirements(SEN-351, SEN-583)
TEST(NativeObjectProxy, IdentityAndPolymorphicCasts)
{
  const ProxyTestFixture fixture;

  EXPECT_FALSE(fixture.proxy->isRemote());
  EXPECT_EQ(fixture.proxy->getId(), fixture.owner->getId());
  EXPECT_EQ(fixture.proxy->getName(), fixture.owner->getName());
  EXPECT_EQ(fixture.proxy->getLocalName(), "localProxy.TestOwner");
  EXPECT_EQ(fixture.proxy->getClass()->getName(), "ExampleClass");

  EXPECT_EQ(fixture.proxy->asProxyObject(), fixture.proxy.get());
  EXPECT_EQ(std::as_const(*fixture.proxy).asProxyObject(), fixture.proxy.get());

  EXPECT_EQ(fixture.proxy->asRemoteObject(), nullptr);
  EXPECT_EQ(std::as_const(*fixture.proxy).asRemoteObject(), nullptr);
}

/// @test
/// Queues a property set made on the proxy onto the owner's work queue, the owner sees the new
/// value only after the queue runs and a commit.
/// @requirements(SEN-351, SEN-573)
TEST(NativeObjectProxy, SendWorkToOwnerQueue)
{
  const ProxyTestFixture fixture;

  fixture.proxy->setNextProp1("updated_via_proxy");

  EXPECT_NE(fixture.owner->getProp1(), "updated_via_proxy");

  while (fixture.getQueue()->executeAll())
  {
  }

  fixture.owner->commit(TimeStamp {100});
  EXPECT_EQ(fixture.owner->getProp1(), "updated_via_proxy");
}

/// @test
/// Pulls committed owner state into the proxy on drainInputs, firing the proxy's property-changed
/// callback and adopting the owner's commit time.
/// @requirements(SEN-351, SEN-573, SEN-574)
TEST(NativeObjectProxy, DrainInputsSynchronizesStateAndFiresEvents)
{
  const ProxyTestFixture fixture;
  bool propertyTriggered = false;
  PropertyCallback callback(fixture.getQueue(), [&](const auto&) { propertyTriggered = true; });
  auto guard = fixture.proxy->onProp1Changed(std::move(callback));

  fixture.owner->setNextProp1("new_synced_value");
  fixture.owner->commit(TimeStamp {200});

  EXPECT_NE(fixture.proxy->getProp1(), "new_synced_value");

  fixture.proxy->drainInputs();

  EXPECT_EQ(fixture.proxy->getProp1(), "new_synced_value");

  while (fixture.getQueue()->executeAll())
  {
  }

  EXPECT_TRUE(propertyTriggered);
  EXPECT_EQ(fixture.proxy->getLastCommitTime().sinceEpoch().getNanoseconds(), 200);
}

/// @test
/// Fires no property-changed callback from drainInputs when a new commit leaves the property
/// values unchanged.
/// @requirements(SEN-573)
TEST(NativeObjectProxy, DrainInputsDoesNotFireWhenUnchanged)
{
  const ProxyTestFixture fixture;

  fixture.owner->setNextProp1("stable_value");
  fixture.owner->commit(TimeStamp {100});
  fixture.proxy->drainInputs();

  while (fixture.getQueue()->executeAll())
  {
  }

  bool propertyTriggered = false;
  PropertyCallback callback(fixture.getQueue(), [&](const auto&) { propertyTriggered = true; });
  auto guard = fixture.proxy->onProp1Changed(std::move(callback));

  fixture.owner->commit(TimeStamp {200});
  fixture.proxy->drainInputs();

  while (fixture.getQueue()->executeAll())
  {
  }

  EXPECT_FALSE(propertyTriggered);
}

/// @test
/// Delivers an owner property change to an untyped per-property subscription on the proxy once
/// drainInputs runs.
/// @requirements(SEN-573, SEN-574)
TEST(NativeObjectProxy, UntypedPropertySubscriptionFiresOnDrain)
{
  const ProxyTestFixture fixture;
  const auto* metaClass = example_class::ExampleClassInterface::meta().type();
  const auto* propertyInfo = metaClass->searchPropertyByName("prop1");

  bool untypedTriggered = false;
  EventCallback<VarList> callback(fixture.getQueue(), [&](const auto&, const auto&) { untypedTriggered = true; });
  auto guard = fixture.proxy->onPropertyChangedUntyped(propertyInfo, std::move(callback));

  fixture.owner->setNextProp1("untyped_trigger_val");
  fixture.owner->commit(TimeStamp {300});

  fixture.proxy->drainInputs();

  while (fixture.getQueue()->executeAll())
  {
  }

  EXPECT_TRUE(untypedTriggered);
}

/// @test
/// Delegates invokeAllPropertyCallbacks to the owner, owner-side subscribers fire while proxy-side
/// subscribers stay silent.
/// @requirements(SEN-573)
TEST(NativeObjectProxy, InvokeAllPropertyCallbacks)
{
  const ProxyTestFixture fixture;
  bool proxyTriggered = false;
  PropertyCallback proxyCallback(fixture.getQueue(), [&](const auto&) { proxyTriggered = true; });
  auto proxyGuard = fixture.proxy->onProp1Changed(std::move(proxyCallback));

  fixture.proxy->invokeAllPropertyCallbacks();

  while (fixture.getQueue()->executeAll())
  {
  }

  EXPECT_FALSE(proxyTriggered);

  bool ownerTriggered = false;
  PropertyCallback ownerCallback(fixture.getQueue(), [&](const auto&) { ownerTriggered = true; });
  auto ownerGuard = fixture.owner->onProp1Changed(std::move(ownerCallback));

  fixture.proxy->invokeAllPropertyCallbacks();

  while (fixture.getQueue()->executeAll())
  {
  }

  EXPECT_TRUE(ownerTriggered);
}

/// @test
/// Routes an untyped method invocation to the owner and delivers an owner-emitted event to an
/// untyped event subscription made on the proxy.
/// @requirements(SEN-351, SEN-573, SEN-574)
TEST(NativeObjectProxy, InvokeAndEventDelegationRoutesToOwner)
{
  const ProxyTestFixture fixture;
  const auto* metaClass = example_class::ExampleClassInterface::meta().type();
  const auto* targetMethod = metaClass->searchMethodByName("setNextProp1");
  const auto* targetEvent = metaClass->searchEventByName("prop1Changed");

  bool methodCompleted = false;
  sen::MethodCallback<sen::Var> methodCallback(fixture.getQueue(),
                                               [&](const auto&, const auto&) { methodCompleted = true; });
  fixture.proxy->invokeUntyped(targetMethod, {sen::Var(std::string("delegated_val"))}, std::move(methodCallback));

  while (fixture.getQueue()->executeAll())
  {
  }

  fixture.owner->commit(TimeStamp {400});

  EXPECT_EQ(fixture.owner->getProp1(), "delegated_val");
  EXPECT_TRUE(methodCompleted);

  bool eventTriggered = false;
  EventCallback<VarList> eventCallback(fixture.getQueue(), [&](const auto&, const auto&) { eventTriggered = true; });
  auto eventGuard = fixture.proxy->onEventUntyped(targetEvent, std::move(eventCallback));

  fixture.owner->senImplEventEmitted(targetEvent->getId(), [] { return VarList {}; }, sen::EventInfo {TimeStamp {400}});

  while (fixture.getQueue()->executeAll())
  {
  }

  EXPECT_TRUE(eventTriggered);
}

/// @test
/// Accepts a default-constructed, invalid callback in onPropertyChangedUntyped as a no-op and
/// keeps answering untyped property reads.
/// @requirements(SEN-574, SEN-1048)
TEST(NativeObjectProxy, IgnoresInvalidatedCallbacks)
{
  const ProxyTestFixture fixture;
  const auto* metaClass = example_class::ExampleClassInterface::meta().type();
  const auto* propertyInfo = metaClass->searchPropertyByName("prop1");

  EventCallback<VarList> emptyCallback;
  auto guard = fixture.proxy->onPropertyChangedUntyped(propertyInfo, std::move(emptyCallback));

  EXPECT_EQ(fixture.proxy->getPropertyUntyped(propertyInfo).get<std::string>(), "");
}

/// @test
/// Stops delivering property changes to an untyped subscription once its connection guard is
/// destroyed.
/// @requirements(SEN-573, SEN-574)
TEST(NativeObjectProxy, RemovesUntypedConnections)
{
  const ProxyTestFixture fixture;
  const auto* metaClass = example_class::ExampleClassInterface::meta().type();
  const auto* propertyInfo = metaClass->searchPropertyByName("prop1");

  bool untypedTriggered = false;
  {
    EventCallback<VarList> callback(fixture.getQueue(), [&](const auto&, const auto&) { untypedTriggered = true; });
    auto guard = fixture.proxy->onPropertyChangedUntyped(propertyInfo, std::move(callback));
  }

  fixture.owner->setNextProp1("disconnect_val");
  fixture.owner->commit(TimeStamp {500});

  fixture.proxy->drainInputs();

  while (fixture.getQueue()->executeAll())
  {
  }

  EXPECT_FALSE(untypedTriggered);
}

/// @test
/// Removing a connection id the proxy does not know is harmless, an owner subscription made
/// afterwards still fires.
/// @requirements(SEN-351, SEN-573)
TEST(NativeObjectProxy, RemoveTypedConnectionFallback)
{
  const ProxyTestFixture fixture;

  fixture.proxy->senImplRemoveTypedConnection(sen::ConnId {9999U});

  bool ownerTriggered = false;
  PropertyCallback callback(fixture.getQueue(), [&](const auto&) { ownerTriggered = true; });
  auto guard = fixture.owner->onProp1Changed(std::move(callback));

  fixture.owner->setNextProp1("trigger_test");
  fixture.owner->commit(TimeStamp {600});

  fixture.owner->asObject().invokeAllPropertyCallbacks();

  while (fixture.getQueue()->executeAll())
  {
  }

  EXPECT_TRUE(ownerTriggered);
}

/// @test
/// Removing a property-change connection while a work item is still queued must not crash
/// and must not deliver the callback on drain.
TEST(NativeObjectProxy, RemoveDuringEmitNoCrashAndCallbackInvalidated)
{
  const ProxyTestFixture fixture;
  const auto* metaClass = example_class::ExampleClassInterface::meta().type();
  const auto* propertyInfo = metaClass->searchPropertyByName("prop1");

  bool callbackTriggered = false;
  {
    auto guard = fixture.proxy->onPropertyChangedUntyped(
      propertyInfo,
      EventCallback<VarList>(fixture.getQueue(), [&](const auto&, const auto&) { callbackTriggered = true; }));

    // Positive control. Without it the assertion below also passes for a connection that
    // was never wired up, or a property lookup that returned the wrong member.
    fixture.owner->setNextProp1("control_emit");
    fixture.owner->commit(TimeStamp {100});
    fixture.proxy->drainInputs();
    while (fixture.getQueue()->executeAll())
    {
    }
    ASSERT_TRUE(callbackTriggered);
    callbackTriggered = false;

    fixture.owner->setNextProp1("first_emit");
    fixture.owner->commit(TimeStamp {200});
    fixture.proxy->drainInputs();
  }  // guard destructs -> remove runs while the work item is still queued

  while (fixture.getQueue()->executeAll())
  {
  }

  EXPECT_FALSE(callbackTriggered);
}
