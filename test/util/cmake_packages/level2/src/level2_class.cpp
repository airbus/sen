// === level2_class.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "level2_class.h"

// sen
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"

// generated code
#include "stl/level2.stl.h"

// std
#include <iostream>
#include <string>

namespace level2
{

Level2ClassImpl::Level2ClassImpl(const std::string& name, const sen::VarMap& args): Level2ClassBase(name, args) {}

void Level2ClassImpl::printSomething0Impl() { std::cout << "printing something 0 from Level2ClassImpl\n"; }

void Level2ClassImpl::printSomething1Impl() { std::cout << "printing something 1 from Level2ClassImpl\n"; }

SEN_EXPORT_CLASS(Level2ClassImpl)

Level2With1ClassImpl::Level2With1ClassImpl(const std::string& name, const sen::VarMap& args)
  : Level2ClassBase(name, args)
{
}

SEN_EXPORT_CLASS(Level2With1ClassImpl)

}  // namespace level2
