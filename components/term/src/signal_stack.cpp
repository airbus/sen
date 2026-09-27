// === signal_stack.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "signal_stack.h"

#if !defined(_WIN32)
// std
#  include <array>

// other posix
// sigaction, SA_ONSTACK and SIGBUS are POSIX; <csignal> has neither the call nor the flag.
// NOLINTNEXTLINE(hicpp-deprecated-headers,modernize-deprecated-headers)
#  include <signal.h>
#endif

namespace sen::components::term
{

#if defined(_WIN32)

int restoreAltStackOnFatalHandlers() { return 0; }

#else

int restoreAltStackOnFatalHandlers()
{
  // The signals whose handler can run on an exhausted stack. SIGABRT and SIGFPE are here because the
  // same handler serves them and the flag costs nothing; SIGSEGV and SIGBUS are the ones that matter.
  static constexpr std::array<int, 5> fatalSignals {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};

  int changed = 0;
  for (const int signalNumber: fatalSignals)
  {
    struct sigaction current {};
    if (::sigaction(signalNumber, nullptr, &current) != 0)
    {
      continue;
    }
    // Through unsigned: sa_flags is a signed int on every platform Sen builds for, and a bitwise
    // operation on a signed operand is undefined once the sign bit is in play.
    const auto flags = static_cast<unsigned int>(current.sa_flags);
    constexpr auto onStack = static_cast<unsigned int>(SA_ONSTACK);
    if ((flags & onStack) != 0U)
    {
      continue;
    }

    // Only the flag changes: the handler stays whoever installed it, so FTXUI's terminal restore and
    // its own uninstall are untouched.
    current.sa_flags = static_cast<int>(flags | onStack);
    if (::sigaction(signalNumber, &current, nullptr) == 0)
    {
      ++changed;
    }
  }
  return changed;
}

#endif

}  // namespace sen::components::term
