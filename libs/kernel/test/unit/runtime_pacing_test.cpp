// === runtime_pacing_test.cpp =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/base/duration.h"
#include "sen/core/base/timestamp.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/kernel.h"
#include "sen/kernel/kernel_config.h"
#include "sen/kernel/test_kernel.h"
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/sen/kernel/kernel_objects.stl.h"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <exception>
#include <future>
#include <limits>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace sen::kernel
{
namespace
{

using WallClock = std::chrono::steady_clock;

/// @test Runtime pacing publishes both updates and retains them when a subsequent call is rejected.
TEST(RuntimePacing, PublishesBothSettingsAndKeepsThemAfterRejectedCall)
{
  struct Observations
  {
    RuntimePacing initial;
    RuntimePacing updated;
    RuntimePacing afterRejection;
    bool sleepAccepted = false;
    bool compressionAccepted = false;
    bool invalidRejected = false;
  };

  std::promise<Observations> completed;
  auto completion = completed.get_future();
  TestComponent component;
  component.onRun(
    [&](RunApi& api)
    {
      KernelApiInterface* kernelApi = nullptr;
      Observations observed {};
      bool requested = false;
      bool rejectionRequested = false;
      bool finished = false;
      const auto subscription =
        api.selectAllFrom<KernelApiInterface>("local.kernel", [&](const auto& added) { kernelApi = *added.begin(); });

      return api.execLoop(
        std::chrono::milliseconds {10},
        [&]()
        {
          if (!kernelApi || finished)
          {
            return;
          }
          const auto pacing = kernelApi->getRuntimePacing();
          if (!requested)
          {
            observed.initial = pacing;
            requested = true;
            kernelApi->setSleepFactor(
              0.5, {api.getWorkQueue(), [&](const auto& result) { observed.sleepAccepted = !result.isError(); }});
            kernelApi->setTimeCompression(
              0.4, {api.getWorkQueue(), [&](const auto& result) { observed.compressionAccepted = !result.isError(); }});
          }
          else if (!rejectionRequested && observed.sleepAccepted && observed.compressionAccepted &&
                   pacing.sleepFactor == 0.5 && pacing.timeCompression == 0.4)
          {
            observed.updated = pacing;
            rejectionRequested = true;
            kernelApi->setTimeCompression(
              0.0, {api.getWorkQueue(), [&](const auto& result) { observed.invalidRejected = result.isError(); }});
          }
          else if (rejectionRequested && observed.invalidRejected)
          {
            observed.afterRejection = pacing;
            finished = true;
            completed.set_value(observed);
          }
        },
        false);
    });

  KernelConfig config;
  auto params = config.getParams();
  params.runMode = RunMode::realTime;
  params.crashReportDisabled = true;
  config.setParams(std::move(params));
  KernelConfig::ComponentToLoad toLoad;
  toLoad.component.instance = &component;
  toLoad.component.info.name = "runtime-pacing-property-test";
  toLoad.config.group = 2U;
  config.addToLoad(std::move(toLoad));

  Kernel kernel(std::move(config));
  const auto runResult = kernel.run(KernelBlockMode::doNotBlock);
  const auto status = completion.wait_for(std::chrono::seconds {5});
  kernel.requestStop();
  ASSERT_EQ(runResult, 0);
  ASSERT_EQ(status, std::future_status::ready);
  const auto observed = completion.get();
  EXPECT_EQ(observed.initial.sleepFactor, 1.0);
  EXPECT_EQ(observed.initial.timeCompression, 1.0);
  EXPECT_EQ(observed.updated.sleepFactor, 0.5);
  EXPECT_EQ(observed.updated.timeCompression, 0.4);
  EXPECT_TRUE(observed.invalidRejected);
  EXPECT_EQ(observed.afterRejection.sleepFactor, 0.5);
  EXPECT_EQ(observed.afterRejection.timeCompression, 0.4);
}

/// @test The local kernel API scales virtual timestamps while keeping the virtual scheduler at 10 Hz.
TEST(RuntimePacing, ChangesLogicalTimeThroughTheLocalBusInVirtualMode)
{
  TestComponent component;
  std::optional<RuntimePacing> pacing;
  bool sleepRejected = false;
  bool invalidCompressionRejected = false;
  bool zeroCompressionRejected = false;
  bool compressionAboveOneRejected = false;
  bool tenThousandTimesAccepted = false;
  std::vector<TimeStamp> samples;

  component.onRun(
    [&](RunApi& api)
    {
      const auto subscription = api.selectAllFrom<KernelApiInterface>(
        "local.kernel",
        [&](const auto& added)
        {
          auto* kernelApi = *added.begin();
          kernelApi
            ->onRuntimePacingChanged({api.getWorkQueue(),
                                      [&, kernelApi]()
                                      {
                                        if (kernelApi->getRuntimePacing().timeCompression == 0.4)
                                        {
                                          pacing = kernelApi->getRuntimePacing();
                                        }
                                      }})
            .keep();
          kernelApi->setSleepFactor(
            0.0, {api.getWorkQueue(), [&](const auto& result) { sleepRejected = result.isError(); }});
          kernelApi->setTimeCompression(
            std::numeric_limits<double>::quiet_NaN(),
            {api.getWorkQueue(), [&](const auto& result) { invalidCompressionRejected = result.isError(); }});
          kernelApi->setTimeCompression(
            0.0, {api.getWorkQueue(), [&](const auto& result) { zeroCompressionRejected = result.isError(); }});
          try
          {
            kernelApi->setTimeCompression(1.1);
          }
          catch (const std::exception&)
          {
            compressionAboveOneRejected = true;
          }
          kernelApi->setTimeCompression(
            0.0001, {api.getWorkQueue(), [&](const auto& result) { tenThousandTimesAccepted = !result.isError(); }});
          kernelApi->setTimeCompression(0.4);
        });
      return api.execLoop(std::chrono::milliseconds {100},
                          [&]()
                          {
                            if (pacing)
                            {
                              samples.push_back(api.getTime());
                            }
                          });
    });

  TestKernel kernel(&component);
  const auto deadline = WallClock::now() + std::chrono::seconds {3};
  while (!pacing && WallClock::now() < deadline)
  {
    kernel.step();
    std::this_thread::sleep_for(std::chrono::milliseconds {1});
  }
  ASSERT_TRUE(pacing.has_value());
  EXPECT_EQ(pacing->sleepFactor, 1.0);
  EXPECT_EQ(pacing->timeCompression, 0.4);
  EXPECT_TRUE(sleepRejected);
  EXPECT_TRUE(invalidCompressionRejected);
  EXPECT_TRUE(zeroCompressionRejected);
  EXPECT_TRUE(compressionAboveOneRejected);
  EXPECT_TRUE(tenThousandTimesAccepted);

  const auto before = kernel.getTime();
  kernel.step(3);
  EXPECT_EQ(kernel.getTime() - before, Duration {std::chrono::milliseconds {300}});
  ASSERT_EQ(samples.size(), 4U);
  EXPECT_EQ(samples[2] - samples[1], Duration {std::chrono::milliseconds {250}});
  EXPECT_EQ(samples[3] - samples[2], Duration {std::chrono::milliseconds {250}});
}

struct Sample
{
  WallClock::time_point wallTime;
  TimeStamp logicalTime;
};

/// @test Changing the remaining wait changes the wall cadence and keeps the scaled clock.
TEST(RuntimePacing, ChangesSleepIndependentlyOfLogicalTimeInRealMode)
{
  constexpr std::array sleepFactors {1.0, 0.5, 0.0, 1.0};
  constexpr std::size_t samplesPerPhase = 8U;
  using PhaseSamples = std::array<Sample, samplesPerPhase>;
  using Samples = std::array<PhaseSamples, sleepFactors.size()>;
  std::promise<Samples> completed;
  auto completion = completed.get_future();

  TestComponent component;
  component.onRun(
    [&](RunApi& api)
    {
      KernelApiInterface* kernelApi = nullptr;
      const auto subscription =
        api.selectAllFrom<KernelApiInterface>("local.kernel", [&](const auto& added) { kernelApi = *added.begin(); });

      Samples samples {};
      std::size_t phase = 0U;
      std::size_t sample = 0U;
      bool requested = false;
      bool applied = false;
      const auto onApplied = [&](const auto& result) { applied = !result.isError(); };

      return api.execLoop(
        std::chrono::milliseconds {100},
        [&]()
        {
          if (!kernelApi || phase == samples.size())
          {
            return;
          }
          if (!requested)
          {
            requested = true;
            if (phase == 0U)
            {
              kernelApi->setTimeCompression(0.1, {api.getWorkQueue(), onApplied});
            }
            else
            {
              kernelApi->setSleepFactor(sleepFactors.at(phase), {api.getWorkQueue(), onApplied});
            }
          }
          if (applied)
          {
            samples.at(phase).at(sample++) = {WallClock::now(), api.getTime()};
            if (sample == samplesPerPhase)
            {
              ++phase;
              sample = 0U;
              requested = false;
              applied = false;
              if (phase == samples.size())
              {
                completed.set_value(samples);
              }
            }
          }
        },
        false);
    });

  KernelConfig config;
  auto params = config.getParams();
  params.runMode = RunMode::realTime;
  params.crashReportDisabled = true;
  config.setParams(std::move(params));
  KernelConfig::ComponentToLoad toLoad;
  toLoad.component.instance = &component;
  toLoad.component.info.name = "runtime-pacing-test";
  toLoad.config.group = 2U;
  config.addToLoad(std::move(toLoad));

  Kernel kernel(std::move(config));
  const auto runResult = kernel.run(KernelBlockMode::doNotBlock);
  const auto status = completion.wait_for(std::chrono::seconds {8});
  kernel.requestStop();
  ASSERT_EQ(runResult, 0);
  ASSERT_EQ(status, std::future_status::ready);
  const auto samples = completion.get();

  const auto elapsed = [](const PhaseSamples& phase) { return phase.back().wallTime - phase[1].wallTime; };
  EXPECT_GE(elapsed(samples[0]), std::chrono::milliseconds {450});
  EXPECT_GE(elapsed(samples[1]), std::chrono::milliseconds {150});
  EXPECT_LT(elapsed(samples[1]), std::chrono::milliseconds {450});
  EXPECT_LT(elapsed(samples[2]), std::chrono::milliseconds {150});
  EXPECT_GE(elapsed(samples[3]), std::chrono::milliseconds {450});

  for (const auto& phase: samples)
  {
    const auto elapsedLogicalTime = phase.back().logicalTime - phase[1].logicalTime;
    EXPECT_GE(elapsedLogicalTime, Duration {std::chrono::seconds {6}});
  }
  EXPECT_EQ(samples[2].back().logicalTime - samples[2][1].logicalTime, Duration {std::chrono::seconds {6}});
}

}  // namespace
}  // namespace sen::kernel
