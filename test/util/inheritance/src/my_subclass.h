// === my_subclass.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_TEST_UTIL_INHERITANCE_SRC_MY_SUBCLASS_H
#define SEN_TEST_UTIL_INHERITANCE_SRC_MY_SUBCLASS_H

#include "my_class.h"
#include "sen/kernel/component_api.h"
#include "stl/inheritance/my_subclass.stl.h"

namespace inheritance
{

class MySubClassImpl: public MySubClassBase<MyClassImpl>
{
public:
  SEN_NOCOPY_NOMOVE(MySubClassImpl)

public:
  MySubClassImpl(const std::string& name, const sen::VarMap& args);
  ~MySubClassImpl() override = default;

public:
  void update(sen::kernel::RunApi& api) override;

protected:
  void throwErrorImpl(const std::string& error) override;
  void doSomethingImpl() override;
  void doSomethingElseImpl(int32_t arg) override;
  void invertBoolImpl() override;

private:
  std::size_t cycle_ = 0U;
};

}  // namespace inheritance

#endif  // SEN_TEST_UTIL_INHERITANCE_SRC_MY_SUBCLASS_H
