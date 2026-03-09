// === wall_clock.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_WALL_CLOCK_H
#define SEN_LIBS_KERNEL_SRC_WALL_CLOCK_H

// sen
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/compiler_macros.h"

// stl
#include "stl/sen/kernel/basic_types.stl.h"

// std
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <thread>
#include <tuple>

// compiler-specific
#ifdef _MSC_VER
#  include <intrin.h>
#endif

namespace sen::kernel
{

using NanoSecs = std::chrono::nanoseconds;

constexpr int64_t nsPerS = NanoSecs::period::den;

/// High resolution real-time clock.
/// Tries to use the most accurate available time source.
class WallClock
{
  SEN_COPY_MOVE(WallClock)

public:
  /// Builds a wallclock.
  /// @param sleepPolicy Configurable sleep policy for the component
  /// @param initCalibrateNs Time used to initially calibrate the clock
  /// @param calibrateIntervalNs Expected interval for calibration.
  explicit WallClock(SleepPolicy sleepPolicy = PrecisionSleep {},
                     NanoSecs initCalibrateNs = std::chrono::milliseconds(50),
                     NanoSecs calibrateIntervalNs = std::chrono::seconds(2)) noexcept;
  ~WallClock() noexcept = default;

public:
  /// True if we have access to the hardware clock.
  static bool hardwareSupportsInvariantTSC();

  /// The sleep policy configured for the component, set to SystemSleep for all components if
  /// SEN_KERNEL_DISABLE_PRECISION_SLEEP is set in the environment
  [[nodiscard]] SleepPolicy getSleepPolicy() noexcept;

public:
  /// Call this from time to time. This keeps the clock aligned with the system time.
  void calibrate() noexcept;

  /// Low-level access to the TSC. Note that these are not nanoseconds, but ticks.
  [[nodiscard]] static int64_t readTimeStampCounterRegistry() noexcept;

  /// Get the relatively low-res time.
  [[nodiscard]] NanoSecs lowResNow() const noexcept;

  /// Convert TSC ticks to nanoseconds. This depends on the calibration.
  [[nodiscard]] NanoSecs timeStampCounterRegistryToNanoseconds(int64_t tsc) const noexcept;

  /// Get the current time with the highest available resolution.
  [[nodiscard]] NanoSecs highResNow() const noexcept;

  /// Get the number of gigahertz at which the TSC registry is operating.
  [[nodiscard]] double getTscGhz() const noexcept { return 1.0 / nanosecondsPerTsc_; }

  /// Get the calibration interval that was passed to the constructor.
  [[nodiscard]] NanoSecs getCalibrateIntervalNs() const noexcept { return calibrateIntervalNs_; }

private:
  using HighResTimeFunc = NanoSecs (WallClock::*)() const noexcept;

private:
  [[nodiscard]] std::tuple<uint64_t, int64_t> syncTime() noexcept;
  void saveParam(int64_t baseTsc, int64_t sysNs, int64_t baseNsError, double newNsPerTsc) noexcept;
  [[nodiscard]] NanoSecs highResNowFromRDTSC() const noexcept;

private:
  double nanosecondsPerTsc_;
  int64_t baseTsc_;
  NanoSecs baseNs_;
  SleepPolicy sleepPolicy_;
  NanoSecs calibrateIntervalNs_;
  int64_t baseNsErr_;
  int64_t nextCalibrateTsc_;
  HighResTimeFunc highResTimeFunc_;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline WallClock::WallClock(SleepPolicy sleepPolicy, NanoSecs initCalibrateNs, NanoSecs calibrateIntervalNs) noexcept
  : sleepPolicy_(std::getenv("SEN_KERNEL_DISABLE_PRECISION_SLEEP") != nullptr ? SystemSleep {} : sleepPolicy)
  , calibrateIntervalNs_(calibrateIntervalNs)
  , highResTimeFunc_(hardwareSupportsInvariantTSC() ? &WallClock::highResNowFromRDTSC : &WallClock::lowResNow)
{
  if (hardwareSupportsInvariantTSC())
  {
    // calibrate the clock
    auto [baseTsc, baseNs] = syncTime();

    auto expireNs = NanoSecs(baseNs) + initCalibrateNs;

    while (lowResNow() < expireNs)
    {
      std::this_thread::yield();
    }

    auto [delayedTsc, delayedNs] = syncTime();

    const double initNsPerTsc = static_cast<double>(delayedNs - baseNs) / static_cast<double>(delayedTsc - baseTsc);
    saveParam(std_util::checkedConversion<int64_t, std_util::ReportPolicyIgnore>(baseTsc), baseNs, 0, initNsPerTsc);
  }
}

SEN_ALWAYS_INLINE NanoSecs WallClock::highResNow() const noexcept { return std::invoke(highResTimeFunc_, this); }

SEN_ALWAYS_INLINE NanoSecs WallClock::highResNowFromRDTSC() const noexcept
{
  return timeStampCounterRegistryToNanoseconds(readTimeStampCounterRegistry());
}

inline void WallClock::calibrate() noexcept
{
  if (!hardwareSupportsInvariantTSC())
  {
    return;
  }

  if (readTimeStampCounterRegistry() < nextCalibrateTsc_)
  {
    return;
  }

  auto [tsc, ns] = syncTime();

  const auto tscI64 = std_util::checkedConversion<int64_t, std_util::ReportPolicyIgnore>(tsc);

  NanoSecs nsErr = timeStampCounterRegistryToNanoseconds(tscI64) - NanoSecs(ns);

  if (nsErr > std::chrono::milliseconds(1))
  {
    nsErr = std::chrono::milliseconds(1);
  }

  if (nsErr < -std::chrono::milliseconds(1))
  {
    nsErr = -std::chrono::milliseconds(1);
  }

  const double newNsPerTsc =
    nanosecondsPerTsc_ * (1.0 - static_cast<double>(nsErr.count() + nsErr.count() - baseNsErr_) /
                                  (static_cast<double>(tsc - baseTsc_) * nanosecondsPerTsc_));

  saveParam(tscI64, ns, nsErr.count(), newNsPerTsc);
}

SEN_ALWAYS_INLINE int64_t WallClock::readTimeStampCounterRegistry() noexcept
{
#ifdef _MSC_VER
  return __rdtsc();
#elif defined __i386 || defined _M_IX86
  uint32_t eax;
  uint32_t edx;
  asm volatile("rdtsc" : "=a"(eax), "=d"(edx));
  return (uint64_t(edx) << 32) + uint64_t(eax);
#elif defined __x86_64__ || defined _M_X64 || defined(__amd64__)
  uint64_t rax;
  uint64_t rdx;
  asm volatile("rdtsc" : "=a"(rax), "=d"(rdx));  // NOLINT(hicpp-no-assembler)
  return static_cast<int64_t>((rdx << 32U) + rax);
#elif defined __arm64
  int64_t tsc;
  asm volatile("mrs %0, cntvct_el0" : "=r"(tsc));
  return tsc;
#else
  return rdsysns();
#endif
}

SEN_ALWAYS_INLINE NanoSecs WallClock::timeStampCounterRegistryToNanoseconds(int64_t tsc) const noexcept
{
  return baseNs_ + NanoSecs(static_cast<NanoSecs::rep>(static_cast<double>(tsc - baseTsc_) * nanosecondsPerTsc_));
}

SEN_ALWAYS_INLINE NanoSecs WallClock::lowResNow() const noexcept
{
  return std::chrono::duration_cast<NanoSecs>(std::chrono::system_clock::now().time_since_epoch());
}

inline std::tuple<uint64_t, int64_t> WallClock::syncTime() noexcept
{
  if (!hardwareSupportsInvariantTSC())
  {
    return {0U, 0U};
  }

  // Linux kernel syncs time by finding the first trial with tsc diff < 50000.
  // We try several times and return the one with the min. tsc diff.
  // Note that MSVC has a 100ns resolution clock, so we need to combine those ns with the same
  // value, and drop the first and the last value as they may not scan a full 100ns range.

#ifdef _MSC_VER
  constexpr int n = 15;
#else
  constexpr int n = 3;
#endif
  int64_t tsc[n + 1];
  int64_t ns[n + 1];

  tsc[0] = readTimeStampCounterRegistry();
  for (std::size_t i = 1; i <= n; i++)
  {
    ns[i] = lowResNow().count();              // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    tsc[i] = readTimeStampCounterRegistry();  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
  }

#ifdef _MSC_VER
  std::size_t j = 1;
  for (std::size_t i = 2; i <= n; i++)
  {
    if (ns[i] == ns[i - 1])  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    {
      continue;
    }

    tsc[j - 1] = tsc[i - 1];  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    ns[j++] = ns[i];          // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
  }
  j--;
#else
  std::size_t j = n + 1;
#endif

  std::size_t best = 1;
  for (std::size_t i = 2; i < j; i++)
  {
    if (tsc[i] - tsc[i - 1] < tsc[best] - tsc[best - 1])  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    {
      best = i;
    }
  }
  return {(tsc[best] + tsc[best - 1U]) >> 1U, ns[best]};  // NOLINT
}

inline void WallClock::saveParam(int64_t baseTsc, int64_t sysNs, int64_t baseNsError, double newNsPerTsc) noexcept
{
  baseNsErr_ = baseNsError;
  nextCalibrateTsc_ =
    baseTsc + static_cast<int64_t>(static_cast<double>(calibrateIntervalNs_.count() - 1000) / newNsPerTsc);

  baseTsc_ = baseTsc;
  baseNs_ = NanoSecs(sysNs + baseNsError);
  nanosecondsPerTsc_ = newNsPerTsc;
}

}  // namespace sen::kernel

#endif  // SEN_LIBS_KERNEL_SRC_WALL_CLOCK_H
