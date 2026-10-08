// === test_kernel_test.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "message_dispatcher.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/object_source.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/kernel_config.h"
#include "sen/kernel/test_kernel.h"
#include "sen/kernel/tracer.h"

// generated code
#include "test_kernel/stl/my_class.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

/// Dummy class to test the concept of instantiating objects in tests.
class MyClassImpl: public test::MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(MyClassImpl)

public:
  using MyClassBase::MyClassBase;

  ~MyClassImpl() override = default;

  using MyClassBase::somethingHappened;
};

void runKernelStepAndDestroy(sen::kernel::TestComponent& component)
{
  sen::kernel::TestKernel kernel(&component);
  kernel.step();
}

/// A component that runs its own cycle instead of calling execLoop, as a bridge to a system with
/// its own clock does. Real-time only, so the kernel leaves it out of the set it advances. The
/// kernel waits only for a component that is inside execLoop.
class SelfDrivenComponent: public sen::kernel::TestComponent
{
public:
  SEN_NOCOPY_NOMOVE(SelfDrivenComponent)

public:
  SelfDrivenComponent() = default;
  ~SelfDrivenComponent() override = default;

  [[nodiscard]] bool isRealTimeOnly() const noexcept override { return true; }
};

//--------------------------------------------------------------------------------------------------------------
// Tests
//--------------------------------------------------------------------------------------------------------------

/// @test
/// Creates a virtual-time kernel from an empty yaml string without throwing.
/// @requirements(SEN-361)
TEST(TestKernel, emptyConfig) { EXPECT_NO_THROW(auto kernel = sen::kernel::TestKernel::fromYamlString("")); }

// Distinct from std::abort so the check below cannot pass by the two being folded together.
volatile int terminateHandlerCalls = 0;

[[noreturn]] void testTerminateHandler()
{
  terminateHandlerCalls++;
  std::abort();
}

/// @test
/// Destroying a kernel must leave crash reporting alone. It is armed once for the whole process by
/// whoever owns the process, so a kernel that never armed it must not disarm it on the way out.
/// @requirements(SEN-361)
TEST(TestKernel, destroyingAKernelLeavesTheProcessTerminateHandlerAlone)
{
  // Installing our own first, rather than reading whatever is there: a kernel destroyed by an
  // earlier test in this binary would already have changed it, and the check would then hold
  // whether or not the defect is present.
  auto* const previous = std::set_terminate(testTerminateHandler);

  {
    auto kernel = sen::kernel::TestKernel::fromYamlString("");
  }

  auto* const after = std::get_terminate();
  std::set_terminate(previous);

  EXPECT_EQ(after, testTerminateHandler);
}

/// @test
/// Steps a one-component kernel in virtual time: each step runs exactly one cycle, the cycle sees the time before
/// the advance while the kernel ends one period ahead, a property set in a cycle is readable after that step, and
/// event and property-change callbacks arrive one step later.
/// @requirements(SEN-363)
TEST(TestKernel, oneComponent)
{
  // --8<-- [start:setup]
  // to track the evolution of the test
  int32_t counter = 0;
  int32_t eventCount = 0;
  int32_t propCount = 0;
  sen::TimeStamp lastTime;

  // the object that we will stimulate
  auto object = std::make_shared<MyClassImpl>("myObject", sen::VarMap {});

  // set up a component that will hold our object
  sen::kernel::TestComponent component;

  // on init we register the object and set a callback to track event counts
  std::shared_ptr<sen::ObjectSource> source;
  component.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      source = api.getSource("local.test");
      source->add(object);
      return sen::kernel::done();
    });

  // on each iteration we track the count and last simulation time
  bool emitEvent = true;
  bool emitProp = true;
  component.onRun(
    [&](auto& api)
    {
      object->onSomethingHappened({api.getWorkQueue(), [&]() { ++eventCount; }}).keep();
      object->onPropChanged({api.getWorkQueue(), [&]() { ++propCount; }}).keep();
      return api.execLoop(std::chrono::seconds(1),
                          [&]()
                          {
                            ++counter;
                            lastTime = api.getTime();
                            if (emitEvent)
                            {
                              object->somethingHappened();
                            }
                            if (emitProp)
                            {
                              object->setNextProp(object->getProp() + 1);
                            }
                          });
    });

  // create the kernel holding only our simple component
  sen::kernel::TestKernel kernel(&component);
  // --8<-- [end:setup]

  // iteration 0, time is 0
  emitEvent = false;
  emitProp = false;
  kernel.step();
  EXPECT_EQ(1, counter);
  EXPECT_EQ(0, eventCount);
  EXPECT_EQ(0, object->getProp());
  EXPECT_EQ(0, propCount);
  EXPECT_EQ(std::chrono::seconds(0), lastTime.sinceEpoch().toChrono());
  EXPECT_EQ(std::chrono::seconds(1), kernel.getTime().sinceEpoch().toChrono());

  // --8<-- [start:delay]
  // iteration 1, sim time was 1s, we have executed 2s, we emit the event
  emitEvent = true;
  emitProp = true;
  kernel.step();
  EXPECT_EQ(2, counter);
  EXPECT_EQ(0, eventCount);
  EXPECT_EQ(1, object->getProp());
  EXPECT_EQ(0, propCount);
  EXPECT_EQ(std::chrono::seconds(1), lastTime.sinceEpoch().toChrono());
  EXPECT_EQ(std::chrono::seconds(2), kernel.getTime().sinceEpoch().toChrono());

  // iteration 2, sim time was 2s, we have executed 3s, we received the event
  emitEvent = false;
  emitProp = false;
  kernel.step();
  EXPECT_EQ(3, counter);
  EXPECT_EQ(1, eventCount);
  EXPECT_EQ(1, object->getProp());
  EXPECT_EQ(1, propCount);
  EXPECT_EQ(std::chrono::seconds(2), lastTime.sinceEpoch().toChrono());
  EXPECT_EQ(std::chrono::seconds(3), kernel.getTime().sinceEpoch().toChrono());
  // --8<-- [end:delay]

  for (std::size_t i = counter; i < 10; ++i)
  {
    emitEvent = false;
    emitProp = false;
    kernel.step();
    EXPECT_EQ(i + 1, counter);
    EXPECT_EQ(1, eventCount);
    EXPECT_EQ(1, object->getProp());
    EXPECT_EQ(1, propCount);
    EXPECT_EQ(std::chrono::seconds(counter - 1), lastTime.sinceEpoch().toChrono());
    EXPECT_EQ(std::chrono::seconds(counter), kernel.getTime().sinceEpoch().toChrono());
  }

  // iteration n
  emitEvent = true;
  emitProp = true;
  kernel.step();
  EXPECT_EQ(1, eventCount);
  EXPECT_EQ(2, object->getProp());
  EXPECT_EQ(1, propCount);
  EXPECT_EQ(std::chrono::seconds(counter - 1), lastTime.sinceEpoch().toChrono());
  EXPECT_EQ(std::chrono::seconds(counter), kernel.getTime().sinceEpoch().toChrono());

  // iteration n+1 we should receive the event
  emitEvent = false;
  emitProp = false;
  kernel.step();
  EXPECT_EQ(2, eventCount);
  EXPECT_EQ(2, object->getProp());
  EXPECT_EQ(2, propCount);
  EXPECT_EQ(std::chrono::seconds(counter - 1), lastTime.sinceEpoch().toChrono());
  EXPECT_EQ(std::chrono::seconds(counter), kernel.getTime().sinceEpoch().toChrono());

  source.reset();
}

/// @test
/// Refuses a second object whose name an object on the same bus already holds, the add returning false.
/// @requirements(SEN-580)
TEST(TestKernel, repeatedNames)
{
  // the object that we will stimulate
  auto object1 = std::make_shared<MyClassImpl>("myObject", sen::VarMap {});
  auto object2 = std::make_shared<MyClassImpl>("myObject", sen::VarMap {});

  // set up a component that will hold our object
  sen::kernel::TestComponent component;

  component.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      auto source = api.getSource("local.test");
      source->add(object1);
      EXPECT_FALSE(source->add(object2));
      return sen::kernel::done();
    });

  component.onRun([&](auto& api) { return api.execLoop(std::chrono::seconds(1), [&]() {}); });

  sen::kernel::TestKernel kernel(&component);
  kernel.step();
}

/// @test
/// A component driving its own cycle can stamp what it sends with the time its data came from
/// instead of the kernel's, which is what a bridge to a system with its own clock needs.
TEST(TestKernel, commitStampsTheObjectsWithTheTimeGiven)
{
  const sen::TimeStamp originTime {sen::Duration {std::chrono::seconds {1234}}};

  auto object = std::make_shared<MyClassImpl>("myObject", sen::VarMap {});

  SelfDrivenComponent component;
  std::shared_ptr<sen::ObjectSource> source;
  component.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      source = api.getSource("local.test");
      source->add(object);
      return sen::kernel::done();
    });

  // The cycle a component runs for itself, as the python binding does. With no execLoop nothing
  // commits after this, so this stamp is the one subscribers receive.
  std::promise<void> cycleRan;
  auto cycleDone = cycleRan.get_future();
  sen::TimeStamp senTime;
  component.onRun(
    [&](auto& api)
    {
      api.drainInputs();
      api.update();
      object->setNextProp(object->getProp() + 1);
      senTime = api.getTime();
      api.commit(originTime);
      cycleRan.set_value();
      return sen::kernel::done();
    });

  sen::kernel::TestKernel kernel(&component);

  // The component has its own thread and the kernel does not step it, so wait for its cycle and
  // not for a step. The wait is bounded so a regression fails here instead of hanging the suite.
  ASSERT_EQ(std::future_status::ready, cycleDone.wait_for(std::chrono::seconds {10}));

  EXPECT_EQ(object->getLastCommitTime(), originTime);

  // The kernel kept its own time while the component tagged its data.
  EXPECT_NE(senTime, originTime);
}

/// @test
/// commit() with no argument stamps the kernel's time, so a component that does not ask for the
/// overload sees no change.
TEST(TestKernel, commitWithoutATimeStampsTheKernelsOwn)
{
  auto object = std::make_shared<MyClassImpl>("myObject", sen::VarMap {});

  SelfDrivenComponent component;
  std::shared_ptr<sen::ObjectSource> source;
  component.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      source = api.getSource("local.test");
      source->add(object);
      return sen::kernel::done();
    });

  std::promise<void> cycleRan;
  auto cycleDone = cycleRan.get_future();
  sen::TimeStamp senTime;
  component.onRun(
    [&](auto& api)
    {
      api.drainInputs();
      api.update();
      object->setNextProp(object->getProp() + 1);
      senTime = api.getTime();
      api.commit();
      cycleRan.set_value();
      return sen::kernel::done();
    });

  sen::kernel::TestKernel kernel(&component);
  ASSERT_EQ(std::future_status::ready, cycleDone.wait_for(std::chrono::seconds {10}));

  EXPECT_EQ(object->getLastCommitTime(), senTime);
}

/// @test
/// execLoop commits at the end of every cycle, so a stamp given inside its work function is
/// replaced by the kernel's. The overload is for a component that runs its own cycle.
TEST(TestKernel, execLoopReplacesAStampGivenInsideIt)
{
  const sen::TimeStamp originTime {sen::Duration {std::chrono::seconds {1234}}};

  auto object = std::make_shared<MyClassImpl>("myObject", sen::VarMap {});

  sen::kernel::TestComponent component;
  std::shared_ptr<sen::ObjectSource> source;
  component.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      source = api.getSource("local.test");
      source->add(object);
      return sen::kernel::done();
    });

  component.onRun(
    [&](auto& api)
    {
      return api.execLoop(std::chrono::seconds(1),
                          [&]()
                          {
                            object->setNextProp(object->getProp() + 1);
                            api.commit(originTime);
                          });
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step();

  EXPECT_NE(object->getLastCommitTime(), originTime);
}

/// @test
/// A pipeline object whose bus address leaves the segment after the dot empty is rejected at
/// configuration load, and the error names the incomplete bus address.
TEST(TestKernel, busAddressMustBeFullySpecified)
{
  const auto* yaml = R"yaml(
build:
  - name: comp
    group: 1
    freqHz: 100
    imports: [my_package]
    objects:
      - name: obj
        class: my_package.MyClass
        bus: "session."
)yaml";
  try
  {
    sen::kernel::TestKernel::fromYamlString(yaml);
    FAIL() << "an incomplete bus address should have been rejected";
  }
  catch (const std::runtime_error& e)
  {
    // Not merely that something threw: loading throws runtime_error for an unregistered
    // class or a bad field too, so only the message pins the path this test is about.
    EXPECT_NE(std::string(e.what()).find("incomplete bus address"), std::string::npos) << e.what();
  }
}

/// @test
/// Refuses a duplicate object name on the same bus across two local participants, both while the first object is
/// still pending and after it is published.
/// @requirements(SEN-580)
TEST(TestKernel, repeatedNamesAcrossLocalParticipants)
{
  auto object1 = std::make_shared<MyClassImpl>("myObject", sen::VarMap {});
  auto object2 = std::make_shared<MyClassImpl>("myObject", sen::VarMap {});

  std::shared_ptr<sen::ObjectSource> source1;
  std::shared_ptr<sen::ObjectSource> source2;
  bool object1Added = false;
  bool object2Added = true;

  sen::kernel::TestComponent component1;
  component1.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      source1 = api.getSource("local.test");
      object1Added = source1->add(object1);
      return sen::kernel::done();
    });
  component1.onRun([&](auto& api) { return api.execLoop(std::chrono::seconds(1), [&]() {}); });

  sen::kernel::TestComponent component2;
  component2.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      source2 = api.getSource("local.test");
      object2Added = source2->add(object2);
      return sen::kernel::done();
    });
  component2.onRun([&](auto& api) { return api.execLoop(std::chrono::seconds(1), [&]() {}); });

  // Keep the first object pending during both initializations
  sen::kernel::KernelConfig config;
  sen::kernel::KernelConfig::ComponentToLoad component1ToLoad;
  component1ToLoad.component.instance = &component1;
  component1ToLoad.component.info.name = "component1";
  component1ToLoad.config.group = 2U;
  config.addToLoad(std::move(component1ToLoad));

  sen::kernel::KernelConfig::ComponentToLoad component2ToLoad;
  component2ToLoad.component.instance = &component2;
  component2ToLoad.component.info.name = "component2";
  component2ToLoad.config.group = 2U;
  config.addToLoad(std::move(component2ToLoad));

  sen::kernel::TestKernel kernel(std::move(config));

  // Check the pending object
  EXPECT_TRUE(object1Added);
  EXPECT_FALSE(object2Added);
  EXPECT_NE(source2, nullptr);

  if (object1Added && source2 != nullptr)
  {
    // Publish the first object
    kernel.step();

    // Check again with a new object when first object is already published
    auto object3 = std::make_shared<MyClassImpl>("myObject", sen::VarMap {});
    EXPECT_FALSE(source2->add(object3));
  }

  source1.reset();
  source2.reset();
}

/// @test
/// Accepts objects with the same name from different local participants when they sit on different buses, both
/// adds succeeding.
/// @requirements(SEN-580)
TEST(TestKernel, sameNameDifferentBuses)
{
  auto object1 = std::make_shared<MyClassImpl>("myObject", sen::VarMap {});
  auto object2 = std::make_shared<MyClassImpl>("myObject", sen::VarMap {});

  std::shared_ptr<sen::ObjectSource> source1;
  std::shared_ptr<sen::ObjectSource> source2;
  bool object1Added = false;
  bool object2Added = false;

  sen::kernel::TestComponent component1;
  component1.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      source1 = api.getSource("local.first");
      object1Added = source1->add(object1);
      return sen::kernel::done();
    });

  sen::kernel::TestComponent component2;
  component2.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      source2 = api.getSource("local.second");
      object2Added = source2->add(object2);
      return sen::kernel::done();
    });

  sen::kernel::KernelConfig config;
  sen::kernel::KernelConfig::ComponentToLoad component1ToLoad;
  component1ToLoad.component.instance = &component1;
  component1ToLoad.component.info.name = "component1";
  component1ToLoad.config.group = 2U;
  config.addToLoad(std::move(component1ToLoad));

  sen::kernel::KernelConfig::ComponentToLoad component2ToLoad;
  component2ToLoad.component.instance = &component2;
  component2ToLoad.component.info.name = "component2";
  component2ToLoad.config.group = 2U;
  config.addToLoad(std::move(component2ToLoad));

  sen::kernel::TestKernel kernel(std::move(config));

  // Both objects are valid
  EXPECT_TRUE(object1Added);
  EXPECT_TRUE(object2Added);

  source1.reset();
  source2.reset();
}

/// @test
/// Destroying a MessageDispatcher with a buffer-owning work item still queued clears the pending work before the
/// internal ByteBufferManager is torn down.
/// @requirements(SEN-1613)
/// IMPORTANT NOTE: This test only fails with Asan enabled in the call to dispatcher.reset(). It does not fail without
/// sanitizers enabled.
TEST(TestKernel, SafeTeardownWithPendingWork)
{
  auto tracer = [](std::string_view) { return std::unique_ptr<sen::kernel::Tracer>(nullptr); };
  auto dispatcher = std::make_unique<sen::kernel::impl::MessageDispatcher>(std::move(tracer));
  auto buffer = dispatcher->getByteBufferManager().getBuffer(1024);

  sen::kernel::impl::MessageDispatcher::WorkItem work([buf = std::move(buffer)]() mutable {}, true);
  dispatcher->enqueueMessage(std::move(work));
  dispatcher.reset();
}

/// @test
/// A subscription attached and released during init leaves nothing behind: the kernel step and tear-down that
/// follow complete without throwing.
/// @requirements(SEN-362)
TEST(TestKernel, SafeSubscriptionLifecycle)
{
  sen::kernel::TestComponent component;

  component.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      auto source = api.getSource("local.test");
      sen::Subscription<sen::Object> tempSub;
      auto interest = sen::Interest::make("SELECT * FROM local.test", api.getTypes());
      tempSub.attachTo(source, interest, false);
      return sen::kernel::done();
    });

  component.onRun([&](auto& api) { return api.execLoop(std::chrono::seconds(1), [&]() {}); });

  EXPECT_NO_THROW(runKernelStepAndDestroy(component));
}

/// @test
/// A listener still attached at shutdown aborts a debug build, while a release build completes shutdown and
/// ignores the late cleanup.
/// @requirements(SEN-362)
TEST(TestKernel, LateSubscriptionDestructionLifecycle)
{
  sen::kernel::TestComponent component;
  sen::ObjectList<sen::Object> leakedList;

  component.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      auto source = api.getSource("local.test");
      auto interest = sen::Interest::make("SELECT * FROM local.test", api.getTypes());
      source->addSubscriber(interest, &leakedList, false);
      return sen::kernel::done();
    });

  component.onRun([&](auto& api) { return api.execLoop(std::chrono::seconds(1), [&]() {}); });

#if defined(DEBUG)
  EXPECT_DEATH(runKernelStepAndDestroy(component), ".*");
#else
  EXPECT_NO_THROW(runKernelStepAndDestroy(component));
#endif
}

/// @test
/// An explicit release after the kernel is destroyed throws nothing, and the destructor that follows is a safe
/// no-op.
/// @requirements(SEN-362)
TEST(TestKernel, SubscriptionTornDownTwice)
{
  sen::kernel::TestComponent component;
  auto sub = std::make_shared<sen::Subscription<sen::Object>>();

  component.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      auto source = api.getSource("local.test");
      auto interest = sen::Interest::make("SELECT * FROM local.test", api.getTypes());
      sub->attachTo(source, interest, false);
      return sen::kernel::done();
    });

  component.onRun([&](auto& api) { return api.execLoop(std::chrono::seconds(1), [&]() {}); });

  runKernelStepAndDestroy(component);

  EXPECT_NO_THROW(sub->release(true));
}

/// @test
/// Releases and destroys a subscription on a second thread while the kernel tears down, and both complete without
/// a crash.
/// @requirements(SEN-362)
TEST(TestKernel, ConcurrentSubscriptionDestruction)
{
  sen::kernel::TestComponent component;
  auto sub = std::make_shared<sen::Subscription<sen::Object>>();

  component.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      auto source = api.getSource("local.test");
      auto interest = sen::Interest::make("SELECT * FROM local.test", api.getTypes());
      sub->attachTo(source, interest, false);
      return sen::kernel::done();
    });

  component.onRun([&](auto& api) { return api.execLoop(std::chrono::milliseconds(10), [&]() {}); });

  auto kernel = std::make_unique<sen::kernel::TestKernel>(&component);
  kernel->step();

  std::thread destroyerThread(
    [&sub]()
    {
      std::this_thread::yield();
      sub->release(true);
      sub.reset();
    });

  kernel.reset();
  destroyerThread.join();

  SUCCEED();
}

/// @test
/// Monitoring is readable from a component thread while the kernel shuts down.
///
/// The kernel holds one lock across stopping and joining the component threads. A reader that waits
/// on that same lock never returns, so the join never completes and the process hangs rather than
/// failing. This test has no assertion of its own: if the deadlock comes back it stops finishing,
/// which is why the suite carries a timeout.
TEST(TestKernel, MonitoringDuringShutdownDoesNotDeadlock)
{
  sen::kernel::TestComponent component;

  std::atomic<bool> polling {true};

  component.onRun(
    [&](auto& api)
    {
      // A thread of the component's own, reading without pause. Inside the work function the read
      // only happens between cycles, which is never the moment the kernel stops.
      std::thread poller(
        [&]()
        {
          while (polling.load())
          {
            std::ignore = api.fetchMonitoringInfo();
          }
        });

      auto result = api.execLoop(std::chrono::milliseconds(1), []() {});

      polling.store(false);
      poller.join();
      return result;
    });

  auto kernel = std::make_unique<sen::kernel::TestKernel>(&component);
  kernel->step(5U);

  // The destructor requests the stop, which joins the component while that thread is still reading.
  kernel.reset();

  SUCCEED();
}

/// @test
/// Virtual-time runners report the host CPU cost of a completed cycle, but do not report real-time overruns.
/// @requirements(SEN-361)
TEST(TestKernel, VirtualTimeMonitoringDistinguishesCpuTimeFromOverruns)
{
  sen::kernel::TestComponent component;
  std::size_t cycleCount = 0U;

  component.onRun(
    [&](auto& api)
    {
      return api.execLoop(std::chrono::seconds(1),
                          [&]()
                          {
                            ++cycleCount;
                            if (cycleCount == 2U)
                            {
                              const auto info = api.fetchComponentMonitoringInfo();

                              EXPECT_TRUE(info.lastCycleExecutionCpuTime.has_value());
                              EXPECT_FALSE(info.overrunCount.has_value());
                            }
                          });
    });

  sen::kernel::TestKernel kernel(&component);
  kernel.step(2U);
}
