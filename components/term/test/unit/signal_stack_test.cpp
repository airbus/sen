// === signal_stack_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "signal_stack.h"

// google test
#include <gtest/gtest.h>

#if !defined(_WIN32)
// std
#  include <csignal>
#endif

namespace sen::components::term
{
namespace
{

#if !defined(_WIN32)

void noopHandler(int /*signalNumber*/) {}

/// Installs a handler the way FTXUI does -- SA_RESTART and no SA_ONSTACK -- and puts back whatever was
/// there when the test ends, so one test cannot change how the rest of the binary handles a fault.
class FtxuiStyleHandler
{
public:
  explicit FtxuiStyleHandler(int signalNumber): signalNumber_(signalNumber)
  {
    struct sigaction replacement {};
    replacement.sa_handler = noopHandler;
    replacement.sa_flags = SA_RESTART;
    sigemptyset(&replacement.sa_mask);
    EXPECT_EQ(::sigaction(signalNumber_, &replacement, &saved_), 0);
  }

  ~FtxuiStyleHandler() { EXPECT_EQ(::sigaction(signalNumber_, &saved_, nullptr), 0); }

  FtxuiStyleHandler(const FtxuiStyleHandler&) = delete;
  FtxuiStyleHandler& operator=(const FtxuiStyleHandler&) = delete;
  FtxuiStyleHandler(FtxuiStyleHandler&&) = delete;
  FtxuiStyleHandler& operator=(FtxuiStyleHandler&&) = delete;

  [[nodiscard]] static bool altStackFlagIsSet(int signalNumber)
  {
    struct sigaction current {};
    EXPECT_EQ(::sigaction(signalNumber, nullptr, &current), 0);
    return (current.sa_flags & SA_ONSTACK) != 0;
  }

  [[nodiscard]] static void* handlerOf(int signalNumber)
  {
    struct sigaction current {};
    EXPECT_EQ(::sigaction(signalNumber, nullptr, &current), 0);
    return reinterpret_cast<void*>(current.sa_handler);  // NOLINT(*-reinterpret-cast) -- identity only
  }

private:
  int signalNumber_;
  struct sigaction saved_ {};
};

TEST(SignalStack, PutsTheAltStackFlagBackWithoutChangingTheHandler)
{
  // The control is the first assertion: a handler installed the way FTXUI installs its own really does
  // arrive without SA_ONSTACK, which is what silently disables stack-overflow reporting.
  FtxuiStyleHandler installed(SIGSEGV);
  ASSERT_FALSE(FtxuiStyleHandler::altStackFlagIsSet(SIGSEGV))
    << "the premise is wrong: this handler already had SA_ONSTACK, so the fix could not be measured";
  void* before = FtxuiStyleHandler::handlerOf(SIGSEGV);

  EXPECT_GT(restoreAltStackOnFatalHandlers(), 0) << "nothing was changed";

  EXPECT_TRUE(FtxuiStyleHandler::altStackFlagIsSet(SIGSEGV)) << "the flag was not put back";
  EXPECT_EQ(FtxuiStyleHandler::handlerOf(SIGSEGV), before)
    << "the handler itself changed; FTXUI's terminal restore and its uninstall depend on it";
}

TEST(SignalStack, IsIdempotent)
{
  // Called once per init(), and a component can be loaded more than once in a process.
  FtxuiStyleHandler installed(SIGSEGV);
  std::ignore = restoreAltStackOnFatalHandlers();
  const int secondPass = restoreAltStackOnFatalHandlers();
  EXPECT_TRUE(FtxuiStyleHandler::altStackFlagIsSet(SIGSEGV));
  EXPECT_GE(secondPass, 0) << "a second call must not fail";
}

#else

TEST(SignalStack, IsANoOpOnWindows) { EXPECT_EQ(restoreAltStackOnFatalHandlers(), 0); }

#endif

}  // namespace
}  // namespace sen::components::term
