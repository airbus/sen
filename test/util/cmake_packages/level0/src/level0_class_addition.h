// === level0_class_addition.h =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_TEST_UTIL_CMAKE_PACKAGES_LEVEL0_SRC_LEVEL0_CLASS_ADDITION_H
#define SEN_TEST_UTIL_CMAKE_PACKAGES_LEVEL0_SRC_LEVEL0_CLASS_ADDITION_H

#include "sen/kernel/component_api.h"
#include "stl/level0.stl.h"

namespace level0_addition
{

class SEN_EXPORT Level0ClassAdditionImpl: public level0::Level0ClassBase
{
public:
  SEN_NOCOPY_NOMOVE(Level0ClassAdditionImpl)

public:
  Level0ClassAdditionImpl(const std::string& name, const sen::VarMap& args);
  ~Level0ClassAdditionImpl() override = default;

private:
  void printSomething0Impl() override;
};

}  // namespace level0_addition

#endif  // SEN_TEST_UTIL_CMAKE_PACKAGES_LEVEL0_SRC_LEVEL0_CLASS_ADDITION_H
