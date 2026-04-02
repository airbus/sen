// === assert.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_ASSERT_H
#define SEN_CORE_BASE_ASSERT_H

#include "sen/core/base/detail/assert_impl.h"

// std
#include <string>

namespace sen
{

/// \file assert.h
/// The following macros implement a replacement of `assert` that is connected to the overall
/// fault handling mechanism present in the runtime. It avoids conditional compilation (debug
/// vs no debug). These macros are very rough and simple approximation to the idea of contracts,
/// as they allow expressing pre-conditions, post-conditions and invariants and perform some
/// checks during execution. Take into account that a failed 'expect', 'assert' or 'ensure'
/// indicate a bug in the software and means that the process is in a state that was not reasoned
/// about when writing the code. For most applications this implies that no decision
/// can be made at that point and therefore the usual approach is to terminate or restart.
///
/// @note the checks performed by `SEN_EXPECT` `SEN_ASSERT` and `SEN_ENSURE` will always
///       be performed, regardless of compile flags.

/// \addtogroup error_handling
/// @{

/// Checks a pre-condition of a procedure (function parameter for example). NOLINTNEXTLINE
#define SEN_EXPECT(expr) ::sen::impl::senCheckImpl((expr), ::sen::impl::CheckType::expect, #expr, SEN_SL())

/// Checks an intermediate result produced by a procedure (not an input or output). NOLINTNEXTLINE
#define SEN_ASSERT(expr) ::sen::impl::senCheckImpl((expr), ::sen::impl::CheckType::assert, #expr, SEN_SL())

/// Checks an intermediate result produced by a procedure (not an input or output). NOLINTNEXTLINE
#if defined(DEBUG)
#  define SEN_DEBUG_ASSERT(expr) ::sen::impl::senCheckImpl((expr), ::sen::impl::CheckType::assert, #expr, SEN_SL())
#else
#  define SEN_DEBUG_ASSERT(expr)
#endif

/// Checks a post-condition of a procedure (function return value for example).  NOLINTNEXTLINE
#define SEN_ENSURE(expr) ::sen::impl::senCheckImpl((expr), ::sen::impl::CheckType::ensure, #expr, SEN_SL())

/// Installs a custom termination handler.
void registerTerminateHandler();

/// Throws std::exception that attempts to collect the stack trace. We also wrap it to avoid including stdexcept.
[[noreturn]] void throwRuntimeError(const std::string& err);

/// Prints the current stack trace to stderr.
void trace();

/// Prints the current stack trace to stderr.
///
/// @param preMessage: that is printed in the line before the stacktrace
void trace(std::string preMessage);

/// @}

}  // namespace sen

#endif  // SEN_CORE_BASE_ASSERT_H
