// === crash_report.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_TEST_INTEGRATION_CRASH_REPORT_H
#define SEN_LIBS_KERNEL_TEST_INTEGRATION_CRASH_REPORT_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/var.h"

// generated code
#include "stl/crash_report.stl.h"

// std
#include <string>

namespace sen::test::crash_report
{

class CrashMakerImpl final: public CrashMakerBase
{
public:
  SEN_NOCOPY_NOMOVE(CrashMakerImpl)

  CrashMakerImpl(std::string name, const VarMap& args);
  ~CrashMakerImpl() override = default;
};

}  // namespace sen::test::crash_report

#endif  // SEN_LIBS_KERNEL_TEST_INTEGRATION_CRASH_REPORT_H
