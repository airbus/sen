// === test_kernel_test.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/timestamp.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/test_kernel.h"

// generated code
#include "test_kernel/stl/my_class.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>

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
  sen::TimeStamp lastTime;

  // the object that we will stimulate
  auto object = std::make_shared<MyClassImpl>("myObject", sen::VarMap {});

  // set up a component that will hold our object
  sen::kernel::TestComponent component;

  // on init we register the object and set a callback to track event counts
  component.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      auto source = api.getSource("local.test");
      source->add(object);
      object->onSomethingHappened({api.getWorkQueue(), [&]() { ++eventCount; }}).keep();
      return sen::kernel::done();
    });

  // on each iteration we track the count and last simulation time
  component.onRun(
    [&](auto& api)
    {
      return api.execLoop(std::chrono::seconds(1),
                          [&]()
                          {
                            ++counter;
                            lastTime = api.getTime();
                          });
    });

  // create the kernel holding only our simple component
  sen::kernel::TestKernel kernel(&component);

  // iteration 0, time is 0
  kernel.step();
  EXPECT_EQ(1, counter);
  EXPECT_EQ(0, eventCount);
  EXPECT_EQ(std::chrono::seconds(0), lastTime.sinceEpoch().toChrono());
  EXPECT_EQ(std::chrono::seconds(1), kernel.getTime().sinceEpoch().toChrono());

  // emit the event in iteration 1
  object->somethingHappened();

  // iteration 1, sim time was 1s, we have executed 2s, we emit the event
  kernel.step();
  EXPECT_EQ(2, counter);
  EXPECT_EQ(0, eventCount);
  EXPECT_EQ(std::chrono::seconds(1), lastTime.sinceEpoch().toChrono());
  EXPECT_EQ(std::chrono::seconds(2), kernel.getTime().sinceEpoch().toChrono());

  // iteration 2, sim time was 2s, we have executed 3s, we received the event
  kernel.step();
  EXPECT_EQ(3, counter);
  EXPECT_EQ(1, eventCount);
  EXPECT_EQ(std::chrono::seconds(2), lastTime.sinceEpoch().toChrono());
  EXPECT_EQ(std::chrono::seconds(3), kernel.getTime().sinceEpoch().toChrono());

  for (std::size_t i = counter; i < 10; ++i)
  {
    kernel.step();
    EXPECT_EQ(i + 1, counter);
    EXPECT_EQ(1, eventCount);
    EXPECT_EQ(std::chrono::seconds(counter - 1), lastTime.sinceEpoch().toChrono());
    EXPECT_EQ(std::chrono::seconds(counter), kernel.getTime().sinceEpoch().toChrono());
  }

  // emit the event in iteration n
  object->somethingHappened();

  // iteration n
  kernel.step();
  EXPECT_EQ(1, eventCount);
  EXPECT_EQ(std::chrono::seconds(counter - 1), lastTime.sinceEpoch().toChrono());
  EXPECT_EQ(std::chrono::seconds(counter), kernel.getTime().sinceEpoch().toChrono());

  // iteration n+1 we should receive the event
  kernel.step();
  EXPECT_EQ(2, eventCount);
  EXPECT_EQ(std::chrono::seconds(counter - 1), lastTime.sinceEpoch().toChrono());
  EXPECT_EQ(std::chrono::seconds(counter), kernel.getTime().sinceEpoch().toChrono());
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
