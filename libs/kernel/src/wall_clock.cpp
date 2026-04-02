// === wall_clock.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "wall_clock.h"

// stl
#include "stl/sen/kernel/basic_types.stl.h"

// kernel
#include "kernel_impl.h"

// spdlog
#include <spdlog/logger.h>  // NOLINT(misc-include-cleaner)

// std
#include <cstdint>
#include <cstdlib>
#include <cstring>

// OS and compiler-specific
#ifdef _WIN32
#  include <intrin.h>
#elif defined(__linux__) && (defined(__x86_64__) || defined(__i386__))
#  include <cpuid.h>
#endif

namespace sen::kernel
{

#if defined __i386 || defined _M_IX86 || defined __x86_64__ || defined _M_X64

namespace
{

void cpuId(uint32_t* regs, uint32_t leaf)
{
  memset(regs, 0, sizeof(uint32_t) * 4);
#  if defined _MSC_VER
  __cpuidex((int*)regs, leaf, 0);
#  else
  __get_cpuid(leaf, regs, regs + 1, regs + 2, regs + 3);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
#  endif
}

void complainAboutLowResTimers()
{
  impl::KernelImpl::getKernelLogger()->warn("kernel: CPU does not support invariant TSC. Using default timers.");
}

bool checkHardwareSupportsInvariantTSC()
{
  if (std::getenv("SEN_AVOID_TSC") != nullptr)
  {
    impl::KernelImpl::getKernelLogger()->info("kernel: not using the time step counter registry.");
    return false;
  }

  uint32_t regs[4];
  cpuId(regs, 1);

  // check that we have the right bit enabled
  if (!(regs[3] & (1 << 4)))  // NOLINT
  {
    complainAboutLowResTimers();
    return false;
  }

  // The time stamp counter in newer processors may support an enhancement,
  // referred to as invariant TSC. Processor's support for invariant TSC is
  // indicated by CPUID.80000007H:EDX[8].
  //
  // This is equivalent to X86_FEATURE_CONSTANT_TSC + X86_FEATURE_NONSTOP_TSC.
  //
  // The invariant TSC will run at a constant rate in all ACPI P-, C-. and
  // T-states. On processors with invariant TSC support, the OS may use the
  // TSC for wall clock timer services (instead of ACPI or HPET timers).
  // TSC reads are much more efficient and do not incur the overhead
  // associated with a ring transition or access to a platform resources.
  cpuId(regs, 0x80000007);
  if (regs[3] & (1 << 8))  // NOLINT
  {
    return true;
  }

  complainAboutLowResTimers();
  return false;
}

}  // namespace

bool WallClock::hardwareSupportsInvariantTSC()
{
  // we just need to do the check once
  static bool cachedResult = checkHardwareSupportsInvariantTSC();
  return cachedResult;
}

SleepPolicy WallClock::getSleepPolicy() noexcept { return sleepPolicy_; }

#elif defined(__APPLE__) || defined(__aarch64__)

bool WallClock::hardwareSupportsInvariantTSC() { return false; }

SleepPolicy WallClock::getSleepPolicy() noexcept { return SystemSleep {}; }

#else
#  error "Architecture not supported"
#endif

}  // namespace sen::kernel
