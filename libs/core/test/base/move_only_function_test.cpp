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
/// Is move constructible and move assignable but neither copy constructible nor copy assignable, asserted at
/// compile time.
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, ConstAssignRequirements)
{
  static_assert(std::is_move_constructible_v<move_only_function<int(int)>>, "MOF should be move constructable");
  static_assert(std::is_move_assignable_v<move_only_function<int(int)>>, "MOF should be move assignable");
  static_assert(!std::is_copy_constructible_v<move_only_function<int(int)>>, "MOF should not be copy constructable");
  static_assert(!std::is_copy_assignable_v<move_only_function<int(int)>>, "MOF should not be copy assignable");
}

/// @test
/// Wraps a captureless void lambda and completes an invocation of it.
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, LambdaEmpty)
{
  move_only_function<void()> f = []() {};
  f();
}

/// @test
/// Wraps a one-parameter lambda and forwards the int argument on invocation.
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, LambdaWithParameter)
{
  move_only_function<void(int)> f = [](int /* i */) {};
  f(42);
}

/// @test
/// Wraps a three-parameter lambda and forwards int, char, and bool arguments on invocation.
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, LambdaWithParameters)
{
  move_only_function<void(int, char, bool)> f = [](int /* i */, char /* c */, bool /* b */) {};
  f(42, 'f', false);
}

/// @test
/// Returns the wrapped lambda's value, an invocation yielding 42.
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, LambdaWithReturn)
{
  move_only_function<int()> f = []() { return 42; };
  int ret = f();
  EXPECT_EQ(ret, 42);
}

/// @test
/// Forwards both arguments to the wrapped lambda and returns its sum, f(20, 22) yielding 42.
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, LambdaCalculation)
{
  move_only_function<int(int, int)> f = [](int a, int b) { return a + b; };
  int ret = f(20, 22);
  EXPECT_EQ(ret, 42);
}

/// @test
/// Preserves a by-value capture, the wrapped lambda returning the captured 42.
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, LambdaWithCapture)
{
  int capturedValue = 42;
  move_only_function<int()> f = [capturedValue]() { return capturedValue; };
  int ret = f();
  EXPECT_EQ(ret, 42);
}

/// @test
/// Stores a lambda carrying 24 bytes of by-value captures, six ints, the invocation summing the captures with
/// the argument to 42.
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
/// Default-constructs an empty function and destroys it without invoking, since calling an empty
/// move_only_function is undefined.
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, CheckEmptyConstruction)
{
  move_only_function<int()> outerF;
  // int ret  = outerF(); // not allowed -> UB
}

/// @test
/// Keeps captured state alive across move assignment, a function assigned from an inner scope still returning
/// 42 after its source has gone out of scope.
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
/// Wraps a free function pointer taking and returning nothing and completes an invocation of it.
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, FreeFunctionEmpty)
{
  move_only_function<void()> f = &fooEmpty;
  f();
}

void fooParam(int /* i */) {}
/// @test
/// Wraps a one-parameter free function and forwards the int argument on invocation.
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, FreeFunctionWithParameter)
{
  move_only_function<decltype(fooParam)> f = &fooParam;
  f(42);
}

void fooParams(int /* i */, char /* c */, bool /* b */) {}
/// @test
/// Wraps a three-parameter free function and forwards int, char, and bool arguments on invocation.
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, FreeFunctionWithParameters)
{
  move_only_function<decltype(fooParams)> f = &fooParams;
  f(42, 'f', false);
}

int fooReturn() { return 42; }
/// @test
/// Returns the wrapped free function's value, an invocation yielding 42.
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, FreeFunctionWithReturn)
{
  move_only_function<decltype(fooReturn)> f = &fooReturn;
  int ret = f();
  EXPECT_EQ(ret, 42);
}

int fooCalculation(int a, int b) { return a + b; }
/// @test
/// Forwards both arguments to the wrapped free function and returns its sum, f(20, 22) yielding 42.
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, FreeFunctionWithCalculation)
{
  move_only_function<decltype(fooCalculation)> f = &fooCalculation;
  int ret = f(20, 22);
  EXPECT_EQ(ret, 42);
}

struct Foo
{
  void empty() {}
  void param(int /* i */) {}
  void params(int /* i */, char /* c */, bool /* b */) {}
  int ret() { return 42; }
  int calculation(int a, int b) { return a + b; }
};

/// @test
/// Binds a member function pointer with the object pointer as the leading parameter and invokes it on an
/// instance.
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, MemberFunctionEmpty)
{
  Foo foo;
  move_only_function<void(Foo*)> f = &Foo::empty;
  f(&foo);
}

/// @test
/// Binds a member function pointer and forwards the object pointer plus an int argument on invocation.
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, MemberFunctionWithParameter)
{
  Foo foo;
  move_only_function<void(Foo*, int)> f = &Foo::param;
  f(&foo, 42);
}

/// @test
/// Binds a member function pointer and forwards the object pointer plus int, char, and bool arguments on
/// invocation.
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, MemberFunctionWithParameters)
{
  Foo foo;
  move_only_function<void(Foo*, int, char, bool)> f = &Foo::params;
  f(&foo, 42, 'f', false);
}

/// @test
/// Returns a member function's value through the wrapper, invocation on an instance yielding 42.
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, MemberFunctionWithReturn)
{
  Foo foo;
  move_only_function<int(Foo*)> f = &Foo::ret;
  int ret = f(&foo);
  EXPECT_EQ(ret, 42);
}

/// @test
/// Forwards arguments to a member function and returns its sum, invocation with 20 and 22 yielding 42.
/// @requirements(SEN-1047)
TEST(MoveOnlyFunction, MemberFunctionWithCalculation)
{
  Foo foo;
  move_only_function<int(Foo*, int, int)> f = &Foo::calculation;
  int ret = f(&foo, 20, 22);
  EXPECT_EQ(ret, 42);
}
