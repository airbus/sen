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
    if (const auto countdown = getCountdown(); getState() == RunningState::on && countdown > 0)
    {
      const auto elapsedTime = runApi.getTargetCycleTime().value();
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
