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
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <optional>
#include <ratio>
#include <string>
#include <tuple>
#include <utility>
#include <variant>

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__)) || defined(__MINGW32__) || defined(__MINGW64__)
#  include <time.h>
#  include <unistd.h>
#  if defined(__linux__)
#    include <sys/prctl.h>
#  endif
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

/// What a sleep costs beyond what it asks for.
///
/// A sleep of X costs X plus a roughly constant amount, so the overhead is a property of the
/// machine rather than of any one step size. It is measured once and shared.
class SleepOverhead
{
public:
  using NanoSecsAsDouble = std::chrono::duration<double, std::ratio<1, 1000000000>>;

public:
  /// Starts pessimistic: assuming sleeps are cheap means taking the first few before finding out
  /// they were not, and those are the ones that oversleep.
  explicit SleepOverhead(NanoSecs initialGuess) noexcept: overhead_(NanoSecsAsDouble(initialGuess)) {}

  /// What a sleep of this size is expected to cost.
  [[nodiscard]] NanoSecsAsDouble costOf(NanoSecs grain) const noexcept { return NanoSecsAsDouble(grain) + overhead_; }

  /// Whether one more sleep of this size fits in the time left.
  [[nodiscard]] bool fits(NanoSecs remaining, NanoSecs grain) const noexcept
  {
    return NanoSecsAsDouble(remaining) > costOf(grain);
  }

  /// Fold one measured sleep into the overhead.
  ///
  /// Holds the largest cost seen rather than averaging: an average sits below half its
  /// measurements, and each of those is a sleep taken that could not be afforded.
  void observe(NanoSecs requested, NanoSecs took) noexcept
  {
    // A sleep cannot return early, so a measurement saying it did is the clock having moved rather
    // than the sleep having been cheap.
    const auto measured = NanoSecsAsDouble(took) - NanoSecsAsDouble(requested);
    if (measured.count() < 0.0)
    {
      return;
    }

    overhead_ = std::max(overhead_, measured);
    ++samples_;
  }

  /// Called once per sleep, whether or not the step ran.
  ///
  /// An over-large overhead stops the step that would measure it, so nothing would notice the
  /// machine getting faster. Easing it down is the way back, and costs at most one late cycle:
  /// the first step that runs measures the truth and puts it straight back.
  void decay() noexcept { overhead_ *= decayPerCycle; }

  [[nodiscard]] NanoSecsAsDouble overhead() const noexcept { return overhead_; }
  [[nodiscard]] uint64_t samples() const noexcept { return samples_; }

private:
  /// How much of the overhead survives a cycle that measured nothing. Slow enough not to drift
  /// below what sleeps really cost, quick enough to recover within about a second.
  static constexpr double decayPerCycle = 0.999;

  NanoSecsAsDouble overhead_;
  uint64_t samples_ {0};
};

/// Sleeps in steps of one size for as long as another step fits in the time left.
///
/// "Ops" is how a step sleeps: it takes the duration to ask for and returns what the sleep really
/// took. Production passes an adapter over the system call; a test passes a cost model, so the loop
/// can be driven over thousands of cycles without waiting for them.
template <typename Ops>
[[nodiscard]] NanoSecs stepDownWith(NanoSecs duration, NanoSecs grain, SleepOverhead& cost, Ops& ops) noexcept
{
  while (cost.fits(duration, grain))
  {
    const auto took = ops.sleepFor(grain);
    cost.observe(grain, took);
    duration -= took;
  }

  return duration;
}

struct PrecisionSleeperTestAccess;

/// Helper to sleep and have more precise wake-ups.
/// The implementation does sleeps of different granularity, ending
/// up in a spin-lock when getting really close to the desired wake-up
/// time. This keeps the thread hot and minimizes delays.
class PrecisionSleeper
{
  SEN_NOCOPY_NOMOVE(PrecisionSleeper)

  // The sleep steps only run inside a real sleep, so a test has no other way to reach them.
  friend struct PrecisionSleeperTestAccess;

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

  /// Sleep for a small time step using the highest available clock source.
  /// @param[in, out] duration: The duration to sleep. Gets reduced by the time we have slept.
  void highResFineGrainSleep(NanoSecs duration) const noexcept;

  /// Sleep for a small time step using the default clock source.
  /// @param[in, out] duration: The duration to sleep. Gets reduced by the time we have slept.
  void lowResFineGrainSleep(NanoSecs duration) const noexcept;

  /// Make a system call to sleep.
  void doSleep(NanoSecs duration) noexcept;

private:
  /// How a step sleeps in production: ask the operating system, and measure what it really took.
  class SystemSleepOps
  {
  public:
    SystemSleepOps(PrecisionSleeper& sleeper, WallClock& wallClock) noexcept: sleeper_(sleeper), wallClock_(wallClock)
    {
    }

    [[nodiscard]] NanoSecs sleepFor(NanoSecs requested) noexcept
    {
      const auto start = wallClock_.highResNow();
      sleeper_.doSleep(requested);
      return wallClock_.highResNow() - start;
    }

  private:
    PrecisionSleeper& sleeper_;
    WallClock& wallClock_;
  };

private:
  using FineGrainSleepFunc = void (PrecisionSleeper::*)(NanoSecs) const noexcept;
  using SleepFunc = void (PrecisionSleeper::*)(NanoSecs) noexcept;
  struct PrecisionSleepTimes
  {
    NanoSecs veryCoarseGrain {std::chrono::milliseconds(7U)};
    NanoSecs coarseGrain {std::chrono::milliseconds(1U)};
  };

private:
  WallClock& wallClock_;
  FineGrainSleepFunc fineGrainSleepFunc_;
  SleepFunc sleepFunc_;
  PrecisionSleepTimes precisionSleepTimes_;
  std::optional<SleepOverhead> sleepCost_;
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

  // Linux pads every timer request with 50us of slack by default, which lands inside the window
  // this class protects. Asking for none of it shortens how early the spin has to start.
#if defined(__linux__)
  std::ignore = ::prctl(PR_SET_TIMERSLACK, 1UL, 0UL, 0UL, 0UL);  // NOLINT(hicpp-vararg)
#endif

  // Seed once the configured times have settled, from the sleep about to be measured: a seed above
  // what that sleep really costs stops the step before it takes a single sample.
  sleepCost_.emplace(precisionSleepTimes_.coarseGrain);
}

inline void PrecisionSleeper::sleep(NanoSecs duration) noexcept { std::invoke(sleepFunc_, this, duration); }

inline void PrecisionSleeper::doSleepWithPrecision(NanoSecs duration) noexcept
{
  duration = veryCoarseGrainSleep(duration);

  SystemSleepOps ops {*this, wallClock_};

  // Engaged by the constructor before anything can reach here, and never reset, so the checked
  // accessor only adds a throw this noexcept function would have to terminate on.
  SEN_DEBUG_ASSERT(sleepCost_.has_value());
  auto& cost = *sleepCost_;
  cost.decay();

  duration = stepDownWith(duration, precisionSleepTimes_.coarseGrain, cost, ops);

  // A finer step here would end the sleeping closer in and cut the spin further. It is safe only
  // where a sleep costs little more than it asks for, and that overhead varies by more than an
  // order of magnitude across machines, so it needs measuring on the target first.

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
