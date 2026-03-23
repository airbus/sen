// === mutex_utils.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_MUTEX_UTILS_H
#define SEN_CORE_BASE_MUTEX_UTILS_H

// sen
#include "sen/core/base/compiler_macros.h"

// std
#include <functional>
#include <mutex>

namespace sen
{

/// Guarded is a lightweight wrapper class that can be used to protect an objet of type T from concurrent
/// modification. The wrapper only allows access/reads/modifications of the underlying value while holding a lock but
/// does not require the user to hold the lock himself.
///
/// Example:
/// @code{.cpp}
///   struct MyClass
///   {
///     void add(int val)
///     {
///       protectedInt += val; // automatically holds a lock while modifying the protected int
///     }
///
///     Guarded<int> protectedInt;
///   };
/// @endcode
///
/// @note Guarded cannot guarantee that no lock-order inversion occurs, so even with the wrapper the
/// developer still needs to ensure that concurrent reads/writes do not access different wrappers or other mutexes in
/// different orders.
///
/// Furthermore, keep in mind that the best concurrent code is the one that is designed in a way to work without locked
/// values.
///
/// @tparam T the type of the underlying protected value
/// @tparam MutexType the type of the mutex that should be used
template <typename T, typename MutexType = std::mutex>
struct Guarded
{
  using UnderlyingValueTypeOfT = std::remove_reference_t<T>;

  /// Constructs the protected value in-place, forwarding all arguments to its constructor.
  /// @param args  Arguments forwarded to the constructor of `T`.
  template <typename... TArgs>
  explicit Guarded(TArgs... args): value_(std::forward<TArgs>(args)...)
  {
  }

  /// Returns a copy of the protected value.
  /// @return A copy of the underlying value, acquired under the mutex.
  UnderlyingValueTypeOfT getValue() const
  {
    std::lock_guard lock(mutex_);
    return value_;
  }

public:  // Operators
  // NOLINTNEXTLINE(hicpp-explicit-conversions): intended to be used as a wrapper
  operator UnderlyingValueTypeOfT() const { return getValue(); }

  /// Assigns a new value to the protected object under the mutex.
  /// @param newValue  New value to assign; must be convertible to `T`.
  /// @return Reference to this `Guarded` instance.
  template <typename U, std::enable_if_t<std::is_convertible_v<U, T>, bool> = true>
  Guarded& operator=(U&& newValue)
  {
    std::lock_guard lock(mutex_);
    value_ = std::forward<U>(newValue);
    return *this;
  }

  /// Returns the sum of the protected value and @p rhs under the mutex.
  /// @param rhs  Right-hand side operand.
  /// @return `value_ + rhs`.
  template <typename U>
  T operator+(U&& rhs) const
  {
    std::lock_guard lock(mutex_);
    return value_ + rhs;
  }

  /// Adds @p rhs to the protected value under the mutex.
  /// @param rhs  Value to add.
  /// @return Reference to this `Guarded` instance.
  template <typename U>
  Guarded& operator+=(U&& rhs)
  {
    std::lock_guard lock(mutex_);
    value_ += rhs;
    return *this;
  }

public:  // Access token handling
  /// Access token that enables the user to interact with the protected object
  /// while simultaneously holding a lock to ensure access are correctly
  /// synchronized.
  ///
  /// Note: The TemporaryAccessToken should only be held as a temporary or
  /// stored for short periods of time, to ensure that the underlying object
  /// can be accessed by other threads.
  struct TemporaryAccessToken
  {
    /// @param valuePtr  Pointer to the protected value; must not be null.
    /// @param m         Mutex to lock for the lifetime of this token.
    TemporaryAccessToken(T* valuePtr, MutexType& m): valuePtr_(valuePtr), lock_(m) {}

    std::conditional_t<std::is_pointer_v<T>, T&, T*> operator->()
    {
      if constexpr (std::is_pointer_v<T>)
      {
        return *valuePtr_;
      }
      else
      {
        return valuePtr_;
      }
    }

  private:
    T* valuePtr_;
    std::lock_guard<MutexType> lock_;
  };

  /// Returns a `TemporaryAccessToken` that locks the mutex and exposes `operator->` on the value.
  TemporaryAccessToken operator->() { return createAccessToken(); }

  /// Creates and returns a `TemporaryAccessToken` that holds the mutex for its lifetime.
  /// @return Token providing scoped access to the protected value.
  [[nodiscard]] TemporaryAccessToken createAccessToken() { return TemporaryAccessToken(&value_, mutex_); }

public:
  /// Immediately invokes the given callable while holding a block, passing in the protected value.
  ///
  /// @param callable that will be invoke with the protected value
  template <typename CallableType>
  auto invoke(CallableType callable)
  {
    std::lock_guard lock(mutex_);
    return std::invoke(callable, value_);
  }

public:  // Enable comparisions between wrapped and unwrapped types
  template <
    typename LHSType,
    typename RHSType,
    std::enable_if_t<std::disjunction_v<std::is_same<LHSType, Guarded>, std::is_same<RHSType, Guarded>>, bool> = true>
  friend inline bool operator==(const LHSType& lhs, const RHSType& rhs) noexcept
  {
    if constexpr (std::is_same_v<LHSType, Guarded> && std::is_same_v<RHSType, Guarded>)
    {
      return lhs.getValue() == rhs.getValue();
    }
    else if constexpr (std::is_same_v<LHSType, Guarded>)
    {
      return lhs.getValue() == rhs;
    }
    else
    {
      return lhs == rhs.getValue();
    }

    SEN_UNREACHABLE();
    return false;
  }

  template <
    typename LHSType,
    typename RHSType,
    std::enable_if_t<std::disjunction_v<std::is_same<LHSType, Guarded>, std::is_same<RHSType, Guarded>>, bool> = true>
  friend inline bool operator!=(const LHSType& lhs, const RHSType& rhs) noexcept
  {
    return !(lhs == rhs);
  }

private:
  T value_;
  mutable MutexType mutex_;
};

}  // namespace sen

#endif  // SEN_CORE_BASE_MUTEX_UTILS_H
