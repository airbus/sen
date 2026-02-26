// === level1_class.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "level1_class.h"

// sen
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"

// generated code
#include "stl/level1.stl.h"

// std
#include <iostream>
#include <string>

namespace level1
{

Level1ClassImpl::Level1ClassImpl(const std::string& name, const sen::VarMap& args): Level1ClassBase(name, args) {}

void Level1ClassImpl::printSomething0Impl() { std::cout << "printing something 0 from Level1ClassImpl\n"; }

void Level1ClassImpl::printSomething1Impl() { std::cout << "printing something 1 from Level1ClassImpl\n"; }

SEN_EXPORT_CLASS(Level1ClassImpl)

Level1With0ClassImpl::Level1With0ClassImpl(const std::string& name, const sen::VarMap& args)
  : Level1ClassBase(name, args)
{
}

void Level1With0ClassImpl::printSomething1Impl() { std::cout << "printing something 1 from Level1With0ClassImpl\n"; }

SEN_EXPORT_CLASS(Level1With0ClassImpl)

}  // namespace level1
