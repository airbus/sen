// === vm_test.cpp =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/lang/vm.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace sen::lang::test
{

namespace
{

/// Compiles a query and evaluates its WHERE clause, with an environment for the variables it names.
[[nodiscard]] bool evaluate(const std::string& program, std::vector<ValueGetter> environment = {})
{
  VM vm;

  auto compiled = vm.compile(vm.parse(program));
  EXPECT_TRUE(compiled.isOk()) << "failed to compile: " << program;

  auto chunk = std::move(compiled).getValue();
  auto result = environment.empty() ? vm.interpret(chunk) : vm.interpret(chunk, environment);
  EXPECT_TRUE(result.isOk()) << "failed to interpret: " << program;

  const auto& value = result.getValue();
  EXPECT_TRUE(std::holds_alternative<bool>(value)) << "the clause did not evaluate to a bool: " << program;
  return std::holds_alternative<bool>(value) && std::get<bool>(value);
}

/// A query whose WHERE clause is the expression under test, wrapped so that the result is always a bool
/// and can be asserted without knowing which numeric type the arithmetic produced.
[[nodiscard]] std::string query(const std::string& where)
{
  return R"(SELECT some.Class FROM some.bus WHERE )" + where;
}

}  // namespace

/// @test
/// Arithmetic inside a WHERE clause, one operator at a time.
///
/// The compiler tests exercise the comparisons and the logical operators; the four arithmetic opcodes
/// had no test, so opAdd, opSub, opMul and opDiv were never interpreted. Each case compares the result
/// against a literal, so the assertion needs no knowledge of the type the operation produced.
TEST(VmTest, ArithmeticOperatorsEvaluate)
{
  EXPECT_TRUE(evaluate(query("1 + 2 = 3")));
  EXPECT_TRUE(evaluate(query("5 - 2 = 3")));
  EXPECT_TRUE(evaluate(query("2 * 3 = 6")));
  EXPECT_TRUE(evaluate(query("6 / 2 = 3")));

  // The same four, with the comparison false, so a stuck "true" cannot pass every case above.
  EXPECT_FALSE(evaluate(query("1 + 2 = 4")));
  EXPECT_FALSE(evaluate(query("5 - 2 = 4")));
  EXPECT_FALSE(evaluate(query("2 * 3 = 7")));
  EXPECT_FALSE(evaluate(query("6 / 2 = 4")));
}

/// @test
/// The two comparisons the compiler tests leave out.
TEST(VmTest, InequalityAndLessOrEqualEvaluate)
{
  EXPECT_TRUE(evaluate(query("1 != 2")));
  EXPECT_FALSE(evaluate(query("2 != 2")));
  EXPECT_TRUE(evaluate(query("2 <= 2")));
  EXPECT_TRUE(evaluate(query("1 <= 2")));
  EXPECT_FALSE(evaluate(query("3 <= 2")));
}

/// @test
/// A variable of every type a Value can hold compares against a literal.
///
/// This is the shape the kernel uses the VM in: an interest filter compares an object's property
/// against a constant, and a property can be any of the eleven alternatives of Value. A literal in a
/// query only ever parses to a couple of them, so the rest of the type combinations in the comparison
/// path are reachable only through a variable — which is why they had never been evaluated.
TEST(VmTest, AVariableOfEveryNumericTypeComparesAgainstALiteral)
{
  struct Case
  {
    const char* name;
    Value value;
  };

  const std::vector<Case> cases {{"float32", float32_t {2.0F}},
                                 {"float64", float64_t {2.0}},
                                 {"int32", int32_t {2}},
                                 {"uint32", uint32_t {2U}},
                                 {"int64", int64_t {2}},
                                 {"uint64", uint64_t {2U}},
                                 {"uint8", static_cast<uint8_t>(2U)},
                                 {"int16", static_cast<int16_t>(2)},
                                 {"uint16", static_cast<uint16_t>(2U)}};

  for (const auto& testCase: cases)
  {
    std::vector<ValueGetter> environment {[&testCase]() { return testCase.value; }};
    EXPECT_TRUE(evaluate(query("value > 1"), environment)) << testCase.name << " > 1 was not true";

    std::vector<ValueGetter> again {[&testCase]() { return testCase.value; }};
    EXPECT_FALSE(evaluate(query("value > 3"), again)) << testCase.name << " > 3 was not false";
  }
}

/// @test
/// A string variable compares against a string literal, which is the other type a property can carry.
TEST(VmTest, AStringVariableComparesAgainstALiteral)
{
  std::vector<ValueGetter> environment {[]() { return Value {std::string {"ownship"}}; }};
  EXPECT_TRUE(evaluate(query(R"(value = "ownship")"), environment));

  std::vector<ValueGetter> other {[]() { return Value {std::string {"ownship"}}; }};
  EXPECT_FALSE(evaluate(query(R"(value = "wingman")"), other));
}

}  // namespace sen::lang::test
