// === mat3.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_SRC_DR_MAT3_H
#define SEN_LIBS_SRC_DR_MAT3_H

#include "quat.h"
#include "vec3.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"

// std
#include <cmath>
#include <initializer_list>

// NOLINTNEXTLINE
#define INNER_PRODUCT(a, b, r, c)                                                                                      \
  ((a).v_[r][0] * (b).v_[0][c]) + ((a).v_[r][1] * (b).v_[1][c]) + ((a).v_[r][2] * (b).v_[2][c])

// NOLINTNEXTLINE
#define SET_ROW(row, v1, v2, v3)                                                                                       \
  v_[(row)][0] = (v1);                                                                                                 \
  v_[(row)][1] = (v2);                                                                                                 \
  v_[(row)][2] = (v3);
// @}

namespace sen::util
{

/// Handles all mathematical ops involving 3D Matrices
template <typename T>
class Mat3
{
public:
  SEN_COPY_MOVE(Mat3)

public:
  Mat3() = default;
  ~Mat3() = default;

  /// Initializer list constructor
  Mat3(std::initializer_list<std::initializer_list<T>> list);

public:
  /// Constructs an identity matrix
  static Mat3 makeIdentity();

  /// Returns the world to body transformation matrix given the Euler angles
  static Mat3 makeFromEulerYPB(const Vec3d& eulerAngles);

  /// Returns the world to body transformation matrix given the Euler angles
  static Mat3 makeTransposedFromEulerYPB(const Vec3d& eulerAngles);

  /// Returns the matrix generated when multiplying the angular velocity vector by its transpose
  static Mat3 makeOmegaOmegaTranspose(const Vec3d& omega);

  /// Returns the skew matrix given an angular velocity
  static Mat3 makeSkewMatrix(const Vec3d& omega);

public:  // access operators
  /// Allows read/write access to the elements of the matrix
  T& operator()(u32 row, u32 col);

  /// Allows read-only access to the elements of the matrix
  T operator()(u32 row, u32 col) const;

public:
  /// Transforms a matrix into its transpose
  void transpose(const Mat3& value);

  /// Performs a matrix multiplication and stores the result in the current instance
  void mult(const Mat3& lhs, const Mat3& rhs);

  /// Pre-multiplies the matrix given as argument to the current instance
  void preMult(const Mat3& other);

  /// Post-multiplies the matrix given as argument to the current instance
  void postMult(const Mat3& other);

  /// Pre-multiplies the vector given as argument to the current instance
  [[nodiscard]] Vec3<T> preMult(const Vec3<T>& vec) const noexcept;

  /// Post-multiplies the vector given as argument to the current instance
  [[nodiscard]] Vec3<T> postMult(const Vec3<T>& vec) const noexcept;

public:  // multiplication operators
  /// Operator for post-multiplying the matrix by a vector
  [[nodiscard]] Vec3<T> operator*(const Vec3<T>& vec) const noexcept;

  /// Operator for post-multiplying the matrix by another matrix
  Mat3 operator*(const Mat3& other) const noexcept;

  /// Operator for multiplying the matrix by a constant
  Mat3 operator*(T scalar) const noexcept;

  /// Operator for summing the matrix with another matrix
  Mat3 operator+(const Mat3& other) const noexcept;

private:
  T v_[3U][3U];
};

using Mat3f = Mat3<f32>;
using Mat3d = Mat3<f64>;

//-------------------------------------------------------------------------------------------------------------------
// Inline implementation
//-------------------------------------------------------------------------------------------------------------------

template <typename T>
Mat3<T>::Mat3(std::initializer_list<std::initializer_list<T>> list)
{
  u32 i = 0;
  u32 j = 0;
  for (const auto& row: list)
  {
    for (const auto& elem: row)
    {
      v_[i][j++] = elem;  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
      if (j == 3)
      {
        j = 0;
        i++;
      }
      if (i == 3)
      {
        return;
      }
    }
  }
}

template <typename T>
Mat3<T> Mat3<T>::makeIdentity()
{
  return {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
}

template <typename T>
Mat3<T> Mat3<T>::makeFromEulerYPB(const Vec3d& eulerAngles)
{
  const auto psi = eulerAngles.getX();
  const auto theta = eulerAngles.getY();
  const auto phi = eulerAngles.getZ();

  const auto cosPsi = cos(psi);
  const auto cosTheta = cos(theta);
  const auto cosPhi = cos(phi);
  const auto sinPsi = sin(psi);
  const auto sinTheta = sin(theta);
  const auto sinPhi = sin(phi);
  const auto sinPhiTimesSinTheta = sinPhi * sinTheta;
  const auto cosPhiTimesSinTheta = cosPhi * sinTheta;

  Mat3<T> result {};
  {
    result(0, 0) = cosTheta * cosPsi;
    result(0, 1) = cosTheta * sinPsi;
    result(0, 2) = -sinTheta;
    result(1, 0) = sinPhiTimesSinTheta * cosPsi - cosPhi * sinPsi;
    result(1, 1) = sinPhiTimesSinTheta * sinPsi + cosPhi * cosPsi;
    result(1, 2) = sinPhi * cosTheta;
    result(2, 0) = cosPhiTimesSinTheta * cosPsi + sinPhi * sinPsi;
    result(2, 1) = cosPhiTimesSinTheta * sinPsi - sinPhi * cosPsi;
    result(2, 2) = cosPhi * cosTheta;
  }

  return result;
}

template <typename T>
Mat3<T> Mat3<T>::makeTransposedFromEulerYPB(const Vec3d& eulerAngles)
{
  const auto psi = eulerAngles.getX();
  const auto theta = eulerAngles.getY();
  const auto phi = eulerAngles.getZ();

  const auto cosPsi = cos(psi);
  const auto cosTheta = cos(theta);
  const auto cosPhi = cos(phi);
  const auto sinPsi = sin(psi);
  const auto sinTheta = sin(theta);
  const auto sinPhi = sin(phi);
  const auto sinPhiTimesSinTheta = sinPhi * sinTheta;
  const auto cosPhiTimesSinTheta = cosPhi * sinTheta;

  Mat3<T> result {};
  {
    result(0, 0) = cosTheta * cosPsi;
    result(1, 0) = cosTheta * sinPsi;
    result(2, 0) = -sinTheta;
    result(0, 1) = sinPhiTimesSinTheta * cosPsi - cosPhi * sinPsi;
    result(1, 1) = sinPhiTimesSinTheta * sinPsi + cosPhi * cosPsi;
    result(2, 1) = sinPhi * cosTheta;
    result(0, 2) = cosPhiTimesSinTheta * cosPsi + sinPhi * sinPsi;
    result(1, 2) = cosPhiTimesSinTheta * sinPsi - sinPhi * cosPsi;
    result(2, 2) = cosPhi * cosTheta;
  }

  return result;
}

template <typename T>
Mat3<T> Mat3<T>::makeOmegaOmegaTranspose(const Vec3d& omega)
{
  const auto wx = omega.getX();
  const auto wy = omega.getY();
  const auto wz = omega.getZ();
  const auto wxwy = wx * wy;
  const auto wxwz = wx * wz;
  const auto wywz = wy * wz;

  Mat3<T> result {};
  {
    result(0, 0) = wx * wx;
    result(0, 1) = wxwy;
    result(0, 2) = wxwz;
    result(1, 0) = wxwy;
    result(1, 1) = wy * wy;
    result(1, 2) = wywz;
    result(2, 0) = wxwz;
    result(2, 1) = wywz;
    result(2, 2) = wz * wz;
  }

  return result;
}

template <typename T>
Mat3<T> Mat3<T>::makeSkewMatrix(const Vec3d& omega)
{
  const auto wx = omega.getX();
  const auto wy = omega.getY();
  const auto wz = omega.getZ();

  Mat3<T> result {};
  {
    result(0, 0) = 0;
    result(0, 1) = -wz;
    result(0, 2) = wy;
    result(1, 0) = wz;
    result(1, 1) = 0;
    result(1, 2) = -wx;
    result(2, 0) = -wy;
    result(2, 1) = wx;
    result(2, 2) = 0;
  }
  return result;
}

template <typename T>
inline T& Mat3<T>::operator()(u32 row, u32 col)
{
  return v_[row][col];  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
}

template <typename T>
inline T Mat3<T>::operator()(u32 row, u32 col) const
{
  return v_[row][col];  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
}

template <typename T>
inline void Mat3<T>::transpose(const Mat3& value)
{
  if (&value == this)
  {
    Mat3 tm(value);
    return transpose(tm);
  }
  v_[0][0] = value.v_[0][0];
  v_[0][1] = value.v_[1][0];
  v_[0][2] = value.v_[2][0];
  v_[1][0] = value.v_[0][1];
  v_[1][1] = value.v_[1][1];
  v_[1][2] = value.v_[2][1];
  v_[2][0] = value.v_[0][2];
  v_[2][1] = value.v_[1][2];
  v_[2][2] = value.v_[2][2];
}

template <typename T>
inline void Mat3<T>::mult(const Mat3& lhs, const Mat3& rhs)
{
  if (&lhs == this)
  {
    postMult(rhs);
    return;
  }
  if (&rhs == this)
  {
    preMult(lhs);
    return;
  }

  v_[0][0] = INNER_PRODUCT(lhs, rhs, 0, 0);
  v_[0][1] = INNER_PRODUCT(lhs, rhs, 0, 1);
  v_[0][2] = INNER_PRODUCT(lhs, rhs, 0, 2);
  v_[1][0] = INNER_PRODUCT(lhs, rhs, 1, 0);
  v_[1][1] = INNER_PRODUCT(lhs, rhs, 1, 1);
  v_[1][2] = INNER_PRODUCT(lhs, rhs, 1, 2);
  v_[2][0] = INNER_PRODUCT(lhs, rhs, 2, 0);
  v_[2][1] = INNER_PRODUCT(lhs, rhs, 2, 1);
  v_[2][2] = INNER_PRODUCT(lhs, rhs, 2, 2);
}

template <typename T>
inline void Mat3<T>::preMult(const Mat3& other)
{
  // temporal storage used for efficiency
  T temp[3];

  for (u32 col = 0; col < 3U; ++col)
  {
    temp[0] = INNER_PRODUCT(other, *this, 0, col);  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    temp[1] = INNER_PRODUCT(other, *this, 1, col);  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    temp[2] = INNER_PRODUCT(other, *this, 2, col);  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    v_[0][col] = temp[0];                           // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    v_[1][col] = temp[1];                           // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    v_[2][col] = temp[2];                           // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
  }
}

template <typename T>
inline void Mat3<T>::postMult(const Mat3& other)
{
  // temporal storage used for efficiency
  T temp[3];

  for (u32 row = 0; row < 3U; ++row)
  {
    temp[0] = INNER_PRODUCT(*this, other, row, 0);  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    temp[1] = INNER_PRODUCT(*this, other, row, 1);  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    temp[2] = INNER_PRODUCT(*this, other, row, 2);  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    SET_ROW(row, temp[0], temp[1], temp[2])         // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
  }
}

template <typename T>
Vec3<T> Mat3<T>::preMult(const Vec3<T>& vec) const noexcept
{
  return {
    v_[0][0] * vec.getX() + v_[1][0] * vec.getY() + v_[2][0] * vec.getZ(),
    v_[0][1] * vec.getX() + v_[1][1] * vec.getY() + v_[2][1] * vec.getZ(),
    v_[0][2] * vec.getX() + v_[1][2] * vec.getY() + v_[2][2] * vec.getZ(),
  };
}

template <typename T>
inline Vec3<T> Mat3<T>::postMult(const Vec3<T>& vec) const noexcept
{
  return {v_[0][0] * vec.getX() + v_[0][1] * vec.getY() + v_[0][2] * vec.getZ(),
          v_[1][0] * vec.getX() + v_[1][1] * vec.getY() + v_[1][2] * vec.getZ(),
          v_[2][0] * vec.getX() + v_[2][1] * vec.getY() + v_[2][2] * vec.getZ()};
}

template <typename T>
inline Vec3<T> Mat3<T>::operator*(const Vec3<T>& vec) const noexcept
{
  return postMult(vec);
}

template <typename T>
inline Mat3<T> Mat3<T>::operator*(const Mat3& other) const noexcept
{
  Mat3<T> result {};
  result.mult(*this, other);
  return result;
}

template <typename T>
inline Mat3<T> Mat3<T>::operator*(T scalar) const noexcept
{
  Mat3<T> result {};

  for (u32 i = 0; i < 3U; ++i)
  {
    for (u32 j = 0; j < 3U; ++j)
    {
      result(i, j) = v_[i][j] * scalar;  // NOLINT
    }
  }
  return result;
}

template <typename T>
inline Mat3<T> Mat3<T>::operator+(const Mat3& other) const noexcept
{
  Mat3<T> result {};

  for (u32 i = 0; i < 3U; ++i)
  {
    for (u32 j = 0; j < 3U; ++j)
    {
      result(i, j) = v_[i][j] + other(i, j);  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
    }
  }
  return result;
}

}  // namespace sen::util

#endif  // SEN_LIBS_SRC_DR_MAT3_H
