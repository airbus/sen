// === counter.h =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================
#pragma once

#include "stl/my_counter/counter.stl.h"  // (1)!

// sen
#include "sen/kernel/component_api.h"

namespace my_counter
{

class CounterImpl: public CounterBase  // (2)!
{
public:
  SEN_NOCOPY_NOMOVE(CounterImpl)  // (3)!

  using CounterBase::CounterBase;
  ~CounterImpl() override = default;

public:
  void update(sen::kernel::RunApi& runApi) override;  // (4)!

protected:
  std::string helloImpl() const override;  // (5)!
};

}  // namespace my_counter
