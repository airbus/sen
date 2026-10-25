// === generic_local_object_same_process_test.cpp ======================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

/// Tests GenericLocalObjects published by one component and discovered through a
/// GenericLocalObjectProxy by another in the same process.

// test helpers
#include "generic_local_object_helpers.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_source.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/detail/generic_local_object.h"
#include "sen/kernel/detail/generic_local_object_proxy.h"
#include "sen/kernel/kernel_config.h"
#include "sen/kernel/test_kernel.h"

// generated code
#include "bus/stl/rich.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{

using generic_test::encodeFor;
using generic_test::indexOf;
using sen::kernel::impl::GenericLocalObject;
using sen::kernel::impl::GenericLocalObjectProxy;

/// The names of the objects in a subscription.
std::vector<std::string> namesIn(const sen::Subscription<sen::Object>& subscription)
{
  std::vector<std::string> names;
  for (const auto& object: subscription.list.getUntypedObjects())
  {
    names.push_back(object->getName());
  }
  std::sort(names.begin(), names.end());
  return names;
}

/// Sets the property named name of object to value, for the next commit.
void give(GenericLocalObject& object, const char* name, const sen::Var& value)
{
  object.set(indexOf(object, name), encodeFor(object, name, value));
}

/// One component publishes two GenericLocalObjects of a runtime class; another subscribes to all of
/// them, and to those a WHERE selects.
class SameProcess
{
  SEN_NOCOPY_NOMOVE(SameProcess)

public:
  SameProcess()
  {
    publisher_.onInit(
      [this](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
      {
        types_.add(generic_test::makeTrackClass(className));
        const auto type = sen::dynamicTypeHandleCast<const sen::ClassType>(types_.get(className).value()).value();

        high = std::make_shared<GenericLocalObject>("high", type);
        low = std::make_shared<GenericLocalObject>("low", type);
        give(*high, "callsign", std::string("viper"));
        give(*high, "altitude", 1000.0);
        give(*low, "callsign", std::string("maverick"));
        give(*low, "altitude", 10.0);

        publisherSource_ = api.getSource("local.objects");
        publisherSource_->add(high);
        publisherSource_->add(low);
        return sen::kernel::done();
      });
    publisher_.onRun([](sen::kernel::RunApi& api) { return api.execLoop(std::chrono::milliseconds(100), []() {}); });

    subscriber_.onRun(
      [this](sen::kernel::RunApi& api)
      {
        everything = api.selectFrom<sen::Object>("local.objects", "SELECT * FROM local.objects");
        selected = api.selectFrom<sen::Object>("local.objects", "SELECT * FROM local.objects WHERE altitude > 100");
        return api.execLoop(std::chrono::milliseconds(100), []() {});
      });

    sen::kernel::KernelConfig config;
    for (auto [component, name]: {std::pair {&publisher_, "publisher"}, std::pair {&subscriber_, "subscriber"}})
    {
      sen::kernel::KernelConfig::ComponentToLoad toLoad;
      toLoad.component.instance = component;
      toLoad.component.info.name = name;
      toLoad.config.group = 2U;
      config.addToLoad(std::move(toLoad));
    }
    kernel_ = std::make_unique<sen::kernel::TestKernel>(std::move(config));
  }

  ~SameProcess()
  {
    // the subscriptions and the objects go before the kernel
    everything.reset();
    selected.reset();
    if (publisherSource_ != nullptr)
    {
      publisherSource_->remove(high);
      publisherSource_->remove(low);
      kernel_->step(2);
    }
    publisherSource_.reset();
    kernel_.reset();
  }

public:
  /// Steps the kernel until the subscriptions hold the expected objects, or for 20 steps at most.
  void stepUntil(const std::vector<std::string>& all, const std::vector<std::string>& where)
  {
    constexpr std::size_t stepsAllowed = 20U;
    for (std::size_t k = 0U; k < stepsAllowed; ++k)
    {
      kernel_->step();
      if (everything != nullptr && selected != nullptr && namesIn(*everything) == all && namesIn(*selected) == where)
      {
        return;
      }
    }
  }

public:
  static constexpr const char* className = "generic_test.SameProcessTrack";

  std::shared_ptr<GenericLocalObject> high;                    // NOLINT(misc-non-private-member-variables-in-classes)
  std::shared_ptr<GenericLocalObject> low;                     // NOLINT(misc-non-private-member-variables-in-classes)
  std::shared_ptr<sen::Subscription<sen::Object>> everything;  // NOLINT(misc-non-private-member-variables-in-classes)
  std::shared_ptr<sen::Subscription<sen::Object>> selected;    // NOLINT(misc-non-private-member-variables-in-classes)

private:
  sen::kernel::TestComponent publisher_;
  sen::kernel::TestComponent subscriber_;
  sen::CustomTypeRegistry types_;  ///< the publisher's own, which the kernel's does not see
  std::shared_ptr<sen::ObjectSource> publisherSource_;
  std::unique_ptr<sen::kernel::TestKernel> kernel_;
};

/// A generated object with PropertyChangeTimes; it keeps its generated proxy.
struct TimedRich final: rich::RichBase, sen::kernel::impl::PropertyChangeTimes
{
  explicit TimedRich(std::string name): RichBase(std::move(name)) {}

  [[nodiscard]] sen::TimeStamp getPropertyChangeTime(sen::MemberHash /*propertyId*/) const override { return {}; }
};

/// One component publishes a GenericLocalObject and a generated object with PropertyChangeTimes.
/// Another component subscribes to both.
class ProxyChoice
{
  SEN_NOCOPY_NOMOVE(ProxyChoice)

public:
  ProxyChoice()
  {
    publisher_.onInit(
      [this](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
      {
        generic = std::make_shared<GenericLocalObject>("generic", generic_test::makeTrackClass(className));
        timed = std::make_shared<TimedRich>("timed");

        publisherSource_ = api.getSource("local.choice");
        publisherSource_->add(generic);
        publisherSource_->add(timed);
        return sen::kernel::done();
      });
    publisher_.onRun([](sen::kernel::RunApi& api) { return api.execLoop(std::chrono::milliseconds(100), []() {}); });

    subscriber_.onRun(
      [this](sen::kernel::RunApi& api)
      {
        everything = api.selectFrom<sen::Object>("local.choice", "SELECT * FROM local.choice");
        return api.execLoop(std::chrono::milliseconds(100), []() {});
      });

    sen::kernel::KernelConfig config;
    for (auto [component, name]: {std::pair {&publisher_, "publisher"}, std::pair {&subscriber_, "subscriber"}})
    {
      sen::kernel::KernelConfig::ComponentToLoad toLoad;
      toLoad.component.instance = component;
      toLoad.component.info.name = name;
      toLoad.config.group = 2U;
      config.addToLoad(std::move(toLoad));
    }
    kernel_ = std::make_unique<sen::kernel::TestKernel>(std::move(config));
  }

  ~ProxyChoice()
  {
    // the subscription and the objects go before the kernel
    everything.reset();
    if (publisherSource_ != nullptr)
    {
      publisherSource_->remove(generic);
      publisherSource_->remove(timed);
      kernel_->step(2);
    }
    publisherSource_.reset();
    kernel_.reset();
  }

public:
  /// Steps the kernel until the subscription holds both objects, or for 20 steps at most.
  void stepUntilBothArrive()
  {
    constexpr std::size_t stepsAllowed = 20U;
    for (std::size_t k = 0U; k < stepsAllowed; ++k)
    {
      kernel_->step();
      if (everything != nullptr && namesIn(*everything) == std::vector<std::string> {"generic", "timed"})
      {
        return;
      }
    }
  }

public:
  static constexpr const char* className = "generic_test.ChoiceTrack";

  std::shared_ptr<GenericLocalObject> generic;                 // NOLINT(misc-non-private-member-variables-in-classes)
  std::shared_ptr<TimedRich> timed;                            // NOLINT(misc-non-private-member-variables-in-classes)
  std::shared_ptr<sen::Subscription<sen::Object>> everything;  // NOLINT(misc-non-private-member-variables-in-classes)

private:
  sen::kernel::TestComponent publisher_;
  sen::kernel::TestComponent subscriber_;
  std::shared_ptr<sen::ObjectSource> publisherSource_;
  std::unique_ptr<sen::kernel::TestKernel> kernel_;
};

}  // namespace

/// @test
/// Another component discovers the objects through GenericLocalObjectProxies with the right values.
TEST(SameProcess, aSubscriberDiscoversGenericLocalObjectsThroughTheirProxies)
{
  SameProcess setup;
  setup.stepUntil({"high", "low"}, {"high"});

  ASSERT_NE(setup.everything, nullptr);
  ASSERT_EQ(namesIn(*setup.everything), (std::vector<std::string> {"high", "low"}));

  for (const auto& object: setup.everything->list.getUntypedObjects())
  {
    EXPECT_NE(dynamic_cast<const GenericLocalObjectProxy*>(object.get()), nullptr) << object->getName();

    const auto* altitude = object->getClass()->searchPropertyByName("altitude");
    ASSERT_NE(altitude, nullptr);
    EXPECT_EQ(object->getPropertyUntyped(altitude).getCopyAs<double>(), object->getName() == "high" ? 1000.0 : 10.0);
  }
}

/// @test
/// A WHERE selects the objects whose values match, including one whose value changes to match.
TEST(SameProcess, aWhereSelectsByValueAndFollowsAChange)
{
  SameProcess setup;
  setup.stepUntil({"high", "low"}, {"high"});

  ASSERT_NE(setup.selected, nullptr);
  ASSERT_EQ(namesIn(*setup.selected), (std::vector<std::string> {"high"}));

  // set between steps, while no component runs; the publisher's runner commits it
  give(*setup.low, "altitude", 500.0);
  setup.stepUntil({"high", "low"}, {"high", "low"});

  EXPECT_EQ(namesIn(*setup.selected), (std::vector<std::string> {"high", "low"}));
}

/// @test
/// Each object gets the local proxy it makes: the generic one, or the generated one despite change times.
TEST(SameProcess, eachObjectGetsTheLocalProxyItMakes)
{
  ProxyChoice setup;
  setup.stepUntilBothArrive();

  ASSERT_NE(setup.everything, nullptr);
  ASSERT_EQ(namesIn(*setup.everything), (std::vector<std::string> {"generic", "timed"}));

  for (const auto& object: setup.everything->list.getUntypedObjects())
  {
    const bool generic = dynamic_cast<const GenericLocalObjectProxy*>(object.get()) != nullptr;
    const bool generated = dynamic_cast<const rich::RichInterface*>(object.get()) != nullptr;
    EXPECT_NE(object->asProxyObject(), nullptr) << object->getName() << " is not a proxy";
    EXPECT_EQ(generic, object->getName() == "generic") << object->getName();
    EXPECT_EQ(generated, object->getName() == "timed") << object->getName();
  }
}
