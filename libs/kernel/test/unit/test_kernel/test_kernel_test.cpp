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
#include "sen/kernel/test_kernel.h"
#include "sen/kernel/tracer.h"

// generated code
#include "test_kernel/stl/my_class.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <thread>
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

//--------------------------------------------------------------------------------------------------------------
// Tests
//--------------------------------------------------------------------------------------------------------------

/// @test
/// Checks correct creation of kernel class in virtual time mode from an empty yaml file
/// @requirements(SEN-361)
TEST(TestKernel, emptyConfig) { EXPECT_NO_THROW(auto kernel = sen::kernel::TestKernel::fromYamlString("")); }

/// @test
/// Check correctness of a kernel instance that register an object and set up his callback on init, tracking the
/// simulation time on each kernel iteration (step) and testing the correct behaviour of the callback the event is
/// emitted.
/// @requirements(SEN-363)
TEST(TestKernel, oneComponent)
{
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
/// Checks that MessageDispatcher correctly clears pending work before tearing down its internal ByteBufferManager
/// @requirements(SEN-1613)
TEST(TestKernel, SafeTeardownWithPendingWork)
{
  auto tracer = [](std::string_view) { return std::unique_ptr<sen::kernel::Tracer>(nullptr); };
  auto dispatcher = std::make_unique<sen::kernel::impl::MessageDispatcher>(std::move(tracer));
  auto buffer = dispatcher->getByteBufferManager().getBuffer(1024);

  sen::kernel::impl::MessageDispatcher::WorkItem work([buf = std::move(buffer)]() mutable {}, true);
  dispatcher->enqueueMessage(std::move(work));

  EXPECT_NO_THROW(dispatcher.reset());
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
/// Verifies that holding a Subscription past component shutdown triggers an assertion or ignores safely
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
