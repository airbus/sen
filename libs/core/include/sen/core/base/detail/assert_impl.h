// === assert_impl.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_DETAIL_ASSERT_IMPL_H
#define SEN_CORE_BASE_DETAIL_ASSERT_IMPL_H

#include "sen/core/base/source_location.h"

// std
#include <cstdint>
#include <functional>

namespace sen::impl
{

/// Represents the type of check being made.
enum class CheckType : std::uint8_t
{
  assert = 0,  ///< an intermediate result produced by a procedure (not an input or output).
  expect = 1,  ///< a pre-condition of a procedure (function parameter for example).
  ensure = 2,  ///< a post-condition of a procedure (function return value for example).
};

/// Groups the information of a check that is present in the code.
struct CheckInfo
{
  CheckType checkType;
  std::string_view expression;
  SourceLocation sourceLocation;
};

/// This function gets called when a check is failed.
/// It internally calls the currently set handler (@see setFailedCheckHandler).
using FailedCheckHandler = std::function<void(const CheckInfo&)>;

/// Sets the function to be called when a check fails.
/// @note This function is not part of the public Sen API and is meant to be
///       called only by the runtime or (sometimes) during testing.
/// @return the previously set handler function.
[[nodiscard]] FailedCheckHandler setFailedCheckHandler(FailedCheckHandler handler) noexcept;

/// This function is called by the Sen macros as an implementation detail.
/// @note The logic (although simple) is not directly implemented in the macro as
///       it would prevent decorating it with the coverage exclusions, which is
///       needed for assertions.
void senCheckImpl(bool checkResult,
                  CheckType checkType,
                  std::string_view expression,
                  const SourceLocation& sourceLocation) noexcept;

}  // namespace sen::impl

#endif  // SEN_CORE_BASE_DETAIL_ASSERT_IMPL_H
