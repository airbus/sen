// === level0_class.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_TEST_UTIL_CMAKE_PACKAGES_LEVEL_SRC_LEVEL0_CLASS_H
#define SEN_TEST_UTIL_CMAKE_PACKAGES_LEVEL_SRC_LEVEL0_CLASS_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/var.h"

// stl
#include "stl/level0.stl.h"

// std
#include <string>

namespace level0
{

class SEN_EXPORT Level0ClassImpl: public Level0ClassBase
{
public:
  SEN_NOCOPY_NOMOVE(Level0ClassImpl)

public:
  Level0ClassImpl(const std::string& name, const sen::VarMap& args);
  ~Level0ClassImpl() override = default;

private:
  void printSomething0Impl() override;
};

}  // namespace level0

#endif  // SEN_TEST_UTIL_CMAKE_PACKAGES_LEVEL_SRC_LEVEL0_CLASS_H
