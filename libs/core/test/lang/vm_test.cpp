// === vm_test.cpp =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/base/result.h"
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

/// Compiles and runs a query, handing back what the VM returned rather than asserting on it. The
/// cases below need the result itself: a clause can fail to run, or produce a value that is not a
/// bool, and those are different outcomes with different meanings for the caller.
[[nodiscard]] Result<Value, VM::RuntimeError> run(const std::string& program, std::vector<ValueGetter> environment = {})
{
  VM vm;

  auto compiled = vm.compile(vm.parse(program));
  EXPECT_TRUE(compiled.isOk()) << "failed to compile: " << program;
  if (compiled.isError())
  {
    return Err(VM::RuntimeError {compiled.getError().what});
  }

  auto chunk = std::move(compiled).getValue();
  return environment.empty() ? vm.interpret(chunk) : vm.interpret(chunk, environment);
}

/// A getter for a property with no value right now. An empty optional reaches the stack as
/// `VariantAccessError`, which is how the compiler and interest tests model one, and reading an
/// integer as a bool raises the same `bad_variant_access` that `fetchVariable` converts.
[[nodiscard]] ValueGetter emptyOptional()
{
  return []() { return Value {std::get<bool>(Value {int64_t {1}})}; };
}

/// A conjunction of comparisons between literals, which costs the chunk two constants per term and
/// no variables.
[[nodiscard]] std::string literalConjunction(int terms)
{
  std::string where = "1 = 1";
  for (int index = 1; index < terms; ++index)
  {
    where += " AND 1 = 1";
  }
  return where;
}

/// A conjunction of comparisons between variables nothing has named before, which costs the chunk
/// two variable slots per term and no constants.
[[nodiscard]] std::string variableConjunction(int terms)
{
  std::string where;
  for (int index = 0; index < terms; ++index)
  {
    where += (index == 0 ? "" : " AND ");
    where += "v" + std::to_string(2 * index) + " = v" + std::to_string(2 * index + 1);
  }
  return where;
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

/// @test
/// A boolean property compares against a boolean literal.
///
/// Nothing had ever evaluated this: the comparison path tests `holds<bool>` before it reaches the
/// numeric ladder, and that test had never been true in any run of the suite. A query on a flag is
/// the simplest thing an interest can ask, and it was the untested one.
TEST(VmTest, ABooleanVariableComparesAgainstABooleanLiteral)
{
  std::vector<ValueGetter> set {[]() { return Value {true}; }};
  EXPECT_TRUE(evaluate(query("value = true"), set));

  std::vector<ValueGetter> against {[]() { return Value {true}; }};
  EXPECT_FALSE(evaluate(query("value = false"), against));

  std::vector<ValueGetter> clear {[]() { return Value {false}; }};
  EXPECT_TRUE(evaluate(query("value != true"), clear));

  std::vector<ValueGetter> literals {};
  EXPECT_TRUE(evaluate(query("true = true"), literals));
}

/// @test
/// Comparing values of two different kinds fails the query rather than answering it.
///
/// A string and a number have no order between them, and neither do a boolean and a number. The VM
/// reports that through its error channel, which is what `interpret` is declared to return: before
/// this it threw, and the one caller evaluates a query per object per cycle without catching.
TEST(VmTest, ComparingValuesOfDifferentKindsFailsTheQuery)
{
  std::vector<ValueGetter> text {[]() { return Value {std::string {"ownship"}}; }};
  const auto compared = run(query("value > 1"), text);
  ASSERT_TRUE(compared.isError()) << "a string against a number was answered";
  EXPECT_NE(compared.getError().what.find("invalid value for comparison"), std::string::npos)
    << compared.getError().what;

  std::vector<ValueGetter> flag {[]() { return Value {true}; }};
  EXPECT_TRUE(run(query("value = 1"), flag).isError()) << "a boolean against a number was answered";

  std::vector<ValueGetter> arithmetic {[]() { return Value {std::string {"ownship"}}; }};
  const auto added = run(query("value + 1 = 2"), arithmetic);
  ASSERT_TRUE(added.isError()) << "arithmetic on a string was answered";
  EXPECT_NE(added.getError().what.find("invalid value for math"), std::string::npos) << added.getError().what;

  std::vector<ValueGetter> negated {[]() { return Value {std::string {"ownship"}}; }};
  EXPECT_TRUE(run(query("!value"), negated).isError()) << "negating a string was answered";
}

/// @test
/// A property with no value does not match a comparison, whatever shape the comparison takes.
///
/// An empty optional is not equal to anything, not greater than anything, and not inside any
/// range. The comparison path has said so since it was written; what had never been run is the
/// same value arriving through arithmetic or as the right-hand operand, which is a different
/// branch in each of the two macros the file is built from.
TEST(VmTest, AnEmptyOptionalDoesNotMatchAComparison)
{
  const std::vector<std::string> clauses {
    "value = 1", "value != 1", "value > 1", "1 > value", "value + 1 = 2", "1 + value = 2", "value BETWEEN 1 AND 3"};

  for (const auto& clause: clauses)
  {
    std::vector<ValueGetter> environment {emptyOptional()};
    const auto result = run(query(clause), environment);

    ASSERT_TRUE(result.isOk()) << clause << " failed the query instead of answering it";
    ASSERT_TRUE(std::holds_alternative<bool>(result.getValue())) << clause << " did not answer with a boolean";
    EXPECT_FALSE(std::get<bool>(result.getValue())) << clause << " matched an empty optional";
  }
}

/// @test
/// A property with no value reaches the top of the query through the operators that cannot answer
/// for it, and the caller leaves the object out.
///
/// `!`, `AND` and `OR` have no reading of an absent value: treating it as false would let `!field`
/// match an object whose field is not there. So it is carried on, the result is not a boolean, and
/// `ObjectFilter` drops that object. Each of these five clauses used to throw
/// `std::bad_variant_access` out of `interpret` instead, past a caller that evaluates a query per
/// object per cycle and catches nothing.
TEST(VmTest, AnEmptyOptionalInALogicalOperatorReachesTheTop)
{
  const std::vector<std::string> clauses {
    "!value", "value AND true", "true AND value", "value OR true", "false OR value", "value"};

  for (const auto& clause: clauses)
  {
    std::vector<ValueGetter> environment {emptyOptional()};
    const auto result = run(query(clause), environment);

    ASSERT_TRUE(result.isOk()) << clause << " failed the query instead of carrying the empty optional";
    EXPECT_TRUE(std::holds_alternative<VariantAccessError>(result.getValue()))
      << clause << " did not carry the empty optional to the top";
  }
}

/// @test
/// BETWEEN refuses bounds that arrive the wrong way round.
///
/// The parser catches this when both bounds are literals, so the check in the VM is reachable only
/// when a bound is a property, which is the case nothing had run.
TEST(VmTest, BetweenRefusesBoundsAVariablePutTheWrongWayRound)
{
  std::vector<ValueGetter> ordered {[]() { return Value {int64_t {1}}; }, []() { return Value {int64_t {3}}; }};
  EXPECT_TRUE(evaluate(query("2 BETWEEN low AND high"), ordered));

  std::vector<ValueGetter> reversed {[]() { return Value {int64_t {3}}; }, []() { return Value {int64_t {1}}; }};
  const auto result = run(query("2 BETWEEN low AND high"), reversed);
  ASSERT_TRUE(result.isError()) << "reversed bounds were answered";
  EXPECT_NE(result.getError().what.find("less or equal"), std::string::npos) << result.getError().what;
}

/// @test
/// A query with more constants than the chunk can hold does not compile.
///
/// The constant table is indexed by one byte, so the refusal is what keeps an index from wrapping
/// round onto another constant and answering a different question than the one asked.
///
/// 130 terms is 260 constants, the smallest input that crosses the 256 the table holds. The size
/// matters: the sanitizer lane records a 100-frame stack on every allocation, so parsing twice
/// this many constants took 17 s of that lane's 20 s per-test budget and timed out on two runs.
TEST(VmTest, AQueryWithTooManyConstantsDoesNotCompile)
{
  const VM vm;
  const auto result = vm.compile(vm.parse(query(literalConjunction(130))));

  ASSERT_TRUE(result.isError()) << "a query with 260 constants compiled";
  EXPECT_NE(result.getError().what.find("too many constants"), std::string::npos) << result.getError().what;
}

/// @test
/// A query naming more properties than the chunk can hold does not compile. Same byte-wide index
/// as the constants, and the same consequence if it wrapped. Sized like the constants test above,
/// and for the same reason.
TEST(VmTest, AQueryWithTooManyVariablesDoesNotCompile)
{
  const VM vm;
  const auto result = vm.compile(vm.parse(query(variableConjunction(130))));

  ASSERT_TRUE(result.isError()) << "a query naming 260 properties compiled";
  EXPECT_NE(result.getError().what.find("too many variables"), std::string::npos) << result.getError().what;
}

/// @test
/// A clause that is not a condition evaluates to the value it says, not to a boolean.
///
/// `WHERE 1 + 1` parses and runs, and the result is a number. The caller cannot treat that as a
/// match or as a miss, so it leaves the object out — which is only safe because it checks, and this
/// is the case that says it has to.
TEST(VmTest, AClauseThatIsNotAConditionDoesNotEvaluateToABoolean)
{
  const auto result = run(query("1 + 1"));

  ASSERT_TRUE(result.isOk());
  EXPECT_FALSE(std::holds_alternative<bool>(result.getValue())) << "a sum evaluated to a boolean";
}

/// @test
/// A query with more pending operands than the value stack can hold is refused.
///
/// The constant and variable tables each check that they are not full; the stack did not, and
/// `StaticVector::push_back` reports a full container by returning a value `std::stack::push`
/// discards, so a push was dropped and a later `pop` read a slot nothing wrote. Right-nested
/// arithmetic reaches it: every left operand waits until the innermost expression resolves, and
/// alternating a literal with a property keeps both tables under their limits while the stack
/// goes past its 256.
TEST(VmTest, AQueryDeeperThanTheValueStackIsRefused)
{
  constexpr int levels = 160;

  // Grown at both ends rather than by re-wrapping one string, which copies the whole expression
  // once per level. Every variable reads the same, so which index ends up innermost does not matter.
  std::string where;
  std::string tail;
  std::vector<ValueGetter> environment;
  environment.reserve(levels);
  for (int index = 0; index < levels; ++index)
  {
    where += "v";
    where += std::to_string(index);
    where += " + (1 + (";
    tail += "))";
    environment.emplace_back([]() { return Value {int64_t {1}}; });
  }
  where += "0";
  where += tail;

  const auto result = run(query(where), environment);

  ASSERT_TRUE(result.isError()) << "a query " << (2 * levels + 1) << " operands deep was evaluated";
  EXPECT_NE(result.getError().what.find("too deep"), std::string::npos) << result.getError().what;
}

}  // namespace sen::lang::test
