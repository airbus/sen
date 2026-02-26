// === my_class.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "my_class.h"

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/inheritance/basic_types.stl.h"
#include "stl/inheritance/my_class.stl.h"

// std
#include <cstdint>
#include <string>
#include <variant>

namespace inheritance
{

MyClassImpl::MyClassImpl(const std::string& name, const sen::VarMap& args): MyClassBase(name, args) {}

int32_t MyClassImpl::addNumbersImpl(int32_t a, int32_t b) { return a + b; }

std::string MyClassImpl::echoImpl(const std::string& message) { return message; }

void MyClassImpl::update(sen::kernel::RunApi& api)
{
  // change prop2
  {
    StructOfInts val = getProp2();
    val.field1 += 1;
    val.field2 += 2;
    val.field3 = val.field1 + val.field2;
    val.field4 = val.field3 + val.field2;
    val.field5 = val.field4 + val.field3;
    val.field6 = val.field4 + val.field5;
    val.field7 = val.field6 + val.field4;
    setNextProp2(val);
  }

  // change prop3
  {
    MyVariant val = getProp3();
    val = std::visit(
      sen::Overloaded {[&](const MyEnum& /*val*/) -> MyVariant
                       { return StructOfOthers {true, "some", sen::Duration {}, api.getTime()}; },
                       [](const StructOfOthers& /*val*/) -> MyVariant { return std::string {"some other string"}; },
                       [](const std::string& /*val*/) -> MyVariant { return MyEnum::third; }},
      val);
    setNextProp3(val);
  }

  // change prop4
  {
    Vec2 val = getProp4();
    val.x += 0.5f;
    val.y += 1.0;
    setNextProp4(val);
  }
}

SEN_EXPORT_CLASS(MyClassImpl)

}  // namespace inheritance
