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

/// Creates a `ScopeGuard` that invokes `f` when the returned guard goes out of scope.
/// Only rvalue callables are accepted to prevent accidental copies.
/// @tparam F Callable type taking no arguments (deduced).
/// @param f  Callable to invoke on destruction; must be an rvalue reference.
/// @return A `ScopeGuard<F>` that will call `f` on destruction.
template <class F>
[[nodiscard]] auto makeScopeGuard(F&& f);

/// RAII wrapper that executes a callable when the guard goes out of scope.
///
/// Use `makeScopeGuard()` to create instances â€” the constructor is private.
/// The guard is non-copyable and non-movable; use it only as a local variable.
///
/// @tparam F No-argument callable type to invoke on destruction.
template <typename F>
class [[nodiscard]] ScopeGuard
{
  SEN_NOCOPY_NOMOVE(ScopeGuard)

public:  // special members
  ScopeGuard() = delete;

public:
  /// Invokes the stored callable unconditionally.
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
