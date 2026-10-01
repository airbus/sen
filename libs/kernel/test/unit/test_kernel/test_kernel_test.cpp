// === test_kernel_test.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "message_dispatcher.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/result.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/object_mux.h"
#include "sen/core/obj/object_provider.h"
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
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

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

//--------------------------------------------------------------------------------------------------------------
// Tests
//--------------------------------------------------------------------------------------------------------------

/// @test
/// Checks correct creation of kernel class in virtual time mode from an empty yaml file
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
/// Check correctness of a kernel instance that register an object and set up his callback on init, tracking the
/// simulation time on each kernel iteration (step) and testing the correct behaviour of the callback the event is
/// emitted.
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
/// Checks that Sen can not have repeated object names on the same bus
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
/// A pipeline object with one side of `bus` empty (trailing or leading dot) is rejected
/// at config-validation time.
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
/// Checks that Sen can not have repeated object names on the same bus from different local participants
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
/// Checks that different local participants can publish objects with the same name on different buses
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
/// Checks that MessageDispatcher correctly clears pending work before tearing down its internal ByteBufferManager
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
/// Verifies that Subscriptions correctly cleaned up before component shutdown do not throw errors
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
/// Verifies the defined late-subscription lifecycle behavior: debug builds assert when a listener is
/// still attached at shutdown, while release builds complete shutdown and ignore the late cleanup safely.
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
/// Verifies that explicit release followed by destructor is a safe no-op
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
/// Verifies that destroying a subscription concurrently with component shutdown does not cause crashes or undefined
/// behavior
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

/// @test
/// Moving an object from one bus to another the way a component does it: remove it from the source
/// it is on, then add it to the source for the other bus. The report has each move in its own
/// cycle, so the moves here are a cycle apart rather than back to back.
TEST(TestKernel, movesAnObjectBetweenBusesAcrossCycles)
{
  auto object = std::make_shared<MyClassImpl>("myObject", sen::VarMap {});

  sen::kernel::TestComponent component;
  std::shared_ptr<sen::ObjectSource> first;
  std::shared_ptr<sen::ObjectSource> second;
  int tick = 0;
  bool movedOut = false;
  bool movedBack = false;

  component.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      first = api.getSource("local.first");
      second = api.getSource("local.second");
      EXPECT_TRUE(first->add(object)) << "the object could not be published on the first bus";
      return sen::kernel::done();
    });

  component.onRun(
    [&](auto& api)
    {
      return api.execLoop(std::chrono::seconds(3),
                          [&]()
                          {
                            ++tick;
                            if (tick == 2)
                            {
                              first->remove(object);
                              movedOut = second->add(object);
                            }
                            if (tick == 5)
                            {
                              second->remove(object);
                              movedBack = first->add(object);
                            }
                          });
    });

  sen::kernel::TestKernel kernel(&component);
  for (int step = 0; step < 8; ++step)
  {
    kernel.step();
  }

  EXPECT_TRUE(movedOut) << "moving to the second bus failed";
  EXPECT_TRUE(movedBack) << "moving back to the first bus failed";
}

//--------------------------------------------------------------------------------------------------------------
// What a second component hears when an object moves between buses
//--------------------------------------------------------------------------------------------------------------

namespace
{

/// Records presence callbacks in arrival order together with the interest they arrived on, so the
/// order can be attributed to a side rather than assumed.
class PresenceRecorder final: public sen::MuxedProviderListener
{
public:
  struct Entry
  {
    std::string kind;
    sen::InterestId interest;
    std::thread::id thread;
  };

  std::vector<Entry> entries;  // NOLINT(misc-non-private-member-variables-in-classes)

  void onObjectsAdded(const sen::ObjectAdditionList& additions) override { record("added", additions); }

  void onObjectsRemoved(const sen::ObjectRemovalList& removals) override { record("removed", removals); }

  void onExistingObjectsReadded(const sen::ObjectAdditionList& additions) override { record("readded", additions); }

  void onObjectsRefCountReduced(const sen::ObjectRemovalList& removals) override { record("refReduced", removals); }

  [[nodiscard]] std::size_t count(std::string_view kind) const
  {
    return static_cast<std::size_t>(
      std::count_if(entries.begin(), entries.end(), [kind](const Entry& entry) { return entry.kind == kind; }));
  }

  /// The whole sequence, for a failure message: a run that heard nothing reads differently from one
  /// that heard the wrong thing.
  std::thread::id publisherThread;  // NOLINT(misc-non-private-member-variables-in-classes)
  std::thread::id observerThread;   // NOLINT(misc-non-private-member-variables-in-classes)

  [[nodiscard]] std::string describe(sen::InterestId first, sen::InterestId second) const
  {
    if (entries.empty())
    {
      return "<nothing>";
    }

    std::string out;
    for (const auto& entry: entries)
    {
      if (!out.empty())
      {
        out.append(" ");
      }
      out.append(entry.kind).append("(");
      if (entry.interest == first)
      {
        out.append("first");
      }
      else if (entry.interest == second)
      {
        out.append("second");
      }
      else
      {
        out.append("other");
      }
      out.append(entry.thread == observerThread ? ",observerThread"
                                                : (entry.thread == publisherThread ? ",publisherThread" : ",other"));
      out.append(")");
    }
    return out;
  }

private:
  void record(std::string_view kind, const sen::ObjectAdditionList& additions)
  {
    for (const auto& addition: additions)
    {
      entries.push_back({std::string(kind), sen::getInterestId(addition), std::this_thread::get_id()});
    }
  }

  void record(std::string_view kind, const sen::ObjectRemovalList& removals)
  {
    for (const auto& removal: removals)
    {
      entries.push_back({std::string(kind), removal.interestId, std::this_thread::get_id()});
    }
  }
};

}  // namespace

/// @test
/// The topology the bus-move report was seen in: the observer is a second component in the same
/// process, watching both buses through one mux, while a publisher moves an object from the first
/// bus to the second and back. Each move should reach the observer as one departure and one
/// arrival, whichever direction it goes.
TEST(TestKernel, anotherComponentHearsAnObjectMoveBetweenBuses)
{
  auto object = std::make_shared<MyClassImpl>("movingObject", sen::VarMap {});

  // Declared before the kernel so they outlive it.
  sen::ObjectMux mux;
  PresenceRecorder recorder;
  sen::InterestId firstInterest {0};
  sen::InterestId secondInterest {0};

  std::shared_ptr<sen::ObjectSource> publisherFirst;
  std::shared_ptr<sen::ObjectSource> publisherSecond;
  std::shared_ptr<sen::ObjectSource> observerFirst;
  std::shared_ptr<sen::ObjectSource> observerSecond;

  int tick = 0;
  bool published = false;
  bool movedOut = false;
  bool movedBack = false;

  sen::kernel::TestComponent publisher;
  publisher.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      publisherFirst = api.getSource("local.first");
      publisherSecond = api.getSource("local.second");
      published = publisherFirst->add(object);
      return sen::kernel::done();
    });
  publisher.onRun(
    [&](auto& api)
    {
      return api.execLoop(std::chrono::seconds(3),
                          [&]()
                          {
                            recorder.publisherThread = std::this_thread::get_id();
                            ++tick;
                            if (tick == 4)
                            {
                              publisherFirst->remove(object);
                              movedOut = publisherSecond->add(object);
                            }
                            if (tick == 9)
                            {
                              publisherSecond->remove(object);
                              movedBack = publisherFirst->add(object);
                            }
                          });
    });
  publisher.onUnload(
    [&](sen::kernel::UnloadApi&& /*api*/) -> sen::kernel::FuncResult
    {
      publisherFirst.reset();
      publisherSecond.reset();
      return sen::Ok();
    });

  sen::kernel::TestComponent observer;
  observer.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      observerFirst = api.getSource("local.first");
      observerSecond = api.getSource("local.second");

      mux.addMuxedListener(&recorder, false);

      auto firstQuery = sen::Interest::make("SELECT * FROM local.first", api.getTypes());
      auto secondQuery = sen::Interest::make("SELECT * FROM local.second", api.getTypes());
      firstInterest = firstQuery->getId();
      secondInterest = secondQuery->getId();

      observerFirst->addSubscriber(firstQuery, &mux, false);
      observerSecond->addSubscriber(secondQuery, &mux, false);
      return sen::kernel::done();
    });
  observer.onRun(
    [&](auto& api)
    { return api.execLoop(std::chrono::seconds(3), [&]() { recorder.observerThread = std::this_thread::get_id(); }); });
  observer.onUnload(
    [&](sen::kernel::UnloadApi&& /*api*/) -> sen::kernel::FuncResult
    {
      observerFirst->removeSubscriber(&mux, false);
      observerSecond->removeSubscriber(&mux, false);
      mux.removeMuxedListener(&recorder, false);
      observerFirst.reset();
      observerSecond.reset();
      return sen::Ok();
    });

  sen::kernel::KernelConfig config;
  sen::kernel::KernelConfig::ComponentToLoad publisherToLoad;
  publisherToLoad.component.instance = &publisher;
  publisherToLoad.component.info.name = "publisher";
  publisherToLoad.config.group = 2U;
  config.addToLoad(std::move(publisherToLoad));

  sen::kernel::KernelConfig::ComponentToLoad observerToLoad;
  observerToLoad.component.instance = &observer;
  observerToLoad.component.info.name = "observer";
  observerToLoad.config.group = 2U;
  config.addToLoad(std::move(observerToLoad));

  {
    sen::kernel::TestKernel kernel(std::move(config));
    for (int step = 0; step < 14; ++step)
    {
      kernel.step();
    }
  }

  ASSERT_TRUE(published) << "the object was never published on the first bus";
  ASSERT_TRUE(movedOut) << "moving to the second bus failed";
  ASSERT_TRUE(movedBack) << "moving back to the first bus failed";

  const auto sequence = recorder.describe(firstInterest, secondInterest);
  ASSERT_FALSE(recorder.entries.empty()) << "the observer heard nothing at all, so this test proves nothing";

  // The publish and the two moves: three arrivals and two departures, and no move netted out into
  // a refcount change.
  EXPECT_EQ(recorder.count("added"), 3U) << sequence;
  EXPECT_EQ(recorder.count("removed"), 2U) << sequence;
  EXPECT_EQ(recorder.count("readded"), 0U) << sequence;
  EXPECT_EQ(recorder.count("refReduced"), 0U) << sequence;
}
