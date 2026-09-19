// === crash_report.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "crash_report.h"

// generated code
#include "stl/crash_report.stl.h"

// sen
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"
#include "sen/kernel/component_api.h"

// std
#include <csignal>
#include <memory>
#include <string>
#include <utility>

namespace sen::test::crash_report
{

CrashMakerImpl::CrashMakerImpl(std::string name, const VarMap& args): CrashMakerBase(std::move(name), args)
{
  generateSignal_ = getGenerateSignal();
}

void CrashMakerImpl::update([[maybe_unused]] kernel::RunApi& runApi)
{
  if (!generateSignal_)
  {
    const std::weak_ptr<int> badWeak;
    std::shared_ptr shared(badWeak);
  }
  else
  {
    raise(SIGFPE);
  }
}

SEN_EXPORT_CLASS(CrashMakerImpl)

}  // namespace sen::test::crash_report
