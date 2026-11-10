// === stop_in_step_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/object_source.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/kernel.h"
#include "sen/kernel/kernel_config.h"
#include "sen/kernel/test_kernel.h"

// generated code
#include "stl/sen/kernel/kernel_objects.stl.h"
#include "test_kernel/stl/my_class.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <utility>

// A component asks the kernel to stop from inside a virtual-time step, while the clock that drives
// the step still has calls to make.

namespace
{

constexpr auto bus = "local.test";
constexpr auto clockBus = "local.kernel";
constexpr auto stopStep = 3;
constexpr auto exitCode = 7;

/// How long the slow component holds the step for the stop to reach the first one.
constexpr auto holdTheStep = std::chrono::seconds(2);

/// Well below the suite's ctest timeout, so that a hang is reported here rather than killed there.
constexpr auto enough = std::chrono::seconds(30);

/// An object for each component: the step it was last updated in, once committed.
class Stepper: public test::MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(Stepper)

public:
  using MyClassBase::MyClassBase;

  ~Stepper() override = default;

  using MyClassBase::setNextProp;
};

template <typename F>
void waitFor(F done, std::chrono::steady_clock::duration atMost)
{
  const auto deadline = std::chrono::steady_clock::now() + atMost;
  while (!done() && std::chrono::steady_clock::now() < deadline)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

/// A component with one object, stepped or on the wall clock.
class Part: public sen::kernel::TestComponent
{
public:
  SEN_NOCOPY_NOMOVE(Part)

public:
  Part(const std::string& name, bool realTimeOnly)
    : object(std::make_shared<Stepper>(name, sen::VarMap {})), realTimeOnly_(realTimeOnly)
  {
    onInit(
      [this](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
      {
        source = api.getSource(bus);
        std::ignore = source->add(object);
        return sen::kernel::done();
      });
  }

  ~Part() override = default;

  [[nodiscard]] bool isRealTimeOnly() const noexcept override { return realTimeOnly_; }

  void addTo(sen::kernel::KernelConfig& config, const std::string& name, u32 group)
  {
    sen::kernel::KernelConfig::ComponentToLoad toLoad;
    toLoad.component.instance = this;
    toLoad.component.info.name = name;
    toLoad.config.group = group;
    config.addToLoad(std::move(toLoad));
  }

public:
  std::shared_ptr<Stepper> object;
  std::shared_ptr<sen::ObjectSource> source;
  std::atomic<int> steps {0};

private:
  bool realTimeOnly_;
};

/// @test
/// A stop asked from inside a step, with the step's flush still to come. Components stop from the
/// highest group down, so the asker (group 4) goes while the slow one (group 3) still holds the step
/// and the driver (group 2) has its flush to send. Requires that step to be the last any component
/// took, to have been flushed, and the process to exit with the code asked for.
TEST(AStopAskedInAStep, waitsForTheStepAndEndsIt)
{
  std::atomic_bool stopAsked {false};
  std::atomic_bool askerGone {false};
  std::atomic_bool flushSent {false};

  // stops the kernel from its third step, and is the first the kernel stops
  Part asker("asker", false);
  asker.onRun(
    [&](sen::kernel::RunApi& api) -> sen::kernel::FuncResult
    {
      auto result = api.execLoop(sen::Duration(std::chrono::milliseconds(10)),
                                 [&]()
                                 {
                                   asker.object->setNextProp(++asker.steps);
                                   if (asker.steps == stopStep)
                                   {
                                     stopAsked = true;
                                     api.requestKernelStop(exitCode);
                                   }
                                 });
      askerGone = true;
      return result;
    });

  // holds that step until the asker is gone, and its own stop until the flush has been sent
  Part slow("slow", false);
  slow.onRun(
    [&](sen::kernel::RunApi& api) -> sen::kernel::FuncResult
    {
      auto result = api.execLoop(sen::Duration(std::chrono::milliseconds(10)),
                                 [&]()
                                 {
                                   slow.object->setNextProp(++slow.steps);
                                   if (slow.steps == stopStep)
                                   {
                                     waitFor([&]() { return askerGone.load(); }, holdTheStep);
                                   }
                                 });
      waitFor([&]() { return flushSent.load(); }, enough);
      return result;
    });

  // drives the steps through the kernel's clock: a step, then its flush, as the clock asks
  Part driver("driver", true);
  std::shared_ptr<sen::Subscription<sen::kernel::VirtualKernelClockInterface>> clocks;
  std::atomic_bool stepping {false};
  sen::Duration delta {};
  driver.onRun(
    [&](sen::kernel::RunApi& api) -> sen::kernel::FuncResult
    {
      clocks = api.selectAllFrom<sen::kernel::VirtualKernelClockInterface>(clockBus);
      return api.execLoop(sen::Duration(std::chrono::milliseconds(1)),
                          [&]()
                          {
                            const auto& found = clocks->list.getObjects();
                            if (found.empty() || stepping)
                            {
                              return;
                            }

                            auto* clock = found.front();
                            stepping = true;
                            clock->processNoFlush(delta,
                                                  {driver.object.get(),
                                                   [&, clock](const auto& /*result*/)
                                                   {
                                                     clock->flushOutputs({driver.object.get(),
                                                                          [&](const auto& result)
                                                                          {
                                                                            if (result.isOk())
                                                                            {
                                                                              delta = result.getValue();
                                                                            }
                                                                            stepping = false;
                                                                          }});
                                                     if (stopAsked)
                                                     {
                                                       flushSent = true;
                                                     }
                                                   }});
                          });
    });

  sen::kernel::KernelConfig config;
  asker.addTo(config, "asker", 4U);
  slow.addTo(config, "slow", 3U);
  driver.addTo(config, "driver", 2U);

  auto params = config.getParams();
  params.runMode = sen::kernel::RunMode::virtualTime;
  params.bus = clockBus;
  params.clockBus = clockBus;
  params.clockName = "clock";
  config.setParams(params);

  sen::kernel::Kernel kernel(std::move(config));
  std::atomic_bool ended {false};
  int code = -1;
  std::thread runner(
    [&]()
    {
      code = kernel.run(sen::kernel::KernelBlockMode::doBlock);
      ended = true;
    });

  waitFor([&]() { return ended.load(); }, enough);
  if (!ended)
  {
    // The threads cannot be joined, so this ends the binary here. The counters say which hang it is:
    // steps at stopStep with the stop asked is this test's, less means the run never got there.
    std::fprintf(stderr,
                 "the kernel did not stop within %llds of a stop asked inside a step"
                 " (asker steps %d, slow steps %d, stop asked %d, flush sent %d)\n",
                 static_cast<long long>(enough.count()),
                 asker.steps.load(),
                 slow.steps.load(),
                 static_cast<int>(stopAsked.load()),
                 static_cast<int>(flushSent.load()));
    std::fflush(stderr);
    std::_Exit(EXIT_FAILURE);
  }
  runner.join();

  EXPECT_EQ(code, exitCode);

  // the step the stop was asked in is the last any component took
  EXPECT_EQ(asker.steps, stopStep);
  EXPECT_EQ(slow.steps, stopStep);

  // and it was flushed: what the components set in it was committed
  EXPECT_EQ(asker.object->getProp(), stopStep);
  EXPECT_EQ(slow.object->getProp(), stopStep);

  clocks.reset();
  asker.source.reset();
  slow.source.reset();
  driver.source.reset();
}

}  // namespace
