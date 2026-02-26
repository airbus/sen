// === my_reactionclass.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_TEST_UTIL_INHERITANCE_SRC_MY_REACTIONCLASS_H
#define SEN_TEST_UTIL_INHERITANCE_SRC_MY_REACTIONCLASS_H

// sen
#include "my_subclass.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"
#include "stl/inheritance/my_reactionclass.stl.h"

namespace inheritance
{

// just to illustrate how to subscribe and react to events and property changes of other objects
class MyReactionClassImpl: public MyReactionClassBase
{
public:
  SEN_NOCOPY_NOMOVE(MyReactionClassImpl)

public:
  using MyReactionClassBase::MyReactionClassBase;

  ~MyReactionClassImpl() override = default;

public:  // Implements sen::NativeObject
  void registered(sen::kernel::RegistrationApi& api) override;
  void unregistered(sen::kernel::RegistrationApi& api) override;

private:
  std::shared_ptr<sen::Subscription<MySubClassImpl>> sub_ {nullptr};
};

}  // namespace inheritance

#endif  // SEN_TEST_UTIL_INHERITANCE_SRC_MY_REACTIONCLASS_H
