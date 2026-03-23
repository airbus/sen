// === quantity.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_QUANTITY_H
#define SEN_CORE_BASE_QUANTITY_H

// sen
#include "compiler_macros.h"
#include "sen/core/base/assert.h"
#include "sen/core/base/class_helpers.h"

// std
#include <string>

namespace sen
{

/// \addtogroup util
/// @{

/// CRTP wrapper that constrains a value of type `T` to the `[D::min, D::max]` range.
/// Use `SEN_RANGED_QUANTITY` to define concrete range-checked types, or
/// `SEN_NON_RANGED_QUANTITY` for unconstrained quantities.
/// @tparam T Underlying arithmetic type.
/// @tparam D Derived type (CRTP); must expose `hasRange`, and optionally `min`/`max`.
template <typename T, typename D>
class Quantity
{
public:
  using ValueType = T;

public:  // special members
  constexpr SEN_MOVE_CONSTRUCT(Quantity) = default;
  constexpr SEN_COPY_CONSTRUCT(Quantity) = default;
  constexpr SEN_MOVE_ASSIGN(Quantity) = default;
  constexpr SEN_COPY_ASSIGN(Quantity) = default;

public:
  /// Default-constructs the internal value. Lower bound is used if not within range.
  constexpr Quantity() noexcept;

  /// Sets the value. Throws std::exception if out of range.
  template <typename U, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  constexpr Quantity(U value)  // NOLINT implicit conversions allowed
  {
    checkAndSet(value);
  }

  ~Quantity() = default;

public:  // getters and setters
  /// @return The stored value by copy.
  [[nodiscard]] constexpr T get() const noexcept { return value_; }

  /// Implicit conversion to `T`.
  /// @return The stored value.
  constexpr operator T() const noexcept { return value_; }  // NOLINT implicit conversions allowed

  /// @return `true` if the stored value is marked as valid (see `setValid()`).
  [[nodiscard]] constexpr bool isValid() const noexcept { return validity_; }

  /// Explicit boolean conversion (same as `isValid()`).
  constexpr explicit operator bool() const noexcept { return validity_; }

  /// Assigns a new value, checking the range constraint.
  /// @param other New value; must be within `[D::min, D::max]` when `D::hasRange` is `true`.
  /// @throws std::exception if the value is out of range.
  template <typename U, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  constexpr Quantity& operator=(U other)
  {
    checkAndSet(other);
    return *this;
  }

  /// Sets the stored value, checking the range constraint.
  /// @param other New value; must be within `[D::min, D::max]` when `D::hasRange` is `true`.
  /// @throws std::exception if the value is out of range.
  template <typename U, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  constexpr void set(U other)
  {
    checkAndSet(other);
  }

  /// Overrides the validity flag without changing the stored value.
  /// @param valid New validity state.
  constexpr void setValid(bool valid) { validity_ = valid; }

public:  // comparison operators
  template <typename U, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  constexpr bool operator==(const U& other) const
  {
    return value_ == other;
  }

  template <typename U, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  constexpr bool operator!=(const U& other) const
  {
    return value_ != other;
  }

  template <typename U, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  constexpr bool operator<(const U& other) const
  {
    return value_ < other;
  }

  template <typename U, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  constexpr bool operator<=(const U& other) const
  {
    return value_ <= other;
  }

  template <typename U, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  constexpr bool operator>(const U& other) const
  {
    return value_ > other;
  }

  template <typename U, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  constexpr bool operator>=(const U& other) const
  {
    return value_ >= other;
  }

private:
  SEN_ALWAYS_INLINE void checkAndSet(T value);

private:
  T value_ = {};
  bool validity_ = true;
};

template <typename T, typename D>
struct ShouldBePassedByValue<Quantity<T, D>>: std::true_type
{
};

/// Use this macro to define types for values that shall stay within some [min, max] range.
/// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SEN_RANGED_QUANTITY(class_name, value_type, min_value, max_value)                                              \
  struct class_name: public sen::Quantity<value_type, class_name>                                                      \
  {                                                                                                                    \
    static constexpr bool hasRange = true;                                                                             \
    static constexpr value_type min = min_value;                                                                       \
    static constexpr value_type max = max_value;                                                                       \
    static_assert(min <= max);                                                                                         \
                                                                                                                       \
    using Quantity::Quantity;                                                                                          \
  };

/// Use this macro to define types for quantities that specify no range
/// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define SEN_NON_RANGED_QUANTITY(class_name, value_type)                                                                \
  struct class_name: public sen::Quantity<value_type, class_name>                                                      \
  {                                                                                                                    \
    static constexpr bool hasRange = false;                                                                            \
    using Quantity::Quantity;                                                                                          \
  };

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

template <typename T, typename D>
constexpr Quantity<T, D>::Quantity() noexcept
{
  if constexpr (D::hasRange)
  {
    if ((T {} < D::min) || (T {} > D::max))
    {
      value_ = D::min;
    }
  }
}

template <typename T, typename D>
void Quantity<T, D>::checkAndSet(T value)
{
  if constexpr (D::hasRange)
  {
    if ((value < D::min) || (value > D::max))
    {
      std::string err;
      err.append(std::to_string(value));
      err.append(" is out of the range [");
      err.append(std::to_string(D::min));
      err.append(", ");
      err.append(std::to_string(D::max));
      err.append("]");
      throwRuntimeError(err);
      SEN_UNREACHABLE();
    }
  }
  value_ = value;
}

}  // namespace sen

#endif  // SEN_CORE_BASE_QUANTITY_H
