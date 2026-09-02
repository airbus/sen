// === timer.cpp =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// generated code
#include "stl/timer.stl.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/class_type.h"
#include "sen/kernel/component_api.h"

// std
#include <iostream>
#include <optional>

namespace timer
{

/// Implementation of the Timer class.
/// This timer countdowns from an input value only if it is On, using the checked Properties approach.
class TimerImpl: public TimerBase
{
public:
  SEN_NOCOPY_NOMOVE(TimerImpl)

public:
  using TimerBase::TimerBase;
  ~TimerImpl() override = default;

public:
  void update(sen::kernel::RunApi& runApi) override
  {
    // --8<-- [start:elapsed]
    // Elapsed time comes from the clock, not from the configured period: a skipped cycle covers two
    // periods, and a component that is stepped rather than cycled has no period at all.
    // The first cycle has nothing to difference against, so it contributes nothing rather than the
    // whole interval since the kernel started.
    const auto now = runApi.getTime();
    const auto elapsedTime = now - lastUpdate_.value_or(now);

    // Advanced on every cycle, including while the timer is off, so switching it on does not
    // subtract the whole interval it spent paused.
    lastUpdate_ = now;
    // --8<-- [end:elapsed]

    if (const auto countdown = getCountdown(); getState() == RunningState::on && countdown > 0)
    {
      const auto newTimerValue = elapsedTime > countdown ? 0 : countdown - elapsedTime;
      setNextCountdown(newTimerValue);

      if (newTimerValue == 0)
      {
        timeout();
        setNextState(RunningState::off);
        std::cout << "Timeout! Timer off." << std::endl;
      }
    }
  }

protected:
  void registered(sen::kernel::RegistrationApi& /*api*/) override
  {
    onProgramChanged({this, [this]() { setNextCountdown(getProgram()); }}).keep();
  }

private:
  /// Empty until the first update, which has nothing to difference against yet.
  std::optional<sen::TimeStamp> lastUpdate_;

  bool programAcceptsSet(sen::Duration /*val*/) const override
  {
    if (getState() == RunningState::off)
    {
      std::cout << "Timer is off! Switch it on to start running." << std::endl;
      return true;
    }

    std::cout << "Timer is running! cannot change the program." << std::endl;
    return false;
  }
};

SEN_EXPORT_CLASS(TimerImpl)

}  // namespace timer
