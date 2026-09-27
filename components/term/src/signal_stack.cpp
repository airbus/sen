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
#  include <csignal>
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
    if ((current.sa_flags & SA_ONSTACK) != 0)
    {
      continue;
    }

    // Only the flag changes: the handler stays whoever installed it, so FTXUI's terminal restore and
    // its own uninstall are untouched.
    current.sa_flags |= SA_ONSTACK;
    if (::sigaction(signalNumber, &current, nullptr) == 0)
    {
      ++changed;
    }
  }
  return changed;
}

#endif

}  // namespace sen::components::term
