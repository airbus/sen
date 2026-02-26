// === level2_class_s.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_TEST_UTIL_CMAKE_PACKAGES_LEVEL2_SRC_LEVEL2_CLASS_S_H
#define SEN_TEST_UTIL_CMAKE_PACKAGES_LEVEL2_SRC_LEVEL2_CLASS_S_H

#include "level1_class_s.h"
#include "stl/level2.stl.h"

namespace level2S
{

class Level2ClassImpl: public level2::Level2ClassBase<>
{
public:
  SEN_NOCOPY_NOMOVE(Level2ClassImpl)

public:
  Level2ClassImpl(const std::string& name, const sen::VarMap& args);
  ~Level2ClassImpl() override = default;

private:
  void printSomething0Impl() override;
  void printSomething1Impl() override;
};

class Level2With1ClassImpl: public level2::Level2ClassBase<level1S::Level1With0ClassImpl>
{
public:
  SEN_NOCOPY_NOMOVE(Level2With1ClassImpl)

public:
  Level2With1ClassImpl(const std::string& name, const sen::VarMap& args);
  ~Level2With1ClassImpl() override = default;
};

}  // namespace level2S

#endif  // SEN_TEST_UTIL_CMAKE_PACKAGES_LEVEL2_SRC_LEVEL2_CLASS_S_H
