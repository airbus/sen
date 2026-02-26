// === strong_type.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_STRONG_TYPE_H
#define SEN_CORE_BASE_STRONG_TYPE_H

#include "sen/core/base/class_helpers.h"

namespace sen
{

/// \addtogroup util
/// @{

/// Wraps T to make it a strong type.
template <typename T>
class StrongTypeBase
{
public:  // requirements
  static_assert(std::is_nothrow_default_constructible_v<T>);
  static_assert(std::is_nothrow_copy_constructible_v<T>);
  static_assert(std::is_nothrow_move_constructible_v<T>);
  static_assert(std::is_nothrow_copy_assignable_v<T>);
  static_assert(std::is_nothrow_move_assignable_v<T>);

public:
  using ValueType = T;

public:  // special members
  constexpr SEN_MOVE_CONSTRUCT(StrongTypeBase) = default;
  constexpr SEN_COPY_CONSTRUCT(StrongTypeBase) = default;
  constexpr SEN_MOVE_ASSIGN(StrongTypeBase) = default;
  constexpr SEN_COPY_ASSIGN(StrongTypeBase) = default;

public:
  /// Initializes the wrapped value using the default constructor.
  constexpr StrongTypeBase() noexcept = default;

  /// Conversion constructor.
  template <typename U, typename = std::enable_if_t<std::is_convertible_v<U, T>>>
  constexpr StrongTypeBase(U value) noexcept: value_(static_cast<T>(value))  // NOLINT
  {
  }

  ~StrongTypeBase() = default;

public:  // getters and setters
  [[nodiscard]] constexpr T get() const noexcept { return value_; }
  constexpr void set(const T& value) noexcept { value_ = value; }
  constexpr void set(T&& value) noexcept { value_ = std::move(value); }

protected:  // accessors for subclasses
  [[nodiscard]] constexpr T& val() noexcept { return value_; }
  [[nodiscard]] constexpr const T& val() const noexcept { return value_; }

private:
  T value_ = {};
};

template <typename T>
struct ShouldBePassedByValue<StrongTypeBase<T>>: std::true_type
{
};

/// CRTP class that wraps T to make it a strong type.
template <typename T, typename D>
class StrongType: public StrongTypeBase<T>
{
public:
  using StrongTypeBase<T>::StrongTypeBase;
  using ValueType = typename StrongTypeBase<T>::ValueType;

public:  // special members
  constexpr SEN_MOVE_CONSTRUCT(StrongType) = default;
  constexpr SEN_COPY_CONSTRUCT(StrongType) = default;
  constexpr SEN_MOVE_ASSIGN(StrongType) = default;
  constexpr SEN_COPY_ASSIGN(StrongType) = default;
  ~StrongType() = default;

public:  // comparison operators, when available
  constexpr std::enable_if_t<HasOperator<T>::eq, bool> operator==(const D& other) const
  {
    return StrongTypeBase<T>::val() == other.val();
  }
  constexpr std::enable_if_t<HasOperator<T>::ne, bool> operator!=(const D& other) const
  {
    return StrongTypeBase<T>::val() != other.val();
  }
  constexpr std::enable_if_t<HasOperator<T>::lt, bool> operator<(const D& other) const
  {
    return StrongTypeBase<T>::val() < other.val();
  }
  constexpr std::enable_if_t<HasOperator<T>::le, bool> operator<=(const D& other) const
  {
    return StrongTypeBase<T>::val() <= other.val();
  }
  constexpr std::enable_if_t<HasOperator<T>::gt, bool> operator>(const D& other) const
  {
    return StrongTypeBase<T>::val() > other.val();
  }
  constexpr std::enable_if_t<HasOperator<T>::ge, bool> operator>=(const D& other) const
  {
    return StrongTypeBase<T>::val() >= other.val();
  }

public:  // increment and decrement, when available
  constexpr std::enable_if_t<HasOperator<T>::inc, D&> operator++()
  {
    StrongTypeBase<T>::val()++;
    return *static_cast<D*>(this);
  }
  constexpr std::enable_if_t<HasOperator<T>::dec, D&> operator--()
  {
    StrongTypeBase<T>::val()--;
    return *static_cast<D*>(this);
  }
  constexpr std::enable_if_t<HasOperator<T>::inc, D> operator++(int) &  // NOLINT(cert-dcl21-cpp)
  {
    D temp(StrongTypeBase<T>::val());
    ++(*this);
    return temp;
  }
  constexpr std::enable_if_t<HasOperator<T>::dec, D> operator--(int) &  // NOLINT(cert-dcl21-cpp)
  {
    D temp(StrongTypeBase<T>::val());
    --(*this);
    return temp;
  }

public:  // arithmetic operators, when available
  constexpr std::enable_if_t<HasOperator<T>::inc, D&> operator+=(const D& other) noexcept
  {
    StrongTypeBase<T>::val() += other.val();
    return *static_cast<D*>(this);
  }
  constexpr std::enable_if_t<HasOperator<T>::dec, D> operator+(const D& other) const noexcept
  {
    return D(StrongTypeBase<T>::val() + other.val());
  }
  constexpr std::enable_if_t<HasOperator<T>::dec, D&> operator-=(const D& other) noexcept
  {
    StrongTypeBase<T>::val() -= other.val();
    return *static_cast<D*>(this);
  }
  constexpr std::enable_if_t<HasOperator<T>::dec, D> operator-(const D& other) const noexcept
  {
    return D(StrongTypeBase<T>::val() - other.val());
  }
  constexpr std::enable_if_t<HasOperator<T>::dec, D> operator-() const noexcept { return D(-StrongTypeBase<T>::val()); }
  constexpr std::enable_if_t<HasOperator<T>::mul, D> operator*=(const D& other) noexcept
  {
    StrongTypeBase<T>::val() *= other.val();
    return *static_cast<D*>(this);
  }
  constexpr std::enable_if_t<HasOperator<T>::mul, D> operator*(const D& other) const noexcept
  {
    return D(StrongTypeBase<T>::val() * other.val());
  }
};

/// @}

template <typename T, typename D>
struct ShouldBePassedByValue<StrongType<T, D>>: std::true_type
{
};

}  // namespace sen

//--------------------------------------------------------------------------------------------------------------
// SEN_STRONG_TYPE, SEN_STRONG_TYPE_HASHABLE
//--------------------------------------------------------------------------------------------------------------

/// Helper macro to define strong types. NOLINTNEXTLINE
#define SEN_STRONG_TYPE(name, native_type)                                                                             \
  struct name: public ::sen::StrongType<native_type, name>                                                             \
  {                                                                                                                    \
    using Base = ::sen::StrongType<native_type, name>;                                                                 \
    using Base::Base;                                                                                                  \
    using Base::ValueType;                                                                                             \
  };

/// Makes your strong type std::hashable. NOLINTNEXTLINE
#define SEN_STRONG_TYPE_HASHABLE(name)                                                                                 \
  template <>                                                                                                          \
  struct std::hash<name>                                                                                               \
  {                                                                                                                    \
    inline std::size_t operator()(const name& val) const noexcept { return std::hash<name::ValueType> {}(val.get()); } \
  };

#endif  // SEN_CORE_BASE_STRONG_TYPE_H
