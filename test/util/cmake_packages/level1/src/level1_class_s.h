// === level1_class_s.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_TEST_UTIL_CMAKE_PACKAGES_LEVEL1_SRC_LEVEL1_CLASS_S_H
#define SEN_TEST_UTIL_CMAKE_PACKAGES_LEVEL1_SRC_LEVEL1_CLASS_S_H

#include "level0_class.h"
#include "stl/level1.stl.h"

namespace level1S
{

class Level1ClassImpl: public level1::Level1ClassBase<>
{
public:
  SEN_NOCOPY_NOMOVE(Level1ClassImpl)

public:
  Level1ClassImpl(const std::string& name, const sen::VarMap& args);
  ~Level1ClassImpl() override = default;

private:
  void printSomething0Impl() override;
  void printSomething1Impl() override;
};

class SEN_EXPORT Level1With0ClassImpl: public level1::Level1ClassBase<level0::Level0ClassImpl>
{
public:
  SEN_NOCOPY_NOMOVE(Level1With0ClassImpl)

public:
  Level1With0ClassImpl(const std::string& name, const sen::VarMap& args);
  ~Level1With0ClassImpl() override = default;

private:
  void printSomething1Impl() override;
};

}  // namespace level1S

#endif  // SEN_TEST_UTIL_CMAKE_PACKAGES_LEVEL1_SRC_LEVEL1_CLASS_S_H
