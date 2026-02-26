// === move_only_function_test.cpp =====================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/move_only_function.h"

// google test
#include <gtest/gtest.h>

// std
#include <type_traits>
#include <utility>

using sen::std_util::move_only_function;

/// @test
/// Check requirements (move-constructable, move-assignable, copy-contructable, copy-assignable) for move only functions
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, ConstAssignRequirements)
{
  static_assert(std::is_move_constructible_v<move_only_function<int(int)>>, "MOF should be move constructable");
  static_assert(std::is_move_assignable_v<move_only_function<int(int)>>, "MOF should be move assignable");
  static_assert(!std::is_copy_constructible_v<move_only_function<int(int)>>, "MOF should not be copy constructable");
  static_assert(!std::is_copy_assignable_v<move_only_function<int(int)>>, "MOF should not be copy assignable");
}

/// @test
/// Check move only function with an empty lambda
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, LambdaEmpty)
{
  move_only_function<void()> f = []() {};
  f();
}

/// @test
/// Check move only function with a lambda with a parameter
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, LambdaWithParameter)
{
  move_only_function<void(int)> f = [](int i) {};
  f(42);
}

/// @test
/// Check move only function with a lambda with various parameters
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, LambdaWithParameters)
{
  move_only_function<void(int, char, bool)> f = [](int i, char c, bool b) {};
  f(42, 'f', false);
}

/// @test
/// Check move only function with a lambda returning a value
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, LambdaWithReturn)
{
  move_only_function<int()> f = []() { return 42; };
  int ret = f();
  EXPECT_EQ(ret, 42);
}

/// @test
/// Check move only function with a lambda calculating and returning a value
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, LambdaCalculation)
{
  move_only_function<int(int, int)> f = [](int a, int b) { return a + b; };
  int ret = f(20, 22);
  EXPECT_EQ(ret, 42);
}

/// @test
/// Check move only function with a lambda capturing a value
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, LambdaWithCapture)
{
  int capturedValue = 42;
  move_only_function<int()> f = [capturedValue]() { return capturedValue; };
  int ret = f();
  EXPECT_EQ(ret, 42);
}

/// @test
/// Check move only function with a lambda capturing various values
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, LambdaWithManyCaptures)
{
  // Captures many things by value to have a callable object thats larger than the small size optimization
  int capturedValue1 = 5;
  int capturedValue2 = 5;
  int capturedValue3 = 5;
  int capturedValue4 = 5;
  int capturedValue5 = 5;
  int capturedValue6 = 5;
  move_only_function<int(int)> f =
    [capturedValue1, capturedValue2, capturedValue3, capturedValue4, capturedValue5, capturedValue6](int input)
  {
    return capturedValue1 + capturedValue2 + capturedValue3 + capturedValue4 + capturedValue5 + capturedValue6 + input;
  };
  int ret = f(12);
  EXPECT_EQ(ret, 42);
}

/// @test
/// Check move only function with an empty construction
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, CheckEmptyConstruction)
{
  move_only_function<int()> outerF;
  // int ret  = outerF(); // not allowed -> UB
}

/// @test
/// Check move only function with a lambda capture moved out of scope
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, MoveOutOfScope)
{
  move_only_function<int()> outerF;

  {
    int state = 42;
    move_only_function<int()> innerF = [state]() { return state; };

    outerF = std::move(innerF);
  }

  int ret = outerF();
  EXPECT_EQ(ret, 42);
}

void fooEmpty() {}
/// @test
/// Check move only function with an empty function
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, FreeFunctionEmpty)
{
  move_only_function<void()> f = &fooEmpty;
  f();
}

void fooParam(int i) {}
/// @test
/// Check move only function with a function with a parameter
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, FreeFunctionWithParameter)
{
  move_only_function<decltype(fooParam)> f = &fooParam;
  f(42);
}

void fooParams(int i, char c, bool b) {}
/// @test
/// Check move only function with a function with various parameters
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, FreeFunctionWithParameters)
{
  move_only_function<decltype(fooParams)> f = &fooParams;
  f(42, 'f', false);
}

int fooReturn() { return 42; }
/// @test
/// Check move only function with a function with return
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, FreeFunctionWithReturn)
{
  move_only_function<decltype(fooReturn)> f = &fooReturn;
  int ret = f();
  EXPECT_EQ(ret, 42);
}

int fooCalculation(int a, int b) { return a + b; }
/// @test
/// Check move only function with a function with parameters and return
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, FreeFunctionWithCalculation)
{
  move_only_function<decltype(fooCalculation)> f = &fooCalculation;
  int ret = f(20, 22);
  EXPECT_EQ(ret, 42);
}

struct Foo
{
  void empty() {};
  void param(int i) {}
  void params(int i, char c, bool b) {}
  int ret() { return 42; }
  int calculation(int a, int b) { return a + b; }
};

/// @test
/// Check move only function with an empty class member function
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, MemberFunctionEmpty)
{
  Foo foo;
  move_only_function<void(Foo*)> f = &Foo::empty;
  f(&foo);
}

/// @test
/// Check move only function with a class member function with a parameter
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, MemberFunctionWithParameter)
{
  Foo foo;
  move_only_function<void(Foo*, int)> f = &Foo::param;
  f(&foo, 42);
}

/// @test
/// Check move only function with a class member function with various parameters
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, MemberFunctionWithParameters)
{
  Foo foo;
  move_only_function<void(Foo*, int, char, bool)> f = &Foo::params;
  f(&foo, 42, 'f', false);
}

/// @test
/// Check move only function with a class member function with return
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, MemberFunctionWithReturn)
{
  Foo foo;
  move_only_function<int(Foo*)> f = &Foo::ret;
  int ret = f(&foo);
  EXPECT_EQ(ret, 42);
}

/// @test
/// Check move only function with a class member function with calculation and return
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, MemberFunctionWithCalculation)
{
  Foo foo;
  move_only_function<int(Foo*, int, int)> f = &Foo::calculation;
  int ret = f(&foo, 20, 22);
  EXPECT_EQ(ret, 42);
}
