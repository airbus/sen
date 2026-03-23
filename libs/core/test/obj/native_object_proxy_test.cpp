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
/// Validates the polymorphic casts and identity methods for local native proxies
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
/// Verifies that property setters on the proxy correctly defer work to the owner queue
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
/// Validates that calling drainInputs pulls the owner state into the proxy
/// and appropriately triggers proxy-level change events
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
/// Ensures drainInputs does not fire events if the owner properties have not actually changed
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
/// Ensures untyped property subscriptions mapped to the proxy natively catch owner state changes upon draining inputs
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
/// Verifies that invokeAllPropertyCallbacks on the proxy delegates strictly to the owner
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
/// Ensures that untyped invoke and standard event delegation route straight to the owner
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
/// Ensures adding bindings on invalidated or uninitialized callbacks exits early cleanly
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
/// Confirms that disconnecting an untyped subscription drops it correctly from the proxy
/// map and forwards the un-registration accurately
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
/// Verifies the logic path where a removed connection ID is not found locally
/// triggering the fallback to removeTypedConnectionOnOwner
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
