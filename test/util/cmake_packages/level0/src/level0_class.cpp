// === level0_class.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "level0_class.h"

// sen
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"

// generated code
#include "stl/level0.stl.h"

// std
#include <iostream>
#include <string>

namespace level0
{

Level0ClassImpl::Level0ClassImpl(const std::string& name, const sen::VarMap& args): Level0ClassBase(name, args) {}

void Level0ClassImpl::printSomething0Impl() { std::cout << "printing something from Level0ClassImpl\n"; }

SEN_EXPORT_CLASS(Level0ClassImpl)

}  // namespace level0
