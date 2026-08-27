// === test_object.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/meta/class_type.h"

// generated code
#include "stl/py_test_package.stl.h"

namespace py_test_package
{

class TestObject: public TestObjectBase
{
public:
  SEN_NOCOPY_NOMOVE(TestObject)

public:
  using TestObjectBase::TestObjectBase;
  ~TestObject() override = default;

protected:
  float32_t divideImpl(float32_t a, float32_t b) override
  {
    float32_t result = 0.0f;
    if (b == 0.0f)
    {
      divisionByZero();
    }
    else
    {
      result = a / b;
    }

    return result;
  }
};

SEN_EXPORT_CLASS(TestObject)

}  // namespace py_test_package
