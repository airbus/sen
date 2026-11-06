// === counter.cpp =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================
#include "counter.h"

// sen
#include "sen/core/meta/class_type.h"
#include "sen/kernel/component_api.h"

// std
#include <string>

namespace my_counter
{

void CounterImpl::update(sen::kernel::RunApi& /*runApi*/)
{
  setNextValue(getValue() + getStep());  // (1)!

  if (getNextValue() % 10 == 0)
  {
    valueIsDivisibleByTen(getNextValue());  // (2)!
  }
}

std::string CounterImpl::helloImpl() const
{
  return "Hello from Sen! My current value is: " + std::to_string(getValue());
}

// (3)!
SEN_EXPORT_CLASS(CounterImpl)

}  // namespace my_counter
