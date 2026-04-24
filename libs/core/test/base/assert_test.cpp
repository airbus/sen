// === assert_test.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/assert.h"

// sen
#include "sen/core/base/detail/assert_impl.h"
#include "sen/core/base/source_location.h"

// google test
#include <gtest/gtest.h>

// std
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>

using sen::impl::CheckInfo;
using sen::impl::CheckType;
using sen::impl::FailedCheckHandler;
using sen::impl::setFailedCheckHandler;

namespace
{

[[nodiscard]] CheckInfo& getExpectedCheckInfo()
{
  static CheckInfo expectedCheckInfo = {CheckType::assert, "", SEN_SL()};
  return expectedCheckInfo;
}

std::size_t callCount = 0U;

void testAssertHandler(const CheckInfo& checkInfo) noexcept
{
  callCount++;

  auto& expectedCheckInfo = getExpectedCheckInfo();
  ASSERT_EQ(checkInfo.getCheckType(), expectedCheckInfo.getCheckType());
  ASSERT_EQ(checkInfo.getSourceLocation().lineNumber, expectedCheckInfo.getSourceLocation().lineNumber + 1);
  ASSERT_EQ(checkInfo.getSourceLocation().fileName, expectedCheckInfo.getSourceLocation().fileName);
  ASSERT_EQ(checkInfo.getExpression(), expectedCheckInfo.getExpression());
}

void exitOnAbort(int /*signal*/) { std::exit(0); }

[[noreturn]] void triggerDefaultCheckHandler()
{
  std::signal(SIGABRT, exitOnAbort);
  SEN_ASSERT(false);
  std::exit(1);
}

[[noreturn]] void triggerTerminateNoException()
{
  sen::registerTerminateHandler();
  std::signal(SIGABRT, exitOnAbort);
  std::terminate();
}

[[noreturn]] void triggerTerminateLogicError()
{
  sen::registerTerminateHandler();
  std::signal(SIGABRT, exitOnAbort);
  try
  {
    throw std::logic_error("mock_logic_error");
  }
  catch (...)
  {
    std::terminate();
  }
}

[[noreturn]] void triggerTerminateRuntimeError()
{
  sen::registerTerminateHandler();
  std::signal(SIGABRT, exitOnAbort);
  try
  {
    throw std::runtime_error("mock_runtime_error");
  }
  catch (...)
  {
    std::terminate();
  }
}

[[noreturn]] void triggerTerminateStdException()
{
  sen::registerTerminateHandler();
  std::signal(SIGABRT, exitOnAbort);
  try
  {
    throw std::exception();
  }
  catch (...)
  {
    std::terminate();
  }
}

[[noreturn]] void triggerTerminateCpptraceException()
{
  sen::registerTerminateHandler();
  std::signal(SIGABRT, exitOnAbort);
  try
  {
    sen::throwRuntimeError("mock_cpptrace_error");
  }
  catch (...)
  {
    std::terminate();
  }
}

struct UnknownMockException
{
};

[[noreturn]] void triggerTerminateUnknownException()
{
  sen::registerTerminateHandler();
  std::signal(SIGABRT, exitOnAbort);
  try
  {
    throw UnknownMockException {};  // NOLINT (hicpp-exception-baseclass)
  }
  catch (...)
  {
    std::terminate();
  }
}

class AssertTest: public testing::Test
{
protected:
  void SetUp() override
  {
    oldHandler = setFailedCheckHandler(testAssertHandler);
    ASSERT_NE(oldHandler, nullptr);
    prevCallCount = callCount;
  }

  void TearDown() override { std::ignore = setFailedCheckHandler(oldHandler); }

  FailedCheckHandler oldHandler = nullptr;  // NOLINT(misc-non-private-member-variables-in-classes)
  std::size_t prevCallCount = 0U;           // NOLINT(misc-non-private-member-variables-in-classes)
};

}  // namespace

/// @test
/// Check expect type macro triggering
/// @requirements(SEN-1049)
TEST_F(AssertTest, macros_triggering_expect)
{
  auto& expectedCheckInfo = getExpectedCheckInfo();

  expectedCheckInfo = {CheckType::expect, "false", SEN_SL()};
  SEN_EXPECT(false);

  ASSERT_EQ(callCount, prevCallCount + 1);
}

/// @test
/// Check ensure type macro triggering
/// @requirements(SEN-1049)
TEST_F(AssertTest, macros_triggering_ensure)
{
  auto& expectedCheckInfo = getExpectedCheckInfo();

  expectedCheckInfo = {CheckType::ensure, "false", SEN_SL()};
  SEN_ENSURE(false);

  ASSERT_EQ(callCount, prevCallCount + 1);
}

/// @test
/// Check assert type macro triggering
/// @requirements(SEN-1049)
TEST_F(AssertTest, macros_triggering_assert)
{
  auto& expectedCheckInfo = getExpectedCheckInfo();

  expectedCheckInfo = {CheckType::assert, "false", SEN_SL()};
  SEN_ASSERT(false);

  ASSERT_EQ(callCount, prevCallCount + 1);
}

/// @test
/// Check debug assert macro triggering
/// @requirements(SEN-1049)
TEST_F(AssertTest, macros_triggering_debug_assert)
{
#if defined(DEBUG)
  auto& expectedCheckInfo = getExpectedCheckInfo();
  expectedCheckInfo = {CheckType::assert, "false", SEN_SL()};
  SEN_DEBUG_ASSERT(false);
  ASSERT_EQ(callCount, prevCallCount + 1);
#else
  SEN_DEBUG_ASSERT(false);
  ASSERT_EQ(callCount, prevCallCount);
#endif
}

/// @test
/// Check not triggering macros
/// @requirements(SEN-1049)
TEST_F(AssertTest, macros_not_triggering)
{
  SEN_EXPECT(true);
  SEN_ENSURE(true);
  SEN_ASSERT(true);
  SEN_DEBUG_ASSERT(true);

  ASSERT_EQ(callCount, prevCallCount);
}

/// @test
/// Check default check handler aborts execution
/// @requirements(SEN-908)
TEST(AssertDeathTest, default_handler_aborts)
{
  EXPECT_EXIT(triggerDefaultCheckHandler(), ::testing::ExitedWithCode(0), "");
}

/// @test
/// Check terminate handler without an active exception
/// @requirements(SEN-908)
TEST(AssertDeathTest, terminate_handler_no_active_exception)
{
  EXPECT_EXIT(
    triggerTerminateNoException(), ::testing::ExitedWithCode(0), "Terminate called without an active exception");
}

/// @test
/// Check terminate handler captures and prints std::logic_error
/// @requirements(SEN-908)
TEST(AssertDeathTest, terminate_handler_logic_error)
{
  EXPECT_EXIT(triggerTerminateLogicError(),
              ::testing::ExitedWithCode(0),
              "Terminate called after throwing an instance of std::logic_error: mock_logic_error");
}

/// @test
/// Check terminate handler captures and prints std::runtime_error
/// @requirements(SEN-908)
TEST(AssertDeathTest, terminate_handler_runtime_error)
{
  EXPECT_EXIT(triggerTerminateRuntimeError(),
              ::testing::ExitedWithCode(0),
              "Terminate called after throwing an instance of std::runtime_error: mock_runtime_error");
}

/// @test
/// Check terminate handler captures and prints base std::exception
/// @requirements(SEN-908)
TEST(AssertDeathTest, terminate_handler_standard_exception)
{
  EXPECT_EXIT(triggerTerminateStdException(),
              ::testing::ExitedWithCode(0),
              "Terminate called after throwing an instance of std::exception");
}

/// @test
/// Check terminate handler handles custom cpptrace exception via throwRuntimeError
/// @requirements(SEN-908)
TEST(AssertDeathTest, terminate_handler_cpptrace_exception)
{
  EXPECT_EXIT(triggerTerminateCpptraceException(), ::testing::ExitedWithCode(0), "mock_cpptrace_error");
}

/// @test
/// Check terminate handler handles unknown exceptions
/// @requirements(SEN-908)
TEST(AssertDeathTest, terminate_handler_unknown_exception)
{
  EXPECT_EXIT(triggerTerminateUnknownException(),
              ::testing::ExitedWithCode(0),
              "Terminate called after throwing an instance of an unknown exception");
}

/// @test
/// Check trace function outputs data to stderr
/// @requirements(SEN-1049)
TEST_F(AssertTest, trace_outputs_to_stderr)
{
  std::stringstream buffer;
  std::streambuf* oldStderr = std::cerr.rdbuf(buffer.rdbuf());

  sen::trace();

  std::cerr.rdbuf(oldStderr);
  ASSERT_FALSE(buffer.str().empty());
}

/// @test
/// Check CheckInfo string representation for all types
/// @requirements(SEN-1049)
TEST_F(AssertTest, check_info_string_conversion)
{
  constexpr sen::SourceLocation loc {"file", 42, "func"};

  const CheckInfo infoAssert {CheckType::assert, "expr", loc};
  EXPECT_NE(infoAssert.str().find("assert"), std::string::npos);
  EXPECT_NE(infoAssert.str().find("expr"), std::string::npos);

  const CheckInfo infoExpect {CheckType::expect, "expr", loc};
  EXPECT_NE(infoExpect.str().find("expect"), std::string::npos);

  const CheckInfo infoEnsure {CheckType::ensure, "expr", loc};
  EXPECT_NE(infoEnsure.str().find("ensure"), std::string::npos);
}
