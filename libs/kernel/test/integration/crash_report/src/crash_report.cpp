// === crash_report.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "crash_report.h"

// sen
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"
#include "stl/crash_report.stl.h"

// std
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace sen::test::crash_report
{

CrashMakerImpl::CrashMakerImpl(std::string name, const VarMap& args): CrashMakerBase(std::move(name), args)
{
  // Two ways to die, because the reporter handles them by different routes: an uncaught exception
  // reaches std::terminate, where allocating is legal, and a signal reaches a handler where it is
  // not. Only the first had a test.
  const auto* mode = std::getenv("SEN_CRASH_MODE");  // NOLINT(concurrency-mt-unsafe)
  const auto selected = mode != nullptr ? std::string_view(mode) : std::string_view();
  if (selected == "none")
  {
    return;  // a run that ends cleanly, so the crash context has to be cleaned up after it
  }

  const auto wantsSignal = selected == "signal";

  std::thread(
    [wantsSignal]
    {
      if (wantsSignal)
      {
        // After the kernel has reached "running", so the recorded phase is not also the value the
        // variable would hold if nothing ever set it.
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        std::ignore = std::raise(SIGSEGV);
        return;
      }

      const std::weak_ptr<int> badWeak;
      std::shared_ptr shared(badWeak);
    })
    .detach();
}

SEN_EXPORT_CLASS(CrashMakerImpl)

}  // namespace sen::test::crash_report
