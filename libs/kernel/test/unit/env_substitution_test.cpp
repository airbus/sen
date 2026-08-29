// === env_substitution_test.cpp =======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// kernel
#include "env_substitution.h"
#include "sen/kernel/bootloader.h"

// sen
#include "sen/core/meta/var.h"

// gtest
#include <gtest/gtest.h>

// std
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>

using sen::kernel::impl::replaceEnvPattern;

namespace
{

// setenv is POSIX and Windows is a supported target, so the two spellings are
// wrapped here rather than at every call site.
void setVariable(const char* name, const char* value)
{
#ifdef _WIN32
  _putenv_s(name, value);
#else
  // cstdlib provides these; the header include-cleaner asks for is <stdlib.h>,
  // which the deprecated-headers checks forbid, so no include satisfies both.
  // NOLINTNEXTLINE(misc-include-cleaner)
  setenv(name, value, 1);
#endif
}

void clearVariable(const char* name)
{
#ifdef _WIN32
  _putenv_s(name, "");
#else
  // cstdlib provides these; the header include-cleaner asks for is <stdlib.h>,
  // which the deprecated-headers checks forbid, so no include satisfies both.
  // NOLINTNEXTLINE(misc-include-cleaner)
  unsetenv(name);
#endif
}

class AnEnvPattern: public ::testing::Test
{
protected:
  void SetUp() override
  {
    setVariable("SEN_TEST_VAR", "expanded");
    clearVariable("SEN_TEST_UNSET");
  }
  void TearDown() override
  {
    clearVariable("SEN_TEST_VAR");
    clearVariable("SEN_TEST_UNSET");
  }
};

}  // namespace

/// @test
/// A pattern naming a variable that exists is replaced by its value.
TEST_F(AnEnvPattern, expandsAVariableThatExists)
{
  EXPECT_EQ(replaceEnvPattern("value: @env(SEN_TEST_VAR)"), "value: expanded");
}

/// @test
/// Text carrying no pattern is returned unchanged.
TEST_F(AnEnvPattern, leavesTextWithoutAPatternAlone)
{
  EXPECT_EQ(replaceEnvPattern("value: plain@example.com"), "value: plain@example.com");
}

/// @test
/// An unset variable falls back to the default given after the comma.
TEST_F(AnEnvPattern, usesTheDefaultWhenTheVariableIsUnset)
{
  EXPECT_EQ(replaceEnvPattern("value: @env(SEN_TEST_UNSET,fallback)"), "value: fallback");
}

/// @test
/// The default is ignored when the variable is set.
TEST_F(AnEnvPattern, prefersTheVariableOverTheDefault)
{
  EXPECT_EQ(replaceEnvPattern("value: @env(SEN_TEST_VAR,fallback)"), "value: expanded");
}

/// @test
/// Whitespace around the comma is accepted: this is the form the documentation shows,
/// and it used to leave the pattern in the configuration with no diagnostic.
TEST_F(AnEnvPattern, acceptsSpacesAroundTheComma)
{
  EXPECT_EQ(replaceEnvPattern("value: @env(SEN_TEST_UNSET, fallback)"), "value: fallback");
  EXPECT_EQ(replaceEnvPattern("value: @env(SEN_TEST_UNSET ,fallback)"), "value: fallback");
  EXPECT_EQ(replaceEnvPattern("value: @env(SEN_TEST_UNSET\t,\tfallback)"), "value: fallback");
  EXPECT_EQ(replaceEnvPattern("value: @env(SEN_TEST_VAR )"), "value: expanded");
}

/// @test
/// An unset variable with no default stops the load rather than substituting nothing.
/// The type is only std::exception: throwRuntimeError throws cpptrace::runtime_error
/// in Debug and std::runtime_error otherwise, and those share no closer base.
TEST_F(AnEnvPattern, throwsWhenTheVariableIsUnsetAndHasNoDefault)
{
  try
  {
    static_cast<void>(replaceEnvPattern("value: @env(SEN_TEST_UNSET)"));
    FAIL() << "expected a throw for an unset variable with no default";
  }
  catch (const std::exception& error)
  {
    EXPECT_NE(std::string(error.what()).find("SEN_TEST_UNSET"), std::string::npos);
  }
}

/// @test
/// One backslash escapes the pattern: the text is reproduced without the backslash.
TEST_F(AnEnvPattern, honoursASingleEscape)
{
  EXPECT_EQ(replaceEnvPattern(R"(value: \@env(SEN_TEST_VAR))"), "value: @env(SEN_TEST_VAR)");
  EXPECT_EQ(replaceEnvPattern(R"(value: \@env(SEN_TEST_VAR,fallback))"), "value: @env(SEN_TEST_VAR,fallback)");
}

/// @test
/// An escaped pattern never reads the variable, so an unset one is not an error.
/// The lookup used to run first and abort the load on text meant to be left alone.
TEST_F(AnEnvPattern, doesNotReadTheVariableOfAnEscapedPattern)
{
  EXPECT_EQ(replaceEnvPattern(R"(value: \@env(SEN_TEST_UNSET))"), "value: @env(SEN_TEST_UNSET)");
}

/// @test
/// Two backslashes escape each other, so the variable is expanded after one of them.
TEST_F(AnEnvPattern, expandsAfterAnEvenNumberOfBackslashes)
{
  EXPECT_EQ(replaceEnvPattern(R"(value: \\@env(SEN_TEST_VAR))"), R"(value: \expanded)");
}

/// @test
/// An odd count above one keeps the pairs and escapes the pattern.
TEST_F(AnEnvPattern, escapesAfterAnOddNumberOfBackslashes)
{
  EXPECT_EQ(replaceEnvPattern(R"(value: \\\@env(SEN_TEST_VAR))"), R"(value: \@env(SEN_TEST_VAR))");
}

/// @test
/// Every pattern in the text is handled, not only the first.
TEST_F(AnEnvPattern, expandsEveryOccurrence)
{
  EXPECT_EQ(replaceEnvPattern("a: @env(SEN_TEST_VAR)\nb: @env(SEN_TEST_VAR)"), "a: expanded\nb: expanded");
}

/// @test
/// A pattern cannot span lines, so a newline before the comma is not a match.
TEST_F(AnEnvPattern, doesNotMatchAcrossLines)
{
  const std::string text = "value: @env(SEN_TEST_VAR,\nfallback)";
  EXPECT_EQ(replaceEnvPattern(text), text);
}

/// @test
/// An included file gets the same expansion as the top-level one. It used to reach
/// the tree as literal text, so a pattern there never resolved and an unset variable
/// raised nothing.
TEST_F(AnEnvPattern, expandsPatternsInsideAnIncludedFile)
{
  const auto directory = std::filesystem::temp_directory_path() / "sen_env_include_test";
  std::filesystem::create_directories(directory);
  {
    std::ofstream included(directory / "included.yaml");
    included << "includedValue: \"@env(SEN_TEST_VAR)\"\n";
  }

  const std::string top = "include: included.yaml\ntopValue: \"@env(SEN_TEST_VAR)\"\n";
  const sen::VarMap config = sen::kernel::getConfigAsVarFromYaml(top, directory / "main.yaml", false);

  EXPECT_EQ(config.at("topValue").getCopyAs<std::string>(), "expanded");
  EXPECT_EQ(config.at("includedValue").getCopyAs<std::string>(), "expanded");

  std::filesystem::remove_all(directory);
}
