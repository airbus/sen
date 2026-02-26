// === level0_class_addition.cpp =======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "level0_class_addition.h"

// sen
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"

// generated code
#include "stl/level0.stl.h"

// std
#include <iostream>
#include <string>

namespace level0_addition
{

Level0ClassAdditionImpl::Level0ClassAdditionImpl(const std::string& name, const sen::VarMap& args)
  : level0::Level0ClassBase(name, args)
{
}

void Level0ClassAdditionImpl::printSomething0Impl()
{
  std::cout << "printing something from Level0ClassAdditionImpl\n";
}

SEN_EXPORT_CLASS(Level0ClassAdditionImpl)

}  // namespace level0_addition
