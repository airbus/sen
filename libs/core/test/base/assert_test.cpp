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

// _Exit rather than exit: this runs inside a signal handler, and exit is not async-signal-safe --
// it runs the at-exit chain, so static destructors free memory in the signal context.
void exitOnAbort(int /*signal*/) { std::_Exit(0); }

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
/// SEN_EXPECT(false) calls the installed failure handler once, reporting the expect check type,
/// the expression text, and the file and line of the macro.
/// @requirements(SEN-1049)
TEST_F(AssertTest, macros_triggering_expect)
{
  auto& expectedCheckInfo = getExpectedCheckInfo();

  expectedCheckInfo = {CheckType::expect, "false", SEN_SL()};
  SEN_EXPECT(false);

  ASSERT_EQ(callCount, prevCallCount + 1);
}

/// @test
/// SEN_ENSURE(false) calls the installed failure handler once, reporting the ensure check type,
/// the expression text, and the file and line of the macro.
/// @requirements(SEN-1049)
TEST_F(AssertTest, macros_triggering_ensure)
{
  auto& expectedCheckInfo = getExpectedCheckInfo();

  expectedCheckInfo = {CheckType::ensure, "false", SEN_SL()};
  SEN_ENSURE(false);

  ASSERT_EQ(callCount, prevCallCount + 1);
}

/// @test
/// SEN_ASSERT(false) calls the installed failure handler once, reporting the assert check type,
/// the expression text, and the file and line of the macro.
/// @requirements(SEN-1049)
TEST_F(AssertTest, macros_triggering_assert)
{
  auto& expectedCheckInfo = getExpectedCheckInfo();

  expectedCheckInfo = {CheckType::assert, "false", SEN_SL()};
  SEN_ASSERT(false);

  ASSERT_EQ(callCount, prevCallCount + 1);
}

/// @test
/// SEN_DEBUG_ASSERT(false) calls the failure handler in a DEBUG build, and in any other build is
/// compiled out and leaves the handler uncalled.
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
/// A true condition leaves the failure handler uncalled for SEN_EXPECT, SEN_ENSURE, SEN_ASSERT
/// and SEN_DEBUG_ASSERT.
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
/// The default failed-check handler aborts the process: a failing SEN_ASSERT raises SIGABRT
/// rather than letting execution continue.
/// @requirements(SEN-908)
TEST(AssertDeathTest, default_handler_aborts)
{
  EXPECT_EXIT(triggerDefaultCheckHandler(), ::testing::ExitedWithCode(0), "");
}

/// @test
/// The registered terminate handler prints "Terminate called without an active exception" when
/// nothing is in flight, then aborts.
/// @requirements(SEN-908)
TEST(AssertDeathTest, terminate_handler_no_active_exception)
{
  EXPECT_EXIT(
    triggerTerminateNoException(), ::testing::ExitedWithCode(0), "Terminate called without an active exception");
}

/// @test
/// The registered terminate handler prints the type and message of an in-flight
/// std::logic_error, then aborts.
/// @requirements(SEN-908)
TEST(AssertDeathTest, terminate_handler_logic_error)
{
  EXPECT_EXIT(triggerTerminateLogicError(),
              ::testing::ExitedWithCode(0),
              "Terminate called after throwing an instance of std::logic_error: mock_logic_error");
}

/// @test
/// The registered terminate handler prints the type and message of an in-flight
/// std::runtime_error, then aborts.
/// @requirements(SEN-908)
TEST(AssertDeathTest, terminate_handler_runtime_error)
{
  EXPECT_EXIT(triggerTerminateRuntimeError(),
              ::testing::ExitedWithCode(0),
              "Terminate called after throwing an instance of std::runtime_error: mock_runtime_error");
}

/// @test
/// The registered terminate handler prints the type of an in-flight plain std::exception, then
/// aborts.
/// @requirements(SEN-908)
TEST(AssertDeathTest, terminate_handler_standard_exception)
{
  EXPECT_EXIT(triggerTerminateStdException(),
              ::testing::ExitedWithCode(0),
              "Terminate called after throwing an instance of std::exception");
}

/// @test
/// The registered terminate handler prints the message of an in-flight exception thrown via
/// throwRuntimeError, then aborts.
/// @requirements(SEN-908)
TEST(AssertDeathTest, terminate_handler_cpptrace_exception)
{
  EXPECT_EXIT(triggerTerminateCpptraceException(), ::testing::ExitedWithCode(0), "mock_cpptrace_error");
}

/// @test
/// The registered terminate handler reports an in-flight exception outside the std::exception
/// family as an unknown exception, then aborts.
/// @requirements(SEN-908)
TEST(AssertDeathTest, terminate_handler_unknown_exception)
{
  EXPECT_EXIT(triggerTerminateUnknownException(),
              ::testing::ExitedWithCode(0),
              "Terminate called after throwing an instance of an unknown exception");
}

/// @test
/// sen::trace writes a non-empty report to stderr.
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
/// CheckInfo::str names the check type for assert, expect and ensure, and includes the failing
/// expression.
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
