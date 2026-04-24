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
/// Ensures arguments with quotes and spaces are correctly parsed into VarList
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
/// Ensures single string arguments without quotes are captured entirely
/// @requirements(SEN-369)
TEST(ShellUtilTest, ParseArgvSingleStringShortcut)
{
  sen::MethodSpec mSpec {{"m", "d"}, sen::VoidType::get()};
  mSpec.callableSpec.args.emplace_back("a", "d", sen::StringType::get());
  const auto method = sen::Method::make(mSpec);

  sen::VarList result;
  sen::components::shell::parseArgv(method.get(), "cmd This is a long raw string", result);

  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result[0].getCopyAs<std::string>(), "This is a long raw string");
}

/// @test
/// Verifies that malformed argument strings throw runtime errors
/// @requirements(SEN-369, SEN-1049)
TEST(ShellUtilTest, ParseArgvMalformedThrows)
{
  sen::VarList result;
  EXPECT_THROW(sen::components::shell::parseArgv(nullptr, "cmd \"str\" 123", result), std::runtime_error);
}

/// @test
/// Validates string formatting utility
/// @requirements(SEN-369)
TEST(ShellUtilTest, UtilFormatting)
{
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
  const auto out = sen::components::shell::fromFormat("Val: %02d", 5);
  EXPECT_EQ(out, "Val: 05");
}

/// @test
/// Validates trimming logic for various whitespace configurations
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
/// Validates that an empty command results in an empty VarList
/// @requirements(SEN-369)
TEST(ShellUtilTest, ParseArgvEmpty)
{
  sen::VarList result;
  sen::components::shell::parseArgv(nullptr, "cmd", result);
  EXPECT_TRUE(result.empty());
}

/// @test
/// Validates parsing arguments where method has no args but user provides malformed JSON
/// @requirements(SEN-369, SEN-1049)
TEST(ShellUtilTest, ParseArgvNoArgsProvidedByMethodThrows)
{
  const sen::MethodSpec mSpec {{"m", "d"}, sen::VoidType::get()};
  const auto method = sen::Method::make(mSpec);

  sen::VarList result;
  sen::components::shell::parseArgv(method.get(), "cmd {bad_json", result);

  ASSERT_EQ(result.size(), 1U);
  EXPECT_EQ(result[0].getCopyAs<std::string>(), "");
}

/// @test
/// Verifies sequence of numbers parsed as array correctly
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
/// Verifies adaptation of arguments in normal method execution
/// @requirements(SEN-369)
TEST(ShellUtilTest, AdaptCallArgumentsNormalFlow)
{
  sen::MethodSpec methodSpec {{"normalMethod", "desc"}, sen::VoidType::get()};
  methodSpec.callableSpec.args.emplace_back("val", "desc", sen::UInt32Type::get());
  const auto method = sen::Method::make(methodSpec);

  sen::VarList args = {sen::Var(sen::std_util::checkedConversion<uint32_t>(42))};

  EXPECT_NO_THROW(sen::components::shell::adaptCallArguments(method.get(), args, nullptr));
  EXPECT_EQ(args.size(), 1U);
  EXPECT_EQ(args[0].getCopyAs<uint32_t>(), 42U);
}

/// @test
/// Verifies adaptation handles size mismatches gracefully by returning early
/// @requirements(SEN-369)
TEST(ShellUtilTest, AdaptCallArgumentsSizeMismatch)
{
  sen::MethodSpec methodSpec {{"normalMethod", "desc"}, sen::VoidType::get()};
  methodSpec.callableSpec.args.emplace_back("val", "desc", sen::UInt32Type::get());
  const auto method = sen::Method::make(methodSpec);

  sen::VarList args = {sen::Var(sen::std_util::checkedConversion<uint32_t>(42)),
                       sen::Var(sen::std_util::checkedConversion<uint32_t>(84))};

  EXPECT_NO_THROW(sen::components::shell::adaptCallArguments(method.get(), args, nullptr));
  EXPECT_EQ(args.size(), 2U);
}

/// @test
/// Verifies writer method adaptation logic maps variables correctly
/// @requirements(SEN-369)
TEST(ShellUtilTest, AdaptCallArgumentsWriterMethod)
{
  sen::MethodSpec methodSpec1 {{"method", "desc"}, sen::VoidType::get()};
  methodSpec1.callableSpec.args.emplace_back("argB", "desc", sen::UInt32Type::get());
  methodSpec1.callableSpec.args.emplace_back("argA", "desc", sen::UInt32Type::get());
  const auto method = sen::Method::make(methodSpec1);

  sen::MethodSpec methodSpec2 {{"method", "desc"}, sen::VoidType::get()};
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
