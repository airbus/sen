// === my_class.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "my_class.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/numbers.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/tracer.h"

// generated code
#include "stl/my_package/basic_types.stl.h"
#include "stl/my_package/config.stl.h"
#include "stl/my_package/my_class.stl.h"

// std
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>

namespace my_package
{
MyClassImpl::MyClassImpl(const std::string& name, const sen::VarMap& args): MyClassBase(name, args)
{
  logger_ = sen::kernel::KernelApi::getOrCreateLogger("my_logger");

  logger_->info("MyClassImpl with name \"" + name + "\" created");

  setNextProp6({0.3, 0.4, 0.5, 0.5, 0.5, 0.3, 0.4, 0.5, 0.5, 0.5});

  setNextProp9(9);
}

void MyClassImpl::registered(sen::kernel::RegistrationApi& api)
{
  auto config = api.getConfig();

  auto freq = config["freqHz"].get<int64_t>();
  logger_->info("component freqHz = {}", freq);

  auto userConfig = sen::toValue<Configuration>(api.getConfig());
  std::cout << "MyComponentWithConfig config = \n" << userConfig << "\n";
}

bool MyClassImpl::prop7AcceptsSet(i32 val) const
{
  return val >= -5 && val <= 5;  // just as an example, let's say we accept a range [-5, 5]
}

void MyClassImpl::update(sen::kernel::RunApi& runApi)
{
  SEN_TRACE_ZONE(runApi.getTracer());
  logger_->trace("updating (time: {})", runApi.getTime().toLocalString());

  updateProp5(runApi);
  updateProp8(runApi);
  updateProp10(runApi);
  checkCycleCount(runApi);

  // this is to test what happens when throwing an exception
  if (getThrowException())
  {
    sen::throwRuntimeError("some error");
  }
}

void MyClassImpl::updateProp5(sen::kernel::RunApi& runApi)
{
  SEN_TRACE_ZONE(runApi.getTracer());

  auto nextProp5 = getProp5() + 1;
  setNextProp5(nextProp5);

  // we can fetch the value we just set...
  if (getNextProp5() != nextProp5)
  {
    sen::throwRuntimeError("something went wrong with prop5");
  }

  // but the current still remains
  if (getProp5() == nextProp5)
  {
    sen::throwRuntimeError("values should be different!");
  }
}

void MyClassImpl::updateProp8(sen::kernel::RunApi& runApi)
{
  SEN_TRACE_ZONE(runApi.getTracer());

  auto prop8 = getProp8();
  if (prop8 < sen::QuantityTraits<MetersU16>::max)
  {
    setNextProp8(prop8 + 1);
  }
}

void MyClassImpl::updateProp10(sen::kernel::RunApi& runApi)
{
  SEN_TRACE_ZONE(runApi.getTracer());

  if (cycleCount_ % 10 == 0)
  {
    auto prop10 = getProp10();

    if (prop10)
    {
      if (prop10.value() > 0)
      {
        setNextProp10(-10.0f);
      }
      else
      {
        setNextProp10(std::nullopt);
      }
    }
    else
    {
      setNextProp10(10.0f);
    }
  }
}

void MyClassImpl::checkCycleCount(sen::kernel::RunApi& runApi)
{
  SEN_TRACE_ZONE(runApi.getTracer());

  auto maxCycleCount = getIterationCount();
  if (maxCycleCount != 0 && cycleCount_ == maxCycleCount)
  {
    runApi.requestKernelStop(0);
  }

  cycleCount_++;
}

void MyClassImpl::someLocalMethod()
{
  // nothing to do
}

int32_t MyClassImpl::addNumbersImpl(int32_t a, int32_t b) { return a + b; }

std::string MyClassImpl::echoImpl(const std::string& message) { return message; }

void MyClassImpl::changePropsImpl()
{
  Vec2 val = getProp4();
  val.x += 0.5f;  // simply make some changes
  val.y += 1.5;   // to a property, to see the effect
  setNextProp4(val);
}

void MyClassImpl::doingSomethingDeferredImpl(std::promise<std::string>&& promise)
{
  promise.set_value("Doing something deferred");
}

void MyClassImpl::doingSomethingDeferredWithoutReturningImpl(std::promise<void>&& promise) { promise.set_value(); }

SEN_EXPORT_CLASS(MyClassImpl)

namespace
{
/// The cycle that blocks, and how many periods it blocks for. Late enough that the schedule has
/// settled, early enough that the checks after it still have cycles to run in.
constexpr uint64_t blockingCycle = 4U;
constexpr uint32_t periodsToBlock = 2U;

/// The cycle that burns processor, and how much of it. Wide enough to be several clock ticks on
/// Windows, where thread CPU time is quantised to about 15.6 ms. It is longer than the fastest
/// component's period, so that cycle also overruns, which nothing after it depends on.
constexpr uint64_t burningCycle = 7U;
constexpr int64_t nsToBurn = 50L * 1000L * 1000L;
}  // namespace

void MyMonitoredClassImpl::update(sen::kernel::RunApi& runApi)
{
  checkRuntimeStats(runApi);
  checkTheBlockedCycleWasSeen(runApi);
  blockPastThePeriod(runApi);
  checkTheBurnWasAttributedToUs(runApi);
  burnProcessor();

  if (++cycleCount_ > 10U)
  {
    runApi.requestKernelStop(0);
  }
}

void MyMonitoredClassImpl::blockPastThePeriod(sen::kernel::RunApi& runApi)
{
  if (cycleCount_ != blockingCycle)
  {
    return;
  }

  const auto info = runApi.fetchComponentMonitoringInfo();
  if (!info.cycleTime.has_value() || !info.overrunCount.has_value() || !info.missedFrameCount.has_value() ||
      !info.oversleptCount.has_value() || !info.lastCycleStartDelay.has_value())
  {
    sen::throwRuntimeError("The schedule is not being monitored, so the blocked cycle proves nothing");
  }

  overrunsBeforeBlocking_ = info.overrunCount.value();
  missedFramesBeforeBlocking_ = info.missedFrameCount.value();
  startDelayAtBlockNs_ = info.lastCycleStartDelay.value().getNanoseconds();
  blocked_ = true;

  // Sleeping rather than spinning, so the cycle uses almost no CPU time while the schedule loses
  // two whole periods. Timed, because how many cycles that costs depends on how long the sleep
  // really took, and the check afterwards bounds the count from both sides.
  const auto blockStart = std::chrono::steady_clock::now();
  std::this_thread::sleep_for(std::chrono::nanoseconds(info.cycleTime.value().getNanoseconds() * periodsToBlock));
  blockedForNs_ =
    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - blockStart).count();
}

void MyMonitoredClassImpl::burnProcessor()
{
  if (cycleCount_ != burningCycle)
  {
    return;
  }

  // Spinning rather than sleeping: this cycle has to cost processor, and it has to be our own code
  // that costs it.
  const auto burnStart = std::chrono::steady_clock::now();
  while (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - burnStart).count() <
         nsToBurn)
  {
  }

  burned_ = true;
}

void MyMonitoredClassImpl::checkTheBurnWasAttributedToUs(sen::kernel::RunApi& runApi)
{
  if (!burned_ || cycleCount_ != burningCycle + 1U)
  {
    return;
  }

  const auto info = runApi.fetchComponentMonitoringInfo();
  if (!info.lastCycleExecutionCpuTime.has_value() || !info.lastCycleComponentCpuTime.has_value())
  {
    sen::throwRuntimeError("No CPU time for the cycle that burned processor");
  }

  const auto execution = info.lastCycleExecutionCpuTime.value().getNanoseconds();
  const auto component = info.lastCycleComponentCpuTime.value().getNanoseconds();

  // The cycle did nothing but our own work, so nearly all of its processor time is ours. Half
  // survives a coarse clock and still fails if the measurement stops covering update().
  if (component * 2 < execution)
  {
    sen::throwRuntimeError("A cycle that spent " + std::to_string(nsToBurn) + " ns in our own code reports only " +
                           std::to_string(component) + " ns of " + std::to_string(execution) + " as ours");
  }
}

void MyMonitoredClassImpl::checkTheBlockedCycleWasSeen(sen::kernel::RunApi& runApi)
{
  if (!blocked_ || cycleCount_ != blockingCycle + 1U)
  {
    return;
  }

  const auto info = runApi.fetchComponentMonitoringInfo();

  // A blocked cycle loses frames, which is the wall-clock signal, and every cycle it loses is
  // counted rather than the one block that lost them.
  const auto periodNs = info.cycleTime.value().getNanoseconds();
  const auto lostCycles = info.missedFrameCount.value() - missedFramesBeforeBlocking_;

  // Bounded from above too, against the block we measured, so that counting something other than
  // cycles fails here instead of passing for being more than two. A start delay is negative when
  // the thread wakes early, and casting that to an unsigned would leave no upper bound at all.
  const auto lateStart = (startDelayAtBlockNs_ > 0) ? startDelayAtBlockNs_ : 0;
  const auto mostItCouldLose = static_cast<uint64_t>((lateStart + blockedForNs_) / periodNs) + 3U;
  if (lostCycles < periodsToBlock || lostCycles > mostItCouldLose)
  {
    sen::throwRuntimeError("Blocking for " + std::to_string(blockedForNs_) + " ns at a period of " +
                           std::to_string(periodNs) + " ns should lose between " + std::to_string(periodsToBlock) +
                           " and " + std::to_string(mostItCouldLose) + " cycles, but the count rose by " +
                           std::to_string(lostCycles));
  }

  if (info.lastCycleStartDelay.value().getNanoseconds() < 0)
  {
    sen::throwRuntimeError("The thread reports waking before the cycle it was waiting for");
  }

  // And not an overrun, because an overrun is CPU time over the period and this cycle slept.
  if (info.overrunCount.value() != overrunsBeforeBlocking_)
  {
    sen::throwRuntimeError("A cycle that slept through its period was counted as an execution time overrun");
  }

  if (!info.lastCycleExecutionCpuTime.has_value())
  {
    sen::throwRuntimeError("No CPU time for the cycle that blocked");
  }

  // Checks the cycle really did sleep rather than compute, so the absence of an overrun above is
  // the expected result and not an accident of timing.
  if (info.lastCycleExecutionCpuTime.value() >= info.cycleTime.value())
  {
    sen::throwRuntimeError(
      "The blocked cycle reports a full period of CPU time, so it was busy rather than blocked "
      "and this check is not testing what it claims to");
  }
}

void MyMonitoredClassImpl::checkRuntimeStats(sen::kernel::RunApi& runApi)
{
  // we need at least two cycles to get useful statistics
  if (cycleCount_ < 2)
  {
    return;
  }

  // check kernel monitoring info
  const auto kernelInfo = runApi.fetchMonitoringInfo();

  const std::array<std::string, 3> expectedNames {"myComponent1", "myComponent2", "myComponent3"};
  for (const auto& name: expectedNames)
  {
    if (std::find_if(kernelInfo.components.begin(),
                     kernelInfo.components.end(),
                     [&name](const auto& info) { return info.name == name; }) == kernelInfo.components.end())
    {
      sen::throwRuntimeError("Expected component missing from kernel monitoring data: " + name);
    }
  }

  // check monitoring info of the component
  const auto info = runApi.fetchComponentMonitoringInfo();

  if (!info.cycleTime.has_value())
  {
    sen::throwRuntimeError("No configured cycle time received");
  }

  if (!info.lastCycleExecutionCpuTime.has_value())
  {
    sen::throwRuntimeError("No last cycle execution CPU time received");
  }

  if (!info.overrunCount.has_value())
  {
    sen::throwRuntimeError("No real-time overrun count received");
  }
}

SEN_EXPORT_CLASS(MyMonitoredClassImpl)
}  // namespace my_package
