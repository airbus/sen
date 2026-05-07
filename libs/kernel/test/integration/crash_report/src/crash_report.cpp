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
#include <memory>
#include <string>
#include <utility>

namespace sen::test::crash_report
{

CrashMakerImpl::CrashMakerImpl(std::string name, const VarMap& args): CrashMakerBase(std::move(name), args)
{
  std::thread(
    []
    {
      const std::weak_ptr<int> badWeak;
      std::shared_ptr shared(badWeak);
    })
    .detach();
}

SEN_EXPORT_CLASS(CrashMakerImpl)

}  // namespace sen::test::crash_report
