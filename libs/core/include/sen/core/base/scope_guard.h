// === scope_guard.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_SCOPE_GUARD_H
#define SEN_CORE_BASE_SCOPE_GUARD_H

#include "sen/core/base/class_helpers.h"

namespace sen
{

/// \addtogroup util
/// @{

/// Makes scope guard from a callable taking no arguments.
template <class F>
[[nodiscard]] auto makeScopeGuard(F&& f);

/// Runs the function object F on destruction
template <typename F>
class [[nodiscard]] ScopeGuard
{
  SEN_NOCOPY_NOMOVE(ScopeGuard)

public:  // special members
  ScopeGuard() = delete;

public:
  /// Will trigger the given callback.
  ~ScopeGuard() noexcept { f_(); }

private:
  explicit ScopeGuard(F&& f): f_(std::move(f)) {}

  template <class InputF>
  friend auto makeScopeGuard(InputF&& f);

private:
  F f_;
};

template <class F>
ScopeGuard(F) -> ScopeGuard<F>;

// Deleted overloads for making scope guards.
// The function must be called on a rvalue reference

template <class F>
auto makeScopeGuard(const F&& f) = delete;

template <class F>
auto makeScopeGuard(F& f) = delete;

template <class F>
auto makeScopeGuard(const F& f) = delete;

/// @}

template <typename InputF>
auto makeScopeGuard(InputF&& f)
{
  return ScopeGuard {std::forward<InputF>(f)};
}

}  // namespace sen

#endif  // SEN_CORE_BASE_SCOPE_GUARD_H
