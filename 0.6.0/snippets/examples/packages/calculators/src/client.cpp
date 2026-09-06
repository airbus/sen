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

namespace calculators
{

class ClientImpl: public ClientBase
{
public:
  SEN_NOCOPY_NOMOVE(ClientImpl)
  using ClientBase::ClientBase;
  ~ClientImpl() override = default;

public:
  void registered(sen::kernel::RegistrationApi& api) override
  {
    calculators_ = api.selectAllFrom<CalculatorInterface>(getCalcBus());
  }

protected:
  void useCalculatorImpl() override
  {
    if (const auto& list = calculators_->list.getObjects(); !list.empty())
    {
      auto handleResult = [](sen::MethodResult<float64_t> result)
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
      list.front()->add(3.0, 4.0, {this, handleResult});
    }
  }

private:
  std::shared_ptr<sen::Subscription<CalculatorInterface>> calculators_;
};

SEN_EXPORT_CLASS(ClientImpl)

}  // namespace calculators
