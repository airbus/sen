// === generic_local_object_proxy_test.cpp =============================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

/// Tests GenericLocalObjectProxy, and the generated local proxy that other objects get.

// test helpers
#include "generic_local_object_helpers.h"

// sen
#include "sen/core/base/result.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/core/obj/detail/native_object_proxy.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object.h"
#include "sen/kernel/detail/generic_local_object.h"
#include "sen/kernel/detail/generic_local_object_proxy.h"

// generated code
#include "bus/stl/rich.stl.h"

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

using generic_test::encodeFor;
using generic_test::indexOf;
using sen::TimeStamp;
using sen::kernel::impl::GenericLocalObject;
using sen::kernel::impl::GenericLocalObjectProxy;

/// A GenericLocalObject of a class built at runtime, and a GenericLocalObjectProxy over it.
struct ProxyFixture
{
  std::shared_ptr<GenericLocalObject> owner;       // NOLINT(misc-non-private-member-variables-in-classes)
  std::shared_ptr<GenericLocalObjectProxy> proxy;  // NOLINT(misc-non-private-member-variables-in-classes)

  ProxyFixture()
    : owner(std::make_shared<GenericLocalObject>("track", generic_test::makeTrackClass("generic_test.Track")))
  {
    getQueue()->enable();
    proxy = std::make_shared<GenericLocalObjectProxy>(owner.get(), owner.get(), nullptr, "localProxy");
  }

  [[nodiscard]] sen::impl::WorkQueue* getQueue() const { return sen::impl::getWorkQueue(owner.get()); }

  [[nodiscard]] const sen::Property* property(const char* name) const
  {
    return owner->properties()[indexOf(*owner, name)].get();
  }

  void set(const char* name, const sen::Var& value) const
  {
    owner->set(indexOf(*owner, name), encodeFor(*owner, name, value));
  }

  /// A callback that sets told to true when it runs.
  [[nodiscard]] sen::EventCallback<sen::VarList> recorder(bool& told) const
  {
    return {getQueue(), [&told](const sen::EventInfo& /*info*/, const sen::VarList& /*args*/) { told = true; }};
  }

  void drainQueue() const
  {
    while (getQueue()->executeAll())
    {
    }
  }
};

/// An object of a generated class.
struct Generated final: rich::RichBase
{
  explicit Generated(std::string name): RichBase(std::move(name)) {}
  using RichBase::commit;
};

/// A generated object with PropertyChangeTimes, like a replayed object with its recorded times.
struct Timed final: rich::RichBase, sen::kernel::impl::PropertyChangeTimes
{
  explicit Timed(std::string name): RichBase(std::move(name)) {}
  using RichBase::commit;

  [[nodiscard]] TimeStamp getPropertyChangeTime(sen::MemberHash /*propertyId*/) const override { return changed; }

  TimeStamp changed;  // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Drains a proxy, as its subscriber's component does each cycle, and runs the callbacks.
void drainAndRun(sen::impl::NativeObjectProxy& proxy, sen::impl::WorkQueue* queue)
{
  proxy.drainInputs();
  while (queue->executeAll())
  {
  }
}

/// Subscribes untyped to a property of object, recording the time of each change notified.
[[nodiscard]] sen::ConnectionGuard recordTimes(sen::Object& object,
                                               const sen::Property* property,
                                               sen::impl::WorkQueue* queue,
                                               std::vector<TimeStamp>& told)
{
  return object.onPropertyChangedUntyped(
    property,
    sen::EventCallback<sen::VarList>(
      queue, [&told](const sen::EventInfo& info, const sen::VarList& /*args*/) { told.push_back(info.creationTime); }));
}

}  // namespace

/// @test
/// Mirrors the owner's id, name and class, and is a local proxy, not a remote one.
TEST(GenericLocalObjectProxy, identityAndCasts)
{
  const ProxyFixture fixture;

  EXPECT_FALSE(fixture.proxy->isRemote());
  EXPECT_EQ(fixture.proxy->getId(), fixture.owner->getId());
  EXPECT_EQ(fixture.proxy->getName(), "track");
  EXPECT_EQ(fixture.proxy->getLocalName(), "localProxy.track");
  EXPECT_EQ(fixture.proxy->getClass().type(), fixture.owner->getClass().type());
  EXPECT_EQ(fixture.proxy->asProxyObject(), fixture.proxy.get());
  EXPECT_EQ(fixture.proxy->asRemoteObject(), nullptr);
}

/// @test
/// Takes the committed values at a drain, not before, and notifies the change with the commit time.
TEST(GenericLocalObjectProxy, aDrainTakesTheCommittedValuesAndTellsOfTheChange)
{
  const ProxyFixture fixture;
  const auto* altitude = fixture.property("altitude");

  std::vector<TimeStamp> told;
  auto guard = fixture.proxy->onPropertyChangedUntyped(
    altitude,
    sen::EventCallback<sen::VarList>(fixture.getQueue(),
                                     [&told](const sen::EventInfo& info, const sen::VarList& /*args*/)
                                     { told.push_back(info.creationTime); }));

  fixture.set("altitude", 1200.0);
  fixture.owner->commit(TimeStamp {200});
  EXPECT_TRUE(fixture.proxy->getPropertyUntyped(altitude).isEmpty());

  fixture.proxy->drainInputs();
  fixture.drainQueue();

  EXPECT_EQ(fixture.proxy->getPropertyUntyped(altitude).getCopyAs<double>(), 1200.0);
  ASSERT_EQ(told.size(), 1U);
  EXPECT_EQ(told.front(), TimeStamp {200});
  EXPECT_EQ(fixture.proxy->getLastCommitTime(), TimeStamp {200});
}

/// @test
/// Notifies nothing at a drain when no value changed since the last one.
TEST(GenericLocalObjectProxy, aDrainTellsNothingWhenNothingChanged)
{
  const ProxyFixture fixture;
  fixture.set("altitude", 5.0);
  fixture.owner->commit(TimeStamp {100});
  fixture.proxy->drainInputs();
  fixture.drainQueue();

  bool told = false;
  auto guard = fixture.proxy->onPropertyChangedUntyped(fixture.property("altitude"), fixture.recorder(told));

  fixture.owner->commit(TimeStamp {200});
  fixture.owner->commit(TimeStamp {300});
  fixture.proxy->drainInputs();
  fixture.drainQueue();

  EXPECT_FALSE(told);
}

/// @test
/// Takes a static property's value at a drain without notifying it.
TEST(GenericLocalObjectProxy, aStaticPropertyIsTakenWithoutTelling)
{
  const ProxyFixture fixture;
  const auto* callsign = fixture.property("callsign");

  bool told = false;
  auto guard = fixture.proxy->onPropertyChangedUntyped(callsign, fixture.recorder(told));

  fixture.set("callsign", std::string("viper"));
  fixture.proxy->drainInputs();
  fixture.drainQueue();

  EXPECT_EQ(fixture.proxy->getPropertyUntyped(callsign).getCopyAs<std::string>(), "viper");
  EXPECT_FALSE(told);
}

/// @test
/// Writes its values as of its last drain, not the owner's latest.
TEST(GenericLocalObjectProxy, writesTheValuesItHolds)
{
  const ProxyFixture fixture;
  fixture.set("callsign", std::string("viper"));
  fixture.set("altitude", 10.0);
  fixture.owner->commit(TimeStamp {100});
  fixture.proxy->drainInputs();

  EXPECT_EQ(generic_test::collectAll(*fixture.proxy), generic_test::collectAll(*fixture.owner));
  EXPECT_EQ(generic_test::collectRecords(*fixture.proxy, generic_test::Access::writeStatic()),
            generic_test::collectRecords(*fixture.owner, generic_test::Access::writeStatic()));
  EXPECT_EQ(generic_test::collectRecords(*fixture.proxy, generic_test::Access::writeDynamic()),
            generic_test::collectRecords(*fixture.owner, generic_test::Access::writeDynamic()));

  const auto before = generic_test::collectAll(*fixture.proxy);
  fixture.set("altitude", 20.0);
  fixture.owner->commit(TimeStamp {200});
  EXPECT_EQ(generic_test::collectAll(*fixture.proxy), before);
  EXPECT_NE(generic_test::collectAll(*fixture.owner), before);
}

/// @test
/// After one property changes, a drain notifies only that one, and every value reads as the owner's.
TEST(GenericLocalObjectProxy, aDrainTakesOnlyTheChangedProperty)
{
  const ProxyFixture fixture;
  const auto* altitude = fixture.property("altitude");
  const auto* status = fixture.property("status");
  fixture.set("callsign", std::string("viper"));
  fixture.set("altitude", 1000.0);
  fixture.set("status", std::string("climbing"));
  fixture.owner->commit(TimeStamp {100});
  fixture.proxy->drainInputs();
  fixture.drainQueue();

  bool toldAltitude = false;
  bool toldStatus = false;
  auto altitudeGuard = fixture.proxy->onPropertyChangedUntyped(altitude, fixture.recorder(toldAltitude));
  auto statusGuard = fixture.proxy->onPropertyChangedUntyped(status, fixture.recorder(toldStatus));

  fixture.set("altitude", 2000.0);
  fixture.owner->commit(TimeStamp {200});
  fixture.proxy->drainInputs();
  fixture.drainQueue();

  EXPECT_TRUE(toldAltitude);
  EXPECT_FALSE(toldStatus);
  EXPECT_EQ(fixture.proxy->getPropertyUntyped(altitude).getCopyAs<double>(), 2000.0);
  EXPECT_EQ(fixture.proxy->getPropertyUntyped(status).getCopyAs<std::string>(), "climbing");
  EXPECT_EQ(fixture.proxy->getPropertyUntyped(fixture.property("callsign")).getCopyAs<std::string>(), "viper");
  EXPECT_EQ(generic_test::collectAll(*fixture.proxy), generic_test::collectAll(*fixture.owner));
}

/// @test
/// Forwards invokeAllPropertyCallbacks to the owner, whose subscribers are called.
TEST(GenericLocalObjectProxy, invokeAllPropertyCallbacksGoesToTheOwner)
{
  const ProxyFixture fixture;

  bool ownerTold = false;
  auto guard = fixture.owner->onPropertyChangedUntyped(fixture.property("altitude"), fixture.recorder(ownerTold));

  fixture.proxy->invokeAllPropertyCallbacks();
  fixture.drainQueue();

  EXPECT_TRUE(ownerTold);
}

/// @test
/// Forwards a method call to the owner: a getter call returns the owner's value.
TEST(GenericLocalObjectProxy, aMethodCallGoesToTheOwner)
{
  const ProxyFixture fixture;
  fixture.set("altitude", 64.0);
  fixture.owner->commit(TimeStamp {100});

  std::optional<sen::MethodResult<sen::Var>> answer;
  fixture.proxy->invokeUntyped(
    &fixture.property("altitude")->getGetterMethod(),
    {},
    sen::MethodCallback<sen::Var>(
      fixture.getQueue(),
      [&answer](const sen::MethodCallInfo& /*info*/, const sen::MethodResult<sen::Var>& result) { answer = result; }));
  fixture.drainQueue();

  ASSERT_TRUE(answer.has_value());
  ASSERT_TRUE(answer->isOk());
  EXPECT_EQ(answer->getValue().getCopyAs<double>(), 64.0);
}

/// @test
/// Stops notifying an untyped subscriber once its connection guard is destroyed.
TEST(GenericLocalObjectProxy, aRemovedConnectionIsNotTold)
{
  const ProxyFixture fixture;

  bool told = false;
  {
    auto guard = fixture.proxy->onPropertyChangedUntyped(fixture.property("altitude"), fixture.recorder(told));
  }

  fixture.set("altitude", 1.0);
  fixture.owner->commit(TimeStamp {100});
  fixture.proxy->drainInputs();
  fixture.drainQueue();

  EXPECT_FALSE(told);
}

/// @test
/// A GenericLocalObject makes a GenericLocalObjectProxy with the subscriber's prefix.
TEST(MakeLocalProxy, givesAGenericLocalObjectTheGenericProxy)
{
  const ProxyFixture fixture;
  const auto proxy = fixture.owner->senImplMakeLocalProxy("subscriber");
  ASSERT_NE(proxy, nullptr);
  EXPECT_NE(dynamic_cast<GenericLocalObjectProxy*>(proxy.get()), nullptr);
  EXPECT_EQ(proxy->getLocalName(), "subscriber.track");
}

/// @test
/// Making the generic proxy returns nullptr instead of throwing, here for an object without a shared_ptr.
TEST(MakeLocalProxy, isNullRatherThanAThrow)
{
  GenericLocalObject loose("loose", generic_test::makeTrackClass("generic_test.LooseTrack"));
  std::shared_ptr<sen::impl::NativeObjectProxy> proxy;
  EXPECT_NO_THROW(proxy = loose.senImplMakeLocalProxy("subscriber"));
  EXPECT_EQ(proxy, nullptr);
}

/// @test
/// A generated local proxy notifies a changed value once, with the commit's time.
TEST(GeneratedLocalProxy, tellsOfAChangeWithTheCommitTime)
{
  const auto generated = std::make_shared<Generated>("generated");
  auto* queue = sen::impl::getWorkQueue(generated.get());
  queue->enable();
  const auto* height = rich::RichInterface::meta()->searchPropertyByName("height");

  const auto proxy = generated->getClass()->createLocalProxy(generated.get(), "subscriber");
  ASSERT_NE(proxy, nullptr);
  EXPECT_NE(dynamic_cast<rich::RichInterface*>(proxy.get()), nullptr);
  drainAndRun(*proxy, queue);  // discovery drains a new proxy once

  std::vector<TimeStamp> told;
  auto guard = recordTimes(*proxy, height, queue, told);

  generated->setNextPropertyUntyped(height, 12.5);
  generated->commit(TimeStamp {100});
  drainAndRun(*proxy, queue);
  ASSERT_EQ(told.size(), 1U);
  EXPECT_EQ(told.front(), TimeStamp {100});

  told.clear();
  generated->commit(TimeStamp {200});
  drainAndRun(*proxy, queue);
  EXPECT_TRUE(told.empty());
}

/// @test
/// A generated proxy ignores PropertyChangeTimes and notifies only changed values.
TEST(GeneratedLocalProxy, ignoresChangeTimes)
{
  const auto timed = std::make_shared<Timed>("timed");
  auto* queue = sen::impl::getWorkQueue(timed.get());
  queue->enable();
  const auto* height = rich::RichInterface::meta()->searchPropertyByName("height");

  const auto proxy = timed->senImplMakeLocalProxy("subscriber");
  ASSERT_NE(proxy, nullptr);
  EXPECT_NE(dynamic_cast<rich::RichInterface*>(proxy.get()), nullptr);
  drainAndRun(*proxy, queue);

  std::vector<TimeStamp> told;
  auto guard = recordTimes(*proxy, height, queue, told);

  timed->changed = TimeStamp {50};
  timed->setNextPropertyUntyped(height, 12.5);
  timed->commit(TimeStamp {100});
  drainAndRun(*proxy, queue);
  ASSERT_EQ(told.size(), 1U);
  EXPECT_EQ(told.front(), TimeStamp {100});

  told.clear();
  timed->changed = TimeStamp {150};
  timed->commit(TimeStamp {200});
  drainAndRun(*proxy, queue);
  EXPECT_TRUE(told.empty());
}

/// @test
/// A GenericLocalObjectProxy also reads and writes the values of a generated object.
TEST(GenericLocalObjectProxy, servesAnyNativeObject)
{
  const auto generated = std::make_shared<Generated>("generated");
  sen::impl::getWorkQueue(generated.get())->enable();
  const auto meta = rich::RichInterface::meta();
  const auto* height = meta->searchPropertyByName("height");
  const auto* label = meta->searchPropertyByName("label");
  generated->setNextPropertyUntyped(height, 12.5);
  generated->setNextPropertyUntyped(label, std::string("rich"));
  generated->commit(TimeStamp {100});

  const auto proxy = std::make_shared<GenericLocalObjectProxy>(generated.get(), nullptr, nullptr, "localProxy");
  proxy->drainInputs();

  EXPECT_EQ(proxy->getPropertyUntyped(height).getCopyAs<double>(), 12.5);
  EXPECT_EQ(proxy->getPropertyUntyped(label).getCopyAs<std::string>(), "rich");
  EXPECT_EQ(generic_test::collectAll(*proxy), generic_test::collectAll(*generated));
}

/// @test
/// With PropertyChangeTimes, the proxy notifies a property whose time moved, with that time, even if
/// its value did not change.
TEST(GenericLocalObjectProxy, anObjectWithChangeTimesIsToldOfByThem)
{
  const auto timed = std::make_shared<Timed>("timed");
  auto* queue = sen::impl::getWorkQueue(timed.get());
  queue->enable();
  const auto* height = rich::RichInterface::meta()->searchPropertyByName("height");
  timed->setNextPropertyUntyped(height, 12.5);
  timed->changed = TimeStamp {100};
  timed->commit(TimeStamp {100});

  const auto proxy = std::make_shared<GenericLocalObjectProxy>(timed.get(), nullptr, timed.get(), "localProxy");
  std::vector<TimeStamp> told;
  auto guard = recordTimes(*proxy, height, queue, told);

  drainAndRun(*proxy, queue);
  EXPECT_TRUE(told.empty());

  timed->changed = TimeStamp {200};
  timed->commit(TimeStamp {300});
  drainAndRun(*proxy, queue);
  ASSERT_EQ(told.size(), 1U);
  EXPECT_EQ(told.front(), TimeStamp {200});
  EXPECT_EQ(proxy->getPropertyUntyped(height).getCopyAs<double>(), 12.5);

  told.clear();
  timed->commit(TimeStamp {400});
  drainAndRun(*proxy, queue);
  EXPECT_TRUE(told.empty());
}
