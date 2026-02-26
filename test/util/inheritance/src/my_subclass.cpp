// === my_subclass.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "my_subclass.h"

#include "my_class.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/inheritance/basic_types.stl.h"
#include "stl/inheritance/my_subclass.stl.h"

// std
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>

namespace inheritance
{

MySubClassImpl::MySubClassImpl(const std::string& name, const sen::VarMap& args)
  : MySubClassBase<MyClassImpl>(name, args)
{
}

void MySubClassImpl::update(sen::kernel::RunApi& api)
{
  MyClassImpl::update(api);

  std::chrono::duration<double> time = api.getTime().sinceEpoch().toChrono();
  setNextADouble(sin(time.count()) * 100.0);

  switch (cycle_++ % 5)
  {
    case 0:
      setNextAVariant(MyEnum::first);
      setNextASequence({});
      break;
    case 1:
      setNextAVariant(MyEnum::second);
      setNextASequence({0.0f, 1.0f, 2.0f, 0.3f, 4.0f, 5.0f});
      break;
    case 2:
      setNextAVariant("some string");
      setNextASequence({5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.0f});
      break;
    case 3:
      setNextAVariant("some other string");
      setNextASequence({0.8f, 9.0f, 0.7f});
      break;
    case 4:
      setNextAFloat(getAFloat() + 7.43f);
      setNextASequence({0.8f, 9.0f, 0.0f, 1.8f, 2.0f, 3.0f});
      break;
  }
}

void MySubClassImpl::throwErrorImpl(const std::string& error)
{
  std::cout << getName() << ": throwing error '" << error << "'\n";
  sen::throwRuntimeError(error);
}

void MySubClassImpl::doSomethingImpl()
{
  std::cout << getName() << ": doing something and firing the somethingHappened() event\n";
  somethingHappened();
}

void MySubClassImpl::doSomethingElseImpl(int32_t arg)
{
  std::cout << getName() << ": doing something else and firing the somethingElseHappened(" << arg << ") event\n";
  somethingElseHappened(arg);
}

void MySubClassImpl::invertBoolImpl() { setNextABool(!getABool()); }

SEN_EXPORT_CLASS(MySubClassImpl)

}  // namespace inheritance
