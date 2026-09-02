// === client.cpp ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// generated code
#include "stl/calculator.stl.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"

// std
#include <iostream>

namespace calculators
{

class ClientImpl: public ClientBase
{
public:
  SEN_NOCOPY_NOMOVE(ClientImpl)
  using ClientBase::ClientBase;
  ~ClientImpl() override = default;

public:
  // --8<-- [start:subscribe]
  void registered(sen::kernel::RegistrationApi& api) override  // (1)!
  {
    calculators_ = api.selectAllFrom<CalculatorInterface>(getCalcBus());  // (3)!
  }
  // --8<-- [end:subscribe]

protected:
  // --8<-- [start:async_call]
  void useCalculatorImpl() override
  {
    if (const auto& list = calculators_->list.getObjects(); !list.empty())  // (4)!
    {
      auto handleResult = [](sen::MethodResult<float32_t> result)  // (6)!
      {
        if (result.isOk())
        {
          std::cout << "add(3, 4) = " << std::to_string(result.getValue()) << std::endl;
        }
        else
        {
          std::cout << "add failed" << std::endl;
        }
      };

      // Call add(3, 4) and handle the result
      list.front()->add(3.0, 4.0, {this, handleResult});  // (5)!
    }
    // --8<-- [end:async_call]
  }

private:
  std::shared_ptr<sen::Subscription<CalculatorInterface>> calculators_;  // (2)!
};

SEN_EXPORT_CLASS(ClientImpl)

}  // namespace calculators
