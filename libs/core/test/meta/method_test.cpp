// === method_test.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/meta/callable.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/time_types.h"

// google test
#include <gtest/gtest.h>

// std
#include <exception>
#include <tuple>
#include <vector>

using sen::Arg;
using sen::CallableSpec;
using sen::Constness;
using sen::Method;
using sen::MethodSpec;

namespace
{

void checkInstanceSpec(const Method& instance, const MethodSpec& spec)  // NOLINT(readability-function-size)
{
  EXPECT_EQ(instance.getName(), spec.callableSpec.name);
  EXPECT_EQ(instance.getDescription(), spec.callableSpec.description);
  EXPECT_EQ(instance.getTransportMode(), spec.callableSpec.transportMode);

  std::vector<Arg> args(instance.getArgs().begin(), instance.getArgs().end());
  EXPECT_EQ(args, spec.callableSpec.args);
  EXPECT_EQ(*instance.getReturnType(), *spec.returnType);

  EXPECT_EQ(instance.getConstness(), spec.constness);
  EXPECT_EQ(instance.getDeferred(), spec.deferred);
  EXPECT_EQ(instance.getLocalOnly(), spec.localOnly);
  EXPECT_EQ(instance.getPropertyRelation().index(), spec.propertyRelation.index());
}

void checkInvalidSpec(const MethodSpec& spec) { EXPECT_THROW(std::ignore = Method::make(spec), std::exception); }

const CallableSpec& callableSpec()
{
  static const std::vector<Arg> args = {{"one", "first", sen::StringType::get()},
                                        {"two", "second", sen::Int64Type::get()},
                                        {"three", "third", sen::Int64Type::get()}};

  static const CallableSpec callableSpec = {"name", "description", args};

  return callableSpec;
}

}  // namespace

/// @test
/// Checks method spec comparison
/// @requirements(SEN-573)
TEST(Method, specComparison)
{
  // same
  {
    MethodSpec lhs = {callableSpec(), sen::Int16Type::get(), Constness::constant, sen::PropertyGetter {}, false, true};
    MethodSpec rhs = {callableSpec(), sen::Int16Type::get(), Constness::constant, sen::PropertyGetter {}, false, true};

    EXPECT_EQ(lhs, lhs);
    EXPECT_EQ(lhs, rhs);
  }

  // different constness
  {
    MethodSpec lhs = {callableSpec(), sen::Int16Type::get(), Constness::constant, sen::PropertyGetter {}, false, true};
    MethodSpec rhs = {
      callableSpec(), sen::Int16Type::get(), Constness::nonConstant, sen::PropertyGetter {}, false, true};

    EXPECT_NE(lhs, rhs);
  }

  // different types
  {
    MethodSpec lhs = {callableSpec(), sen::Int16Type::get(), Constness::constant, sen::PropertyGetter {}, false, true};
    MethodSpec rhs = {callableSpec(), sen::Int32Type::get(), Constness::constant, sen::PropertyGetter {}, false, true};

    EXPECT_NE(lhs, rhs);
  }

  // different property relation
  {
    MethodSpec lhs = {callableSpec(), sen::Int16Type::get(), Constness::constant, sen::PropertySetter {}, false, true};
    MethodSpec rhs = {callableSpec(), sen::Int32Type::get(), Constness::constant, sen::PropertyGetter {}, false, true};

    EXPECT_NE(lhs, rhs);
  }

  // different property relation 2
  {
    MethodSpec lhs = {
      callableSpec(), sen::Int16Type::get(), Constness::constant, sen::NonPropertyRelated {}, false, true};
    MethodSpec rhs = {callableSpec(), sen::Int32Type::get(), Constness::constant, sen::PropertySetter {}, false, true};

    EXPECT_NE(lhs, rhs);
  }

  // different property relation 3
  {
    MethodSpec lhs = {
      callableSpec(), sen::Int16Type::get(), Constness::constant, sen::NonPropertyRelated {}, false, true};
    MethodSpec rhs = {callableSpec(), sen::Int32Type::get(), Constness::constant, {}, false, true};

    EXPECT_NE(lhs, rhs);
  }

  // different local only
  {
    MethodSpec lhs = {
      callableSpec(), sen::Int16Type::get(), Constness::constant, sen::NonPropertyRelated {}, false, true};
    MethodSpec rhs = {
      callableSpec(), sen::Int32Type::get(), Constness::constant, sen::NonPropertyRelated {}, false, false};

    EXPECT_NE(lhs, rhs);
  }

  // no args
  {
    MethodSpec lhs = {callableSpec(), sen::Int16Type::get(), Constness::constant, sen::PropertyGetter {}, false, true};
    MethodSpec rhs = {callableSpec(), sen::Int16Type::get(), Constness::constant, sen::PropertyGetter {}, false, true};

    lhs.callableSpec.args = {};

    EXPECT_NE(lhs, rhs);
  }

  // different callable specs
  {
    MethodSpec lhs = {callableSpec(), sen::Int16Type::get(), Constness::constant, sen::PropertyGetter {}, false, true};
    MethodSpec rhs = {callableSpec(), sen::Int16Type::get(), Constness::constant, sen::PropertyGetter {}, false, true};

    rhs.callableSpec.name.append("X");

    EXPECT_NE(lhs, rhs);
  }

  // different deferred type
  {
    MethodSpec lhs = {callableSpec(), sen::VoidType::get(), Constness::constant, sen::PropertyGetter {}, false, true};
    MethodSpec rhs = {callableSpec(), sen::VoidType::get(), Constness::constant, sen::PropertyGetter {}, true, true};

    EXPECT_NE(lhs, rhs);
  }
}

/// @test
/// Checks correct method instance from spec
/// @requirements(SEN-573)
TEST(Method, instanceCheck)
{
  // constant no deferred
  {
    MethodSpec spec = {
      callableSpec(), sen::TimestampType::get(), Constness::constant, sen::PropertyGetter {}, false, true};
    auto instance = Method::make(spec);
    checkInstanceSpec(*instance, spec);
  }

  // constant deferred
  {
    MethodSpec spec = {callableSpec(), sen::DurationType::get(), Constness::constant, sen::PropertySetter {}, true, {}};
    auto instance = Method::make(spec);
    checkInstanceSpec(*instance, spec);
  }

  // no constant deferred
  {
    MethodSpec spec = {
      callableSpec(), sen::StringType::get(), Constness::nonConstant, sen::NonPropertyRelated {}, true, true};
    auto instance = Method::make(spec);
    checkInstanceSpec(*instance, spec);
  }

  // no constant no deferred
  {
    MethodSpec spec = {callableSpec(), sen::Int16Type::get(), Constness::nonConstant, {}, false, false};
    auto instance = Method::make(spec);
    checkInstanceSpec(*instance, spec);
  }

  // empty
  {
    MethodSpec spec = {{callableSpec()}, sen::VoidType::get(), {}, {}, {}, {}};
    auto instance = Method::make(spec);
    checkInstanceSpec(*instance, spec);
  }
}

/// @test
/// Checks that invalid method instance from spec throws an exception error
/// @requirements(SEN-573)
TEST(Method, invalidInstace)
{
  // full empty
  {
    MethodSpec spec = {{}, sen::VoidType::get(), {}, {}, {}, {}};
    checkInvalidSpec(spec);
  }

  // bad name
  {
    MethodSpec spec = {callableSpec(), sen::VoidType::get()};
    spec.callableSpec.name = "BadName";
    checkInvalidSpec(spec);
  }

  // bad name 2
  {
    MethodSpec spec = {callableSpec(), sen::VoidType::get()};
    spec.callableSpec.name = "$hello";
    checkInvalidSpec(spec);
  }

  // bad name 3
  {
    MethodSpec spec = {callableSpec(), sen::VoidType::get()};
    spec.callableSpec.name = "$*/-+#·";
    checkInvalidSpec(spec);
  }

  // bad name 4
  {
    MethodSpec spec = {callableSpec(), sen::VoidType::get()};
    spec.callableSpec.name = "";
    checkInvalidSpec(spec);
  }
}
