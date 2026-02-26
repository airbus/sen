// === precision_sleeper.h =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_PRECISION_SLEEPER_H
#define SEN_LIBS_KERNEL_SRC_PRECISION_SLEEPER_H

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/class_helpers.h"

// stl
#include "stl/sen/kernel/basic_types.stl.h"

// kernel implementation
#include "wall_clock.h"

// std
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <ratio>
#include <string>
#include <utility>
#include <variant>

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)) || defined(__MINGW32__) || defined(__MINGW64__)
#  include <time.h>
#  include <unistd.h>
#elif defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <Windows.h>
#endif

namespace sen::kernel
{

/// Helper to sleep and have more precise wake-ups.
/// The implementation does sleeps of different granularity, ending
/// up in a spin-lock when getting really close to the desired wake-up
/// time. This keeps the thread hot and minimizes delays.
class PrecisionSleeper
{
  SEN_NOCOPY_NOMOVE(PrecisionSleeper)

public:
  /// We need the wall-clock time to get precise time measurements.
  explicit PrecisionSleeper(WallClock& wallClock, const std::string& componentName);
  ~PrecisionSleeper() noexcept = default;

public:
  /// Blocks the calling thread for the given duration.
  /// The given duration is a minimum, but there are no guarantees about the maximum.
  /// We do the best we can to be accurate, but we depend on the OS scheduling.
  void sleep(NanoSecs duration) noexcept;

private:
  // Internally used to allow precise divisions and mean estimations.
  using NanoSecsAsDouble = std::chrono::duration<double, std::ratio<1, 1000000000>>;

private:
  /// Blocks the calling thread for the given duration.
  /// The given duration is a minimum, but there are no guarantees about the maximum.
  /// We do the best we can to be accurate, but we depend on the OS scheduling.
  void doSleepWithPrecision(NanoSecs duration) noexcept;

  /// Sleep for a relatively large time step.
  /// @param[in] duration: The duration to sleep.
  /// @return Gets duration reduced by the time we have slept.
  [[nodiscard]] NanoSecs veryCoarseGrainSleep(NanoSecs duration) noexcept;

  /// Sleep for a relatively medium time step.
  /// @param[in] duration: The duration to sleep.
  /// @return Gets duration reduced by the time we have slept.
  [[nodiscard]] NanoSecs coarseGrainSleep(NanoSecs duration) noexcept;

  /// Sleep for a small time step using the highest available clock source.
  /// @param[in, out] duration: The duration to sleep. Gets reduced by the time we have slept.
  void highResFineGrainSleep(NanoSecs duration) const noexcept;

  /// Sleep for a small time step using the default clock source.
  /// @param[in, out] duration: The duration to sleep. Gets reduced by the time we have slept.
  void lowResFineGrainSleep(NanoSecs duration) const noexcept;

  /// Update the internal stats with the observed sleep time.
  void updateStats(NanoSecsAsDouble observed) noexcept;

  /// Make a system call to sleep.
  void doSleep(NanoSecs duration) noexcept;

private:
  static constexpr NanoSecsAsDouble defaultEstimate {std::chrono::milliseconds(5U)};
  using FineGrainSleepFunc = void (PrecisionSleeper::*)(NanoSecs) const noexcept;
  using SleepFunc = void (PrecisionSleeper::*)(NanoSecs) noexcept;
  struct PrecisionSleepTimes
  {
    NanoSecs veryCoarseGrain {std::chrono::milliseconds(7U)};
    NanoSecs coarseGrain {std::chrono::milliseconds(1U)};
  };

private:
  NanoSecsAsDouble estimate_ {defaultEstimate};
  NanoSecsAsDouble mean_ {defaultEstimate};
  NanoSecsAsDouble m2_ {0};
  uint64_t count_ = 1;
  WallClock& wallClock_;
  FineGrainSleepFunc fineGrainSleepFunc_;
  SleepFunc sleepFunc_;
  PrecisionSleepTimes precisionSleepTimes_;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline PrecisionSleeper::PrecisionSleeper(WallClock& wallClock, const std::string& componentName)
  : wallClock_(wallClock)
  , fineGrainSleepFunc_(WallClock::hardwareSupportsInvariantTSC() ? &PrecisionSleeper::highResFineGrainSleep
                                                                  : &PrecisionSleeper::lowResFineGrainSleep)
  , sleepFunc_(std::holds_alternative<SystemSleep>(wallClock_.getSleepPolicy())
                 ? &PrecisionSleeper::doSleep
                 : &PrecisionSleeper::doSleepWithPrecision)
{
  // apply configured precision sleep times if needed
  const auto policyVariant = wallClock_.getSleepPolicy();
  if (const auto* policy = std::get_if<PrecisionSleep>(&policyVariant); policy != nullptr)
  {
    if (policy->veryCoarseGrainSleepTime != 0)
    {
      precisionSleepTimes_.veryCoarseGrain = policy->veryCoarseGrainSleepTime.toChrono();
    }

    if (policy->coarseGrainSleepTime != 0U)
    {
      precisionSleepTimes_.coarseGrain = policy->coarseGrainSleepTime.toChrono();
    }

    // check the env var configuration for the very coarse sleep time
    if (const auto* val = std::getenv("KERNEL_SLEEP_THRESHOLD_MS"); val != nullptr)
    {
      precisionSleepTimes_.veryCoarseGrain = std::chrono::milliseconds(std::stoul(val));
    }

    // check that the very coarse grain sleep time is not shorter than the coarse grain sleep time
    if (precisionSleepTimes_.veryCoarseGrain < precisionSleepTimes_.coarseGrain)
    {
      std::string err;
      {
        err.append("Sen Component ");
        err.append(componentName);
        err.append(" stopping due to a configuration error in the Precision Sleeper.");
        err.append("The configured very coarse grain sleep time (");
        err.append(std::to_string(precisionSleepTimes_.veryCoarseGrain.count()));
        err.append(" nanoseconds) is smaller than the coarse grain sleep time (");
        err.append(std::to_string(precisionSleepTimes_.coarseGrain.count()));
        err.append("nanoseconds). The former needs to be bigger than the latter.");
      }

      throwRuntimeError(std::move(err));
    }
  }
}

inline void PrecisionSleeper::sleep(NanoSecs duration) noexcept { std::invoke(sleepFunc_, this, duration); }

inline void PrecisionSleeper::doSleepWithPrecision(NanoSecs duration) noexcept
{
  duration = veryCoarseGrainSleep(duration);
  duration = coarseGrainSleep(duration);
  std::invoke(fineGrainSleepFunc_, this, duration);
}

inline NanoSecs PrecisionSleeper::veryCoarseGrainSleep(NanoSecs duration) noexcept
{
  // if the sleep duration is "big", try to sleep a big chunk of it first
  if (duration > precisionSleepTimes_.veryCoarseGrain)
  {
    const auto start = wallClock_.highResNow();
    doSleep(duration - precisionSleepTimes_.veryCoarseGrain);
    const auto end = wallClock_.highResNow();

    duration -= end - start;  // reduce the remaining time to sleep
  }

  return duration;
}

inline NanoSecs PrecisionSleeper::coarseGrainSleep(NanoSecs duration) noexcept
{
  // regular sleep in 1ms iterations (decreasing the duration variable)
  while (duration > estimate_)
  {
    // try to sleep for a millisecond - we might end up sleeping more than that
    const auto start = wallClock_.highResNow();
    doSleep(precisionSleepTimes_.coarseGrain);
    const auto end = wallClock_.highResNow();

    const auto observed = end - start;  // check how much did we really sleep
    duration -= observed;               // reduce the remaining time to sleep
    updateStats(observed);              // estimate how much do we really sleep
  }

  return duration;
}

inline void PrecisionSleeper::highResFineGrainSleep(NanoSecs duration) const noexcept
{
  // busy wait for the final period
  const auto end = WallClock::readTimeStampCounterRegistry() +
                   static_cast<int64_t>(wallClock_.getTscGhz() * static_cast<double>(duration.count()));

  while (WallClock::readTimeStampCounterRegistry() < end)
  {
    // no code needed
  }
}

inline void PrecisionSleeper::lowResFineGrainSleep(NanoSecs duration) const noexcept
{
  const auto end = wallClock_.lowResNow() + duration;
  while (wallClock_.lowResNow() < end)
  {
    // no code needed
  }
}

inline void PrecisionSleeper::updateStats(NanoSecsAsDouble observed) noexcept
{
  ++count_;
  const auto delta = observed - mean_;
  mean_ += delta / count_;
  m2_ += delta * (observed - mean_).count();
  const auto stdDev = std::sqrt(m2_.count() / static_cast<double>(count_ - 1));
  estimate_ = mean_ + NanoSecsAsDouble(stdDev);
}

inline void PrecisionSleeper::doSleep(NanoSecs duration) noexcept
{
#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)) || defined(__MINGW32__) || defined(__MINGW64__)
  const ::timespec req = {static_cast<time_t>(duration.count() / nsPerS),
                          static_cast<int64_t>(duration.count() % nsPerS)};
  ::nanosleep(&req, nullptr);

#elif defined(_WIN32)
  LARGE_INTEGER dueTime;

  if (duration.count() < 100)
  {
    SleepEx(0UL, FALSE);  // Allows the OS to schedule another process for a single time slice.
  }
  else
  {
    HANDLE timer = nullptr;
    if (
#  ifdef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
      (timer = CreateWaitableTimerEx(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS)) ==
        nullptr &&
#  endif
      (timer = CreateWaitableTimer(nullptr, TRUE, nullptr)) == nullptr)
    {
      return;
    }

    dueTime.QuadPart = -(LONGLONG)(duration.count() / 100U);
    SetWaitableTimer(timer, &dueTime, 0L, nullptr, nullptr, FALSE);
    WaitForSingleObject(timer, INFINITE);
    CloseHandle(timer);
  }
#endif
}

}  // namespace sen::kernel

#endif  // SEN_LIBS_KERNEL_SRC_PRECISION_SLEEPER_H
