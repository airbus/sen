// === assert.cpp ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/base/assert.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/detail/assert_impl.h"
#include "sen/core/base/source_location.h"

// cpptrace
#include "cpptrace/cpptrace.hpp"

// std
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>

namespace sen::impl
{

std::string CheckInfo::str() const noexcept
{
  std::stringstream representation;

  // insert case prefix
  switch (checkType_)
  {
    case sen::impl::CheckType::assert:
      representation << "assert";
      break;
    case sen::impl::CheckType::expect:
      representation << "expect";
      break;
    case sen::impl::CheckType::ensure:
      representation << "ensure";
      break;
  }

  // insert expression
  representation << " ==> [";
  representation << expression_;
  representation << "]";

  // insert location information
  representation << " in " << sourceLocation_.functionName;
  representation << " at " << sourceLocation_.fileName;
  representation << ":" << sourceLocation_.lineNumber;

  return representation.str();
}

[[noreturn]] void defaultCheckHandler(const CheckInfo& checkInfo) noexcept
{
  std::ignore = checkInfo;  // inspect this in your debugging session.
#if defined(DEBUG)
  std::cerr << "Err: " << checkInfo << std::endl;
#endif
  lastReportedAssertionError = checkInfo;
  std::raise(SIGABRT);
  SEN_UNREACHABLE();
}

FailedCheckHandler failedCheckHandler = defaultCheckHandler;  // NOLINT(cert-err58-cpp)

void handleFailedCheck(const CheckInfo& checkInfo) noexcept { failedCheckHandler(checkInfo); }

FailedCheckHandler setFailedCheckHandler(FailedCheckHandler handler) noexcept
{
  std::swap(failedCheckHandler, handler);
  return handler;
}

void senCheckImpl(bool checkResult,
                  CheckType checkType,
                  std::string_view expression,
                  const SourceLocation& sourceLocation) noexcept
{
  if (!checkResult)
  {
    const auto checkInfo = CheckInfo {checkType, expression, sourceLocation};
    handleFailedCheck(checkInfo);
  }
}

[[noreturn]] void terminateHandler()
{
  try
  {
    auto ptr = std::current_exception();
    if (ptr == nullptr)
    {
      fputs("Terminate called without an active exception\n", stderr);
    }
    else
    {
      std::rethrow_exception(ptr);
    }
  }
  catch (const cpptrace::exception& e)
  {
    fputs(e.message(), stderr);
    fputs("\n", stderr);
    e.trace().print(std::cerr, cpptrace::isatty(cpptrace::stderr_fileno));
  }
  catch (const std::logic_error& e)
  {
    fputs("Terminate called after throwing an instance of std::logic_error: ", stderr);
    fputs(e.what(), stderr);
    fputs("\n", stderr);
  }
  catch (const std::runtime_error& e)
  {
    fputs("Terminate called after throwing an instance of std::runtime_error: ", stderr);
    fputs(e.what(), stderr);
    fputs("\n", stderr);
  }
  catch (const std::exception& e)
  {
    fputs("Terminate called after throwing an instance of std::exception: ", stderr);
    fputs(e.what(), stderr);
    fputs("\n", stderr);
  }
  catch (...)
  {
    fputs("Terminate called after throwing an instance of an unknown exception", stderr);
  }

  fflush(stderr);
  std::abort();
}

}  // namespace sen::impl

namespace sen
{

void registerTerminateHandler() { std::set_terminate(sen::impl::terminateHandler); }

void throwRuntimeError(const std::string& err)
{
#ifndef NDEBUG
  throw cpptrace::runtime_error(err.c_str());
#else
  throw std::runtime_error(err.c_str());
#endif
}

void trace() { cpptrace::generate_trace().print(std::cerr, cpptrace::isatty(cpptrace::stderr_fileno)); }

void trace(std::string preMessage)
{
  preMessage.append("\n");

  // We first need to combine the string with the preMessage before we print the message, as otherwise the two might get
  // wrongly interleaved with other output.
  std::cerr << preMessage.append(cpptrace::generate_trace().to_string(cpptrace::isatty(cpptrace::stderr_fileno)))
            << std::endl;
}

}  // namespace sen
