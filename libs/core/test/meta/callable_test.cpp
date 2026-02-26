// === callable_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type.h"

// google test
#include <gtest/gtest.h>

// std
#include <exception>
#include <memory>
#include <utility>
#include <vector>

using sen::Arg;
using sen::Callable;
using sen::CallableSpec;

namespace
{

class MyCallable: public Callable
{
  SEN_NOCOPY_NOMOVE(MyCallable)

public:
  using Callable::operator==;
  using Callable::operator!=;

  explicit MyCallable(CallableSpec spec): Callable(std::move(spec)) {}
  ~MyCallable() override = default;
};

void checkInstanceSpec(const Callable& instance, const CallableSpec& spec)
{
  EXPECT_EQ(instance.getName(), spec.name);
  EXPECT_EQ(instance.getDescription(), spec.description);
  EXPECT_EQ(instance.getTransportMode(), spec.transportMode);

  std::vector<Arg> args(instance.getArgs().begin(), instance.getArgs().end());
  EXPECT_EQ(args, spec.args);
}

const std::vector<Arg>& args()
{
  static const std::vector<Arg> args = {{"one", "first", sen::StringType::get()},
                                        {"two", "second", sen::Int64Type::get()},
                                        {"three", "third", sen::Int64Type::get()}};
  return args;
}

}  // namespace

/// @test
/// Check callable arguments comparison
/// @requirements(SEN-355)
TEST(Callable, argComparison)
{
  // same
  {
    Arg lhs {"one", "first", sen::StringType::get()};
    Arg rhs {"one", "first", sen::StringType::get()};

    EXPECT_EQ(lhs, lhs);
    EXPECT_EQ(lhs, rhs);
  }

  // no name
  {
    Arg lhs {"", "first", sen::StringType::get()};
    Arg rhs {"", "first", sen::StringType::get()};

    EXPECT_EQ(lhs, lhs);
    EXPECT_EQ(lhs, rhs);
  }

  // same no description
  {
    Arg lhs {"one", "", sen::StringType::get()};
    Arg rhs {"one", "", sen::StringType::get()};

    EXPECT_EQ(lhs, lhs);
    EXPECT_EQ(lhs, rhs);
  }

  // same no name no description
  {
    Arg lhs {"", "", sen::StringType::get()};
    Arg rhs {"", "", sen::StringType::get()};

    EXPECT_EQ(lhs, lhs);
    EXPECT_EQ(lhs, rhs);
  }

  // different names
  {
    Arg lhs {"one", "first", sen::StringType::get()};
    Arg rhs {"oneX", "first", sen::StringType::get()};

    EXPECT_NE(lhs, rhs);
  }

  // different description
  {

    Arg lhs {"one", "first", sen::StringType::get()};
    Arg rhs {"one", "firstX", sen::StringType::get()};

    EXPECT_NE(lhs, rhs);
  }

  // different types
  {
    Arg lhs {"one", "first", sen::StringType::get()};
    Arg rhs {"one", "first", sen::Int64Type::get()};

    EXPECT_NE(lhs, rhs);
  }
}

/// @test
/// Checks callable spec comparison
/// @requirements(SEN-355)
TEST(Callable, specComparison)
{
  // same
  {
    CallableSpec lhs = {"name", "description", args()};
    CallableSpec rhs = {"name", "description", args()};

    EXPECT_EQ(lhs, lhs);
    EXPECT_EQ(lhs, rhs);
  }

  // different transport
  {
    CallableSpec lhs = {"name", "description", args(), sen::TransportMode::multicast};
    CallableSpec rhs = {"name", "description", args(), sen::TransportMode::confirmed};

    EXPECT_NE(lhs, rhs);
  }

  // different name
  {
    CallableSpec lhs = {"name", "description", args()};
    CallableSpec rhs = {"nameX", "description", args()};

    EXPECT_NE(lhs, rhs);
  }

  // different name 2
  {
    CallableSpec lhs = {"name", "description", args()};
    CallableSpec rhs = {{}, "description", args()};

    EXPECT_NE(lhs, rhs);
  }

  // different description
  {
    CallableSpec lhs = {"name", "description", args()};
    CallableSpec rhs = {"name", "descriptionX", args()};

    EXPECT_NE(lhs, rhs);
  }

  // different description 2
  {
    CallableSpec lhs = {"name", "description", args()};
    CallableSpec rhs = {"name", "", args()};

    EXPECT_NE(lhs, rhs);
  }

  // different hash
  {
    auto lhsArgs = args();
    lhsArgs.emplace_back("anotherArg", "", sen::TimestampType::get());

    auto rhsArgs = args();
    rhsArgs.emplace_back("anotherArg", "", sen::TimestampType::get());
    CallableSpec lhs = {"name", "description", lhsArgs};
    CallableSpec rhs = {"name", "", rhsArgs};

    EXPECT_NE(lhs, rhs);
  }

  // no args()
  {
    CallableSpec lhs = {"name", "description", args()};
    CallableSpec rhs = {"name", "description", {}};

    EXPECT_NE(lhs, rhs);
  }

  // optional arg vs mandatory arg
  {
    auto optionalArg = sen::OptionalType::make({"Arg", "Arg", "opt", sen::StringType::get()});
    // TODO: fix
    CallableSpec lhs = {"name", "description", {{"one", "first", optionalArg}}};
    CallableSpec rhs = {"name", "description", {{"one", "first", sen::StringType::get()}}};

    EXPECT_NE(lhs, rhs);
  }

  // more args()
  {
    auto moreArgs = args();
    moreArgs.emplace_back("four", "fourth", sen::Int64Type::get());

    CallableSpec lhs = {"name", "description", args()};
    CallableSpec rhs = {"name", "description", moreArgs};

    EXPECT_NE(lhs, rhs);
  }

  // less args()
  {
    auto lessArgs = args();
    lessArgs.pop_back();

    CallableSpec lhs = {"name", "description", args()};
    CallableSpec rhs = {"name", "description", lessArgs};

    EXPECT_NE(lhs, rhs);
  }
}

/// @test
/// Checks callable spec correct creation
/// @requirements(SEN-355)
TEST(Callable, specValidation)
{
  // valid
  {
    CallableSpec spec = {"name", "description", args()};
    EXPECT_NO_THROW(Callable::checkSpec(spec));
  }

  // no name
  {
    CallableSpec spec = {{}, "description", args()};
    EXPECT_THROW(Callable::checkSpec(spec), std::exception);
  }

  // invalid name
  {
    CallableSpec spec = {"InvalidName", "description", args()};
    EXPECT_THROW(Callable::checkSpec(spec), std::exception);
  }

  // empty descriptions are ok
  {
    CallableSpec spec = {"name", {}, args()};
    EXPECT_NO_THROW(Callable::checkSpec(spec));
  }

  // repeated argument name
  {
    auto invalidArgs = args();
    invalidArgs.emplace_back("three", "fourth", sen::Int64Type::get());

    CallableSpec spec = {"name", "description", invalidArgs};
    EXPECT_THROW(Callable::checkSpec(spec), std::exception);
  }

  // no name args()
  {
    auto invalidArgs = args();
    invalidArgs.emplace_back("", "", sen::Int64Type::get());

    CallableSpec spec = {"name", "description", invalidArgs};
    EXPECT_NO_THROW(Callable::checkSpec(spec));
  }

  // empty spec
  {
    CallableSpec spec = {};
    EXPECT_THROW(Callable::checkSpec(spec), std::exception);
  }
}

/// @test
/// Checks correct callable instance from specs
/// @requirements(SEN-355)
TEST(Callable, instanceCheck)
{
  // spec check (valid)
  {
    CallableSpec spec = {"name", "description", args()};
    auto instance = std::make_shared<MyCallable>(spec);
    checkInstanceSpec(*instance, spec);
  }

  // spec check (empty args())
  {
    CallableSpec spec = {"name", "description", {}};
    auto instance = std::make_shared<MyCallable>(spec);
    checkInstanceSpec(*instance, spec);
  }

  // spec check (repeated empty args() name)
  {
    auto argList = args();
    argList.emplace_back("", "", sen::UInt8Type::get());
    argList.emplace_back("", "", sen::UInt8Type::get());

    CallableSpec spec = {"name", "description", argList};
    auto instance = std::make_shared<MyCallable>(spec);
    checkInstanceSpec(*instance, spec);
  }

  // spec check (repeated args())
  {
    auto argList = args();
    argList.emplace_back("abc", "", sen::UInt8Type::get());
    argList.emplace_back("abc", "", sen::UInt8Type::get());

    CallableSpec spec = {"name", "description", argList};
    auto instance = std::make_shared<MyCallable>(spec);
    checkInstanceSpec(*instance, spec);
  }

  // same
  {
    CallableSpec lhs = {"name", "description", args()};
    CallableSpec rhs = {"name", "description", args()};

    auto instanceLhs = std::make_shared<MyCallable>(lhs);
    auto instanceRhs = std::make_shared<MyCallable>(rhs);

    EXPECT_EQ(*instanceLhs, *instanceLhs);
    EXPECT_EQ(*instanceLhs, *instanceRhs);
  }

  // different name
  {
    CallableSpec lhs = {"name", "description", args()};
    CallableSpec rhs = {"nameX", "description", args()};

    auto instanceLhs = std::make_shared<MyCallable>(lhs);
    auto instanceRhs = std::make_shared<MyCallable>(rhs);

    EXPECT_NE(*instanceLhs, *instanceRhs);
  }

  // different description
  {
    CallableSpec lhs = {"name", "description", args()};
    CallableSpec rhs = {"name", "descriptionX", args()};

    auto instanceLhs = std::make_shared<MyCallable>(lhs);
    auto instanceRhs = std::make_shared<MyCallable>(rhs);

    EXPECT_NE(*instanceLhs, *instanceRhs);
  }

  // no args()
  {
    CallableSpec lhs = {"name", "description", args()};
    CallableSpec rhs = {"name", "description", {}};

    auto instanceLhs = std::make_shared<MyCallable>(lhs);
    auto instanceRhs = std::make_shared<MyCallable>(rhs);

    EXPECT_NE(*instanceLhs, *instanceRhs);
  }

  // more args()
  {
    auto moreArgs = args();
    moreArgs.emplace_back("four", "fourth", sen::Int64Type::get());

    CallableSpec lhs = {"name", "description", args()};
    CallableSpec rhs = {"name", "description", moreArgs};

    auto instanceLhs = std::make_shared<MyCallable>(lhs);
    auto instanceRhs = std::make_shared<MyCallable>(rhs);

    EXPECT_NE(*instanceLhs, *instanceRhs);
  }

  // less args()
  {
    auto lessArgs = args();
    lessArgs.pop_back();

    CallableSpec lhs = {"name", "description", args()};
    CallableSpec rhs = {"name", "description", lessArgs};

    auto instanceLhs = std::make_shared<MyCallable>(lhs);
    auto instanceRhs = std::make_shared<MyCallable>(rhs);

    EXPECT_NE(*instanceLhs, *instanceRhs);
  }
}

/// @test
/// Checks callable arguments getter
/// @requirements(SEN-355)
TEST(Callable, getArgFrom)
{
  CallableSpec spec = {"name", "description", args()};
  auto instance = std::make_shared<MyCallable>(spec);

  // from name, valid
  {
    auto arg = instance->getArgFromName(args().at(0U).name);
    EXPECT_EQ(arg->name, args().at(0U).name);
  }

  // from name, valid
  {
    auto arg = instance->getArgFromName(args().at(1U).name);
    EXPECT_EQ(arg->name, args().at(1U).name);
  }

  // from name, invalid
  {
    auto arg = instance->getArgFromName("invalid");
    EXPECT_EQ(arg, nullptr);
  }

  // from name, invalid
  {
    auto arg = instance->getArgFromName({});
    EXPECT_EQ(arg, nullptr);
  }
}
