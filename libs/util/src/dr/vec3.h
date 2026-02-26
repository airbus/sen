// === vec3.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_UTIL_SRC_DR_VEC3_H
#define SEN_LIBS_UTIL_SRC_DR_VEC3_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"

// std
#include <cmath>
#include <initializer_list>

namespace sen::util
{
/// Handles all mathematical ops involving 3D Vectors
template <typename T>
class Vec3
{
public:
  SEN_COPY_MOVE(Vec3)

public:
  Vec3() noexcept;

  Vec3(T x, T y, T z) noexcept;

  ~Vec3() = default;

public:  // data
  T* ptr() noexcept;

  [[nodiscard]] const T* ptr() const noexcept;

public:  // setters
  void set(T x, T y, T z) noexcept;

  void setX(T x) noexcept;

  void setY(T y) noexcept;

  void setZ(T z) noexcept;

  void set(const Vec3& rhs) noexcept;

public:  // getters
  [[nodiscard]] T getX() const noexcept;

  [[nodiscard]] T getY() const noexcept;

  [[nodiscard]] T getZ() const noexcept;

public:  // comparison
  bool operator==(const Vec3& v) const noexcept;

  bool operator!=(const Vec3& v) const noexcept;

  bool operator<(const Vec3& v) const noexcept;

public:  // operators
  /// Point multiplication
  T operator*(const Vec3& rhs) const noexcept;

  /// Cross product.
  Vec3 operator^(const Vec3& rhs) const noexcept;

  /// Scalar multiplication.
  Vec3 operator*(T rhs) const noexcept;

  /// Unary scalar multiplication.
  Vec3& operator*=(T rhs) noexcept;

  /// Division by scalar
  Vec3 operator/(T rhs) const noexcept;

  /// Unary division by scalar
  Vec3& operator/=(T rhs) noexcept;

  /// Binary vector addition.
  Vec3 operator+(const Vec3& rhs) const noexcept;

  /// Binary vector addition.
  Vec3& operator+=(const Vec3& rhs) noexcept;

  /// Binary vector subtraction.
  Vec3 operator-(const Vec3& rhs) const noexcept;

  /// Binary vector subtraction.
  Vec3& operator-=(const Vec3& rhs) noexcept;

  /// Negates the internal elements of the vector.
  Vec3 operator-() const noexcept;

public:
  // Get the module of the vector
  [[nodiscard]] T length() const noexcept;

  // Get the module squared of the vector
  [[nodiscard]] T length2() const noexcept;

  /// Normalizes the vector so that its length is unity.
  /// Returns the previous length of the vector.
  T normalize() noexcept;

private:
  T v_[3];
};

using Vec3f = Vec3<f32>;
using Vec3d = Vec3<f64>;

//-------------------------------------------------------------------------------------------------------------------
// Inline implementation
//-------------------------------------------------------------------------------------------------------------------

template <typename T>
inline Vec3<T>::Vec3() noexcept
{
  v_[0] = 0.0f;
  v_[1] = 0.0f;
  v_[2] = 0.0f;
}

template <typename T>
inline Vec3<T>::Vec3(T x, T y, T z) noexcept
{
  v_[0] = x;
  v_[1] = y;
  v_[2] = z;
}

template <typename T>
inline T* Vec3<T>::ptr() noexcept
{
  return v_;
}

template <typename T>
inline const T* Vec3<T>::ptr() const noexcept
{
  return v_;
}

template <typename T>
inline void Vec3<T>::set(T x, T y, T z) noexcept
{
  v_[0] = x;
  v_[1] = y;
  v_[2] = z;
}

template <typename T>
inline void Vec3<T>::setX(T x) noexcept
{
  v_[0] = x;
}

template <typename T>
inline void Vec3<T>::setY(T y) noexcept
{
  v_[1] = y;
}

template <typename T>
inline void Vec3<T>::setZ(T z) noexcept
{
  v_[2] = z;
}

template <typename T>
inline void Vec3<T>::set(const Vec3& rhs) noexcept
{
  v_[0] = rhs.v_[0];
  v_[1] = rhs.v_[1];
  v_[2] = rhs.v_[2];
}

template <typename T>
[[nodiscard]] inline T Vec3<T>::getX() const noexcept
{
  return v_[0];
}

template <typename T>
[[nodiscard]] inline T Vec3<T>::getY() const noexcept
{
  return v_[1];
}

template <typename T>
[[nodiscard]] inline T Vec3<T>::getZ() const noexcept
{
  return v_[2];
}

template <typename T>
inline bool Vec3<T>::operator==(const Vec3& v) const noexcept
{
  return v_[0] == v.v_[0] && v_[1] == v.v_[1] && v_[2] == v.v_[2];
}

template <typename T>
inline bool Vec3<T>::operator!=(const Vec3& v) const noexcept
{
  return v_[0] != v.v_[0] || v_[1] != v.v_[1] || v_[2] != v.v_[2];
}

template <typename T>
inline bool Vec3<T>::operator<(const Vec3& v) const noexcept
{
  if (v_[0] < v.v_[0])
  {
    return true;
  }
  if (v_[0] > v.v_[0])
  {
    return false;
  }
  if (v_[1] < v.v_[1])
  {
    return true;
  }
  if (v_[1] > v.v_[1])
  {
    return false;
  }
  return (v_[2] < v.v_[2]);
}

template <typename T>
inline T Vec3<T>::operator*(const Vec3& rhs) const noexcept
{
  return v_[0] * rhs.v_[0] + v_[1] * rhs.v_[1] + v_[2] * rhs.v_[2];
}

template <typename T>
inline Vec3<T> Vec3<T>::operator^(const Vec3& rhs) const noexcept
{
  return Vec3(v_[1] * rhs.v_[2] - v_[2] * rhs.v_[1],
              v_[2] * rhs.v_[0] - v_[0] * rhs.v_[2],
              v_[0] * rhs.v_[1] - v_[1] * rhs.v_[0]);
}

template <typename T>
inline Vec3<T> Vec3<T>::operator*(T rhs) const noexcept
{
  return Vec3(v_[0] * rhs, v_[1] * rhs, v_[2] * rhs);
}

template <typename T>
inline Vec3<T>& Vec3<T>::operator*=(T rhs) noexcept
{
  v_[0] *= rhs;
  v_[1] *= rhs;
  v_[2] *= rhs;
  return *this;
}

template <typename T>
inline Vec3<T> Vec3<T>::operator/(T rhs) const noexcept
{
  return Vec3(v_[0] / rhs, v_[1] / rhs, v_[2] / rhs);
}

template <typename T>
inline Vec3<T>& Vec3<T>::operator/=(T rhs) noexcept
{
  v_[0] /= rhs;
  v_[1] /= rhs;
  v_[2] /= rhs;
  return *this;
}

template <typename T>
inline Vec3<T> Vec3<T>::operator+(const Vec3& rhs) const noexcept
{
  return Vec3(v_[0] + rhs.v_[0], v_[1] + rhs.v_[1], v_[2] + rhs.v_[2]);
}

template <typename T>
inline Vec3<T>& Vec3<T>::operator+=(const Vec3& rhs) noexcept
{
  v_[0] += rhs.v_[0];
  v_[1] += rhs.v_[1];
  v_[2] += rhs.v_[2];
  return *this;
}

template <typename T>
inline Vec3<T> Vec3<T>::operator-(const Vec3& rhs) const noexcept
{
  return Vec3(v_[0] - rhs.v_[0], v_[1] - rhs.v_[1], v_[2] - rhs.v_[2]);
}

template <typename T>
inline Vec3<T>& Vec3<T>::operator-=(const Vec3& rhs) noexcept
{
  v_[0] -= rhs.v_[0];
  v_[1] -= rhs.v_[1];
  v_[2] -= rhs.v_[2];
  return *this;
}

template <typename T>
inline Vec3<T> Vec3<T>::operator-() const noexcept
{
  return Vec3(-v_[0], -v_[1], -v_[2]);
}

template <typename T>
[[nodiscard]] inline T Vec3<T>::length() const noexcept
{
  return std::sqrt(v_[0] * v_[0] + v_[1] * v_[1] + v_[2] * v_[2]);
}

template <typename T>
[[nodiscard]] inline T Vec3<T>::length2() const noexcept
{
  return v_[0] * v_[0] + v_[1] * v_[1] + v_[2] * v_[2];
}

template <typename T>
inline T Vec3<T>::normalize() noexcept
{
  T norm = Vec3::length();
  if (norm > 0.0)
  {
    T inv = 1.0f / norm;
    v_[0] *= inv;
    v_[1] *= inv;
    v_[2] *= inv;
  }
  return (norm);
}

}  // namespace sen::util

#endif  // SEN_LIBS_UTIL_SRC_DR_VEC3_H
