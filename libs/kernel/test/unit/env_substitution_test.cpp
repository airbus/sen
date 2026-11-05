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

// spdlog
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/spdlog.h>

// std
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

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
/// A pattern is replaced where it sits, so it can be part of a value rather than the whole of it.
TEST_F(AnEnvPattern, expandsAPatternThatIsPartOfALargerValue)
{
  EXPECT_EQ(replaceEnvPattern("value: @env(SEN_TEST_VAR).cucu"), "value: expanded.cucu");
  EXPECT_EQ(replaceEnvPattern("value: before-@env(SEN_TEST_VAR)-after"), "value: before-expanded-after");
  EXPECT_EQ(replaceEnvPattern("value: @env(SEN_TEST_VAR)@env(SEN_TEST_VAR)"), "value: expandedexpanded");
}

/// @test
/// Expands the shape users ask for, a pattern with a default inside a quoted scalar with text after it: the
/// set variable yields its value, the unset one its default, and the text around the pattern survives.
TEST_F(AnEnvPattern, expandsInsideAQuotedScalarWithADefault)
{
  EXPECT_EQ(replaceEnvPattern(R"(query: "SELECT * FROM @env(SEN_TEST_VAR, fallback).myBus")"),
            R"(query: "SELECT * FROM expanded.myBus")");
  EXPECT_EQ(replaceEnvPattern(R"(query: "SELECT * FROM @env(SEN_TEST_UNSET, fallback).myBus")"),
            R"(query: "SELECT * FROM fallback.myBus")");
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
/// Whitespace around the comma and before the closing parenthesis is accepted: this is the form the
/// documentation shows, and it used to leave the pattern in the configuration with no diagnostic.
TEST_F(AnEnvPattern, acceptsSpacesAroundTheComma)
{
  EXPECT_EQ(replaceEnvPattern("value: @env(SEN_TEST_UNSET, fallback)"), "value: fallback");
  EXPECT_EQ(replaceEnvPattern("value: @env(SEN_TEST_UNSET ,fallback)"), "value: fallback");
  EXPECT_EQ(replaceEnvPattern("value: @env(SEN_TEST_UNSET\t,\tfallback)"), "value: fallback");
  EXPECT_EQ(replaceEnvPattern("value: @env(SEN_TEST_VAR )"), "value: expanded");
}

/// @test
/// A default holds anything up to the closing parenthesis: a path, a host and port, a dotted
/// name. Word characters only did not merely lose the fallback, because the group is optional:
/// the whole pattern stopped matching and the variable went unread.
TEST_F(AnEnvPattern, acceptsADefaultThatIsNotAWord)
{
  EXPECT_EQ(replaceEnvPattern("path: @env(SEN_TEST_UNSET,/data/git-repo)"), "path: /data/git-repo");
  EXPECT_EQ(replaceEnvPattern("host: @env(SEN_TEST_UNSET, localhost:8080)"), "host: localhost:8080");
  EXPECT_EQ(replaceEnvPattern("bus: @env(SEN_TEST_UNSET,local.kernel)"), "bus: local.kernel");
  EXPECT_EQ(replaceEnvPattern("name: @env(SEN_TEST_UNSET,a-b)"), "name: a-b");
}

/// @test
/// A variable that is set wins over a default the expression could not read before. This is the
/// reported symptom: the variable existed and its pattern still reached the configuration as text.
TEST_F(AnEnvPattern, prefersTheVariableOverADefaultThatIsAPath)
{
  EXPECT_EQ(replaceEnvPattern("path: @env(SEN_TEST_VAR,/data/git-repo)"), "path: expanded");
}

/// @test
/// An empty default is how a variable is made optional with nothing behind it.
TEST_F(AnEnvPattern, acceptsAnEmptyDefault) { EXPECT_EQ(replaceEnvPattern("value: @env(SEN_TEST_UNSET,)"), "value: "); }

/// @test
/// The closing parenthesis ends the default, so text after it stays where it is.
TEST_F(AnEnvPattern, endsTheDefaultAtTheParenthesis)
{
  EXPECT_EQ(replaceEnvPattern("value: @env(SEN_TEST_UNSET,a)b"), "value: ab");
}

/// @test
/// Spaces between the default and the parenthesis belong to the separator, not to the value.
TEST_F(AnEnvPattern, dropsSpacesAfterTheDefault)
{
  EXPECT_EQ(replaceEnvPattern("value: @env(SEN_TEST_UNSET, fallback )"), "value: fallback");
}

/// @test
/// Text the expression cannot read is still left where it is, because a configuration that loads
/// today has to go on loading and a comment naming the pattern is legitimate.
TEST_F(AnEnvPattern, leavesAPatternItCannotReadAlone)
{
  for (const auto* text: {"value: @env(1BAD)", "value: @env()", "value: @env(SEN_TEST_VAR"})
  {
    EXPECT_EQ(replaceEnvPattern(text), text);
  }
}

/// @test
/// The pattern it could not read is named in a warning, so a mistyped default does not become
/// the value in silence.
TEST_F(AnEnvPattern, warnsAboutAPatternItCannotRead)
{
  const auto ring = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(8);
  const auto previous = spdlog::default_logger();
  const auto capture = std::make_shared<spdlog::logger>("env_substitution_capture", ring);
  capture->set_level(spdlog::level::warn);
  spdlog::set_default_logger(capture);

  static_cast<void>(replaceEnvPattern("value: @env(1BAD)"));

  spdlog::set_default_logger(previous);
  spdlog::drop("env_substitution_capture");

  const std::vector<std::string> logged = ring->last_formatted();
  ASSERT_EQ(logged.size(), 1U);
  EXPECT_NE(logged.front().find("@env(1BAD)"), std::string::npos);
}

/// @test
/// An escaped pattern asked for the text, so it draws no warning.
TEST_F(AnEnvPattern, staysQuietAboutAnEscapedPattern)
{
  const auto ring = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(8);
  const auto previous = spdlog::default_logger();
  const auto capture = std::make_shared<spdlog::logger>("env_substitution_quiet", ring);
  capture->set_level(spdlog::level::warn);
  spdlog::set_default_logger(capture);

  static_cast<void>(replaceEnvPattern(R"(value: \@env(1BAD))"));

  spdlog::set_default_logger(previous);
  spdlog::drop("env_substitution_quiet");

  EXPECT_TRUE(ring->last_formatted().empty());
}

/// @test
/// An escaped pattern is text, so one the expression cannot read is left alone and not refused.
TEST_F(AnEnvPattern, leavesAnEscapedPatternItCannotReadAlone)
{
  EXPECT_EQ(replaceEnvPattern(R"(value: \@env(1BAD))"), R"(value: \@env(1BAD))");
}

/// @test
/// An unset variable with no default stops the load rather than substituting nothing, and the error
/// names the variable.
TEST_F(AnEnvPattern, throwsWhenTheVariableIsUnsetAndHasNoDefault)
{
  try
  {
    static_cast<void>(replaceEnvPattern("value: @env(SEN_TEST_UNSET)"));
    FAIL() << "expected a throw for an unset variable with no default";
  }
  catch (const std::runtime_error& error)
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
/// A pattern cannot span lines, so a newline after the comma is not a match.
TEST_F(AnEnvPattern, doesNotMatchAcrossLines)
{
  const std::string text = "value: @env(SEN_TEST_VAR,\nfallback)";
  EXPECT_EQ(replaceEnvPattern(text), text);
}

/// @test
/// An included file gets the same expansion as the top-level one. It used to reach
/// the tree as literal text, so a pattern there never resolved and an unset variable
/// raised nothing.
///
/// It also depends, without meaning to, on text after a pattern surviving: the closing quote and
/// newline follow the match. That makes it a second witness to the behaviour above, so keep the
/// quoting rather than simplifying it.
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

/// @test
/// The include key takes a sequence as well as a single file, and every file in it gets the same
/// expansion. Only the single-file form was covered, so a pattern in a sequence could have
/// stopped resolving with nothing to notice.
TEST_F(AnEnvPattern, expandsPatternsInsideFilesIncludedAsASequence)
{
  const auto directory = std::filesystem::temp_directory_path() / "sen_env_include_sequence_test";
  std::filesystem::create_directories(directory);
  {
    std::ofstream first(directory / "first.yaml");
    first << "firstValue: \"@env(SEN_TEST_VAR)\"\n";
    std::ofstream second(directory / "second.yaml");
    second << "secondValue: \"@env(SEN_TEST_VAR)\"\n";
  }

  const std::string top = "include:\n  - ./first.yaml\n  - ./second.yaml\ntopValue: \"@env(SEN_TEST_VAR)\"\n";
  const sen::VarMap config = sen::kernel::getConfigAsVarFromYaml(top, directory / "main.yaml", false);

  EXPECT_EQ(config.at("topValue").getCopyAs<std::string>(), "expanded");
  EXPECT_EQ(config.at("firstValue").getCopyAs<std::string>(), "expanded");
  EXPECT_EQ(config.at("secondValue").getCopyAs<std::string>(), "expanded");

  std::filesystem::remove_all(directory);
}
