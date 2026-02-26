// === my_reactionclass.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "my_reactionclass.h"

// example
#include "my_subclass.h"

// sen
#include "sen/core/meta/class_type.h"
#include "sen/kernel/component_api.h"

// std
#include <cstdint>
#include <iostream>
#include <tuple>

namespace inheritance
{

void MyReactionClassImpl::registered(sen::kernel::RegistrationApi& api)
{
  sub_ = api.selectAllFrom<MySubClassImpl>("local.tutorial");

  std::ignore = sub_->list.onAdded(
    [this](const auto& iterators)
    {
      for (auto it = iterators.typedBegin; it != iterators.typedEnd; ++it)
      {
        auto us = shared_from_this();
        auto other = *it;

        auto somethingHandler = [us]()
        { std::cout << us->getName() << ": Reacting to event \"SomethingHappened\"" << std::endl; };

        auto somethingElseHandler = [us](int32_t arg)
        { std::cout << us->getName() << ": Reacting to event \"SomethingElseHappened\"" << arg << std::endl; };

        auto propertyHandler = [us, other]()
        {
          std::cout << us->getName() << ": Reacting to change of variable \"aBool\": " << other->getABool()
                    << std::endl;
        };

        other->onSomethingHappened({this, std::move(somethingHandler)}).keep();
        other->onSomethingElseHappened({this, std::move(somethingElseHandler)}).keep();
        other->onABoolChanged({this, std::move(propertyHandler)}).keep();
      };
    });
}

void MyReactionClassImpl::unregistered(sen::kernel::RegistrationApi& api)
{
  std::ignore = api;
  sub_.reset();
}

SEN_EXPORT_CLASS(MyReactionClassImpl)

}  // namespace inheritance
