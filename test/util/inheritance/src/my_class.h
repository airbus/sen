// === my_class.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_TEST_UTIL_INHERITANCE_SRC_MY_CLASS_H
#define SEN_TEST_UTIL_INHERITANCE_SRC_MY_CLASS_H

#include "sen/kernel/component_api.h"
#include "stl/inheritance/my_class.stl.h"

namespace inheritance
{

class MyClassImpl: public MyClassBase
{
public:
  SEN_NOCOPY_NOMOVE(MyClassImpl)

public:
  MyClassImpl(const std::string& name, const sen::VarMap& args);
  ~MyClassImpl() override = default;

public:
  void update(sen::kernel::RunApi& api) override;

protected:
  int32_t addNumbersImpl(int32_t a, int32_t b) override;
  std::string echoImpl(const std::string& message) override;
};

}  // namespace inheritance

#endif  // SEN_TEST_UTIL_INHERITANCE_SRC_MY_CLASS_H
