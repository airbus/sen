// === shell_test.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "util.h"

// sen
#include "sen/core/base/checked_conversions.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/var.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <stdexcept>
#include <string>

namespace sen::components::shell
{
void adaptCallArguments(const Method* method, VarList& argValues, const Method* writerMethod);
}

/// @test
/// Parses a quoted string containing spaces and a numeric literal from one command line into two Var
/// arguments, the string kept whole and the number held as an integral value.
/// @requirements(SEN-369)
TEST(ShellUtilTest, ParseArgvComplexStrings)
{
  sen::VarList result;
  sen::components::shell::parseArgv(nullptr, "cmd \"spaced string\", 123", result);

  ASSERT_EQ(result.size(), 2U);
  EXPECT_EQ(result[0].getCopyAs<std::string>(), "spaced string");
  EXPECT_TRUE(result[1].holdsIntegralValue());
  EXPECT_EQ(result[1].getCopyAs<int32_t>(), 123);
}

/// @test
/// Captures everything after the command name as one unquoted string argument when the method takes a
/// single string parameter.
/// @requirements(SEN-369)
TEST(ShellUtilTest, ParseArgvSingleStringShortcut)
{
  sen::MethodSpec mSpec {{"m", "d", {}}, sen::VoidType::get()};
  mSpec.callableSpec.args.emplace_back("a", "d", sen::StringType::get());
  const auto method = sen::Method::make(mSpec);

  sen::VarList result;
  sen::components::shell::parseArgv(method.get(), "cmd This is a long raw string", result);

  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result[0].getCopyAs<std::string>(), "This is a long raw string");
}

/// @test
/// Throws a runtime error for an argument list whose entries are not separated by commas.
/// @requirements(SEN-369, SEN-1049)
TEST(ShellUtilTest, ParseArgvMalformedThrows)
{
  sen::VarList result;
  EXPECT_THROW(sen::components::shell::parseArgv(nullptr, "cmd \"str\" 123", result), std::runtime_error);
}

/// @test
/// Formats printf-style through fromFormat, rendering Val: %02d with 5 as Val: 05.
/// @requirements(SEN-369)
TEST(ShellUtilTest, UtilFormatting)
{
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
  const auto out = sen::components::shell::fromFormat("Val: %02d", 5);
  EXPECT_EQ(out, "Val: 05");
}

/// @test
/// Trims whitespace from both ends of a string by default and strips a caller-supplied character set such
/// as dots.
/// @requirements(SEN-369)
TEST(ShellUtilTest, UtilTrimming)
{
  std::string s1 = "  both  ";
  sen::components::shell::trim(s1);
  EXPECT_EQ(s1, "both");

  std::string s2 = "...dots...";
  sen::components::shell::trim(s2, ".");
  EXPECT_EQ(s2, "dots");
}

/// @test
/// Leaves the VarList empty when the command line carries no arguments after the command name.
/// @requirements(SEN-369)
TEST(ShellUtilTest, ParseArgvEmpty)
{
  sen::VarList result;
  sen::components::shell::parseArgv(nullptr, "cmd", result);
  EXPECT_TRUE(result.empty());
}

/// @test
/// Yields a single empty string argument, without throwing, when a method taking no parameters is given
/// unparseable JSON text.
/// @requirements(SEN-369, SEN-1049)
TEST(ShellUtilTest, ParseArgvNoArgsProvidedByMethodThrows)
{
  const sen::MethodSpec mSpec {{"m", "d", {}}, sen::VoidType::get()};
  const auto method = sen::Method::make(mSpec);

  sen::VarList result;
  sen::components::shell::parseArgv(method.get(), "cmd {bad_json", result);

  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result[0].getCopyAs<std::string>(), "");
}

/// @test
/// Parses a bracketed number list into a single argument holding a nested VarList with its three
/// elements in order.
/// @requirements(SEN-369)
TEST(ShellUtilTest, ParseArgvArray)
{
  sen::VarList result;
  sen::components::shell::parseArgv(nullptr, "cmd [1, 2, 3]", result);

  ASSERT_EQ(result.size(), 1U);
  EXPECT_TRUE(result[0].holds<sen::VarList>());

  const auto& internalList = result[0].get<sen::VarList>();
  ASSERT_EQ(internalList.size(), 3U);
  EXPECT_EQ(internalList[0].getCopyAs<int32_t>(), 1);
  EXPECT_EQ(internalList[2].getCopyAs<int32_t>(), 3);
}

/// @test
/// Adapts an argument list that already matches the method signature without throwing, leaving the value
/// intact.
/// @requirements(SEN-369)
TEST(ShellUtilTest, AdaptCallArgumentsNormalFlow)
{
  sen::MethodSpec methodSpec {{"normalMethod", "desc", {}}, sen::VoidType::get()};
  methodSpec.callableSpec.args.emplace_back("val", "desc", sen::UInt32Type::get());
  const auto method = sen::Method::make(methodSpec);

  sen::VarList args = {sen::Var(sen::std_util::checkedConversion<uint32_t>(42))};

  EXPECT_NO_THROW(sen::components::shell::adaptCallArguments(method.get(), args, nullptr));
  EXPECT_EQ(args.size(), 1U);
  EXPECT_EQ(args[0].getCopyAs<uint32_t>(), 42U);
}

/// @test
/// Returns without throwing and leaves the argument list untouched when its size does not match the
/// method's parameter count.
/// @requirements(SEN-369)
TEST(ShellUtilTest, AdaptCallArgumentsSizeMismatch)
{
  sen::MethodSpec methodSpec {{"normalMethod", "desc", {}}, sen::VoidType::get()};
  methodSpec.callableSpec.args.emplace_back("val", "desc", sen::UInt32Type::get());
  const auto method = sen::Method::make(methodSpec);

  sen::VarList args = {sen::Var(sen::std_util::checkedConversion<uint32_t>(42)),
                       sen::Var(sen::std_util::checkedConversion<uint32_t>(84))};

  EXPECT_NO_THROW(sen::components::shell::adaptCallArguments(method.get(), args, nullptr));
  EXPECT_EQ(args.size(), 2U);
}

/// @test
/// Reorders arguments given in the writer method's parameter order into the target method's order,
/// matching parameters by name.
/// @requirements(SEN-369)
TEST(ShellUtilTest, AdaptCallArgumentsWriterMethod)
{
  sen::MethodSpec methodSpec1 {{"method", "desc", {}}, sen::VoidType::get()};
  methodSpec1.callableSpec.args.emplace_back("argB", "desc", sen::UInt32Type::get());
  methodSpec1.callableSpec.args.emplace_back("argA", "desc", sen::UInt32Type::get());
  const auto method = sen::Method::make(methodSpec1);

  sen::MethodSpec methodSpec2 {{"method", "desc", {}}, sen::VoidType::get()};
  methodSpec2.callableSpec.args.emplace_back("argA", "desc", sen::UInt32Type::get());
  methodSpec2.callableSpec.args.emplace_back("argB", "desc", sen::UInt32Type::get());
  const auto writerMethod = sen::Method::make(methodSpec2);

  sen::VarList args = {sen::Var(sen::std_util::checkedConversion<uint32_t>(20)),
                       sen::Var(sen::std_util::checkedConversion<uint32_t>(10))};

  sen::components::shell::adaptCallArguments(method.get(), args, writerMethod.get());

  ASSERT_EQ(args.size(), 2U);
  EXPECT_EQ(args[0].getCopyAs<uint32_t>(), 10U);
  EXPECT_EQ(args[1].getCopyAs<uint32_t>(), 20U);
}
