// === worker_impl.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef TEST_UTIL_TERM_SHOWCASE_SRC_WORKER_IMPL_H
#define TEST_UTIL_TERM_SHOWCASE_SRC_WORKER_IMPL_H

// sen
#include "sen/core/meta/var.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/term_showcase/term_showcase.stl.h"

// std
#include <cmath>
#include <string>

namespace term_showcase
{

class WorkerImpl: public WorkerBase
{
  SEN_NOCOPY_NOMOVE(WorkerImpl)

public:
  WorkerImpl(const std::string& name, const sen::VarMap& args): WorkerBase(name, args)
  {
    setNextValue(0.0F);
    setNextStatus(WorkerStatus::idle);
    setNextCycle(0U);
  }
  ~WorkerImpl() override = default;

  void update(sen::kernel::RunApi& /*runApi*/) override
  {
    auto c = getCycle() + 1U;
    setNextCycle(c);

    // Sine-ish oscillation so the value is visually interesting in the watch pane.
    setNextValue(std::sin(static_cast<float>(c) * 0.1F) * 50.0F);

    // Cycle status: idle → running → done → idle ...
    constexpr u32 phaseLength = 60U;
    auto phase = (c / phaseLength) % 3U;
    WorkerStatus newStatus = WorkerStatus::idle;
    switch (phase)
    {
      case 0U:
        newStatus = WorkerStatus::idle;
        break;
      case 1U:
        newStatus = WorkerStatus::running;
        break;
      case 2U:
        newStatus = WorkerStatus::done;
        break;
      default:
        break;
    }
    auto oldStatus = getStatus();
    if (newStatus != oldStatus)
    {
      statusChanged(oldStatus, newStatus);
    }
    setNextStatus(newStatus);

    // Heartbeat: every 30 cycles (~1 s). No arguments.
    if (c % 30U == 0U)
    {
      heartbeat();
    }

    // Position report: every 20 cycles. Struct argument.
    if (c % 20U == 0U)
    {
      auto angle = static_cast<float>(c) * 0.1F;
      positionReported(Point {static_cast<i32>(30.0F * std::cos(angle)), static_cast<i32>(30.0F * std::sin(angle))});
    }
  }
};

SEN_EXPORT_CLASS(WorkerImpl)

}  // namespace term_showcase

#endif  // TEST_UTIL_TERM_SHOWCASE_SRC_WORKER_IMPL_H
