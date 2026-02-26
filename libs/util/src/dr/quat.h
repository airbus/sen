// === quat.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_UTIL_SRC_DR_QUAT_H
#define SEN_LIBS_UTIL_SRC_DR_QUAT_H

#include "constants.h"
#include "vec3.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"

namespace sen::util
{

/// Quaternion. Represents the orientation of an object in space.
template <typename T>
class Quat
{
public:
  SEN_COPY_MOVE(Quat)

public:  // set of builders
  Quat() noexcept = default;

  Quat(T x, T y, T z, T w) noexcept;

  Quat(T angle, const Vec3<T>& axis) noexcept;

  Quat(T yaw, T pitch, T bank) noexcept;

  explicit Quat(const Vec3<T>& eulerAngles) noexcept;

  ~Quat() = default;

public:  // custom operators
  bool operator==(const Quat& v) const noexcept;

  bool operator!=(const Quat& v) const noexcept;

  bool operator<(const Quat& v) const noexcept;

  Quat operator*(T rhs) const;

  Quat& operator*=(T rhs) noexcept;

  Quat operator*(const Quat& rhs) const noexcept;

  Vec3<T> operator*(const Vec3<T>& v) const noexcept;

  Quat& operator*=(const Quat& rhs) noexcept;

  Quat operator/(const Quat& denom) const noexcept;

  Quat& operator/=(const Quat& denom) noexcept;

  Quat operator+(const Quat& rhs) const noexcept;

  Quat operator/(const T rhs) const noexcept;

  Quat& operator/=(T rhs) noexcept;

  Quat& operator+=(const Quat& rhs) noexcept;

  Quat operator-(const Quat& rhs) const noexcept;

  Quat& operator-=(const Quat& rhs) noexcept;

  Quat operator-() const noexcept;

  // Dot product operator
  [[nodiscard]] T dot(const Quat<T>& other) const noexcept;

public:  // operations to get stored values
  [[nodiscard]] Vec3<T> asVec3() const noexcept;

  void set(T x, T y, T z, T w) noexcept;

  [[nodiscard]] T getX() const noexcept;
  [[nodiscard]] T getY() const noexcept;
  [[nodiscard]] T getZ() const noexcept;
  [[nodiscard]] T getW() const noexcept;

  void setX(T x) noexcept;

  void setY(T y) noexcept;

  void setZ(T z) noexcept;

  void setW(T w) noexcept;

  [[nodiscard]] bool zeroRotation() const noexcept;

public:  // quaternion information
  [[nodiscard]] T length() const noexcept;

  [[nodiscard]] T length2() const noexcept;

  /// returns the conjugate of a quaternion which is obtained by negating the imaginary components while keeping
  /// the real component unchanged
  [[nodiscard]] Quat conj() const;

  /// returns the  inverse of a quaternion q, denoted as q-1, is defined such that the product of
  /// q and its inverse yields the identity quaternion.
  [[nodiscard]] Quat inverse() const;

public:  // quaternion rotations
  void makeRotate(T angle, T x, T y, T z) noexcept;

  void makeRotate(T angle, const Vec3<T>& vec) noexcept;

  void makeRotate(const Vec3<T>& from, const Vec3<T>& to) noexcept;

  void makeRotate(T angle1,
                  const Vec3<T>& axis1,
                  T angle2,
                  const Vec3<T>& axis2,
                  T angle3,
                  const Vec3<T>& axis3) noexcept;

  void makeRotateFromEulerYPB(T yaw, T pitch, T bank) noexcept;

  void makeRotateFromEulerYPB(const Vec3<T>& eulerAngles) noexcept;

  void getRotate(T& angle, T& x, T& y, T& z) const noexcept;

  void getRotate(T& angle, Vec3<T>& vec) const noexcept;

  [[nodiscard]] Vec3<T> getRotateInEulerYPB() const noexcept;

public:  // interpolations
  /// Performs spherical linear interpolation (slerp) between two quaternions.
  ///
  /// @param t interpolation step.
  /// @param from Starting quaternion.
  /// @param to Ending quaternion.
  void slerp(T t, const Quat& from, const Quat& to) noexcept;

private:
  T v_[4] {0, 0, 0, 1};
};

using Quatf = Quat<f32>;
using Quatd = Quat<f64>;

//-------------------------------------------------------------------------------------------------------------------
// Inline implementation
//-------------------------------------------------------------------------------------------------------------------

template <typename T>
inline Quat<T>::Quat(T x, T y, T z, T w) noexcept
{
  v_[0] = x;
  v_[1] = y;
  v_[2] = z;
  v_[3] = w;
}

template <typename T>
inline Quat<T>::Quat(T angle, const Vec3<T>& axis) noexcept
{
  v_[0] = static_cast<T>(0.0);
  v_[1] = static_cast<T>(0.0);
  v_[2] = static_cast<T>(0.0);
  v_[3] = static_cast<T>(1.0);

  makeRotate(angle, axis);
}

template <typename T>
inline Quat<T>::Quat(T yaw, T pitch, T bank) noexcept
{
  makeRotateFromEulerYPB(yaw, pitch, bank);
}

template <typename T>
inline Quat<T>::Quat(const Vec3<T>& eulerAngles) noexcept
{
  makeRotateFromEulerYPB(eulerAngles.getX(), eulerAngles.getY(), eulerAngles.getZ());
}

template <typename T>
inline bool Quat<T>::operator==(const Quat& v) const noexcept
{
  return v_[0] == v.v_[0] && v_[1] == v.v_[1] && v_[2] == v.v_[2] && v_[3] == v.v_[3];
}

template <typename T>
inline bool Quat<T>::operator!=(const Quat& v) const noexcept
{
  return v_[0] != v.v_[0] || v_[1] != v.v_[1] || v_[2] != v.v_[2] || v_[3] != v.v_[3];
}

template <typename T>
inline bool Quat<T>::operator<(const Quat& v) const noexcept
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

  if (v_[2] < v.v_[2])
  {
    return true;
  }

  if (v_[2] > v.v_[2])
  {
    return false;
  }

  return (v_[3] < v.v_[3]);
}

template <typename T>
inline Vec3<T> Quat<T>::asVec3() const noexcept
{
  return Vec3<T> {v_[0], v_[1], v_[2]};
}

template <typename T>
inline void Quat<T>::set(T x, T y, T z, T w) noexcept
{
  v_[0] = x;
  v_[1] = y;
  v_[2] = z;
  v_[3] = w;
}

template <typename T>
inline T Quat<T>::getX() const noexcept
{
  return v_[0];
}

template <typename T>
inline T Quat<T>::getY() const noexcept
{
  return v_[1];
}

template <typename T>
inline T Quat<T>::getZ() const noexcept
{
  return v_[2];
}

template <typename T>
inline T Quat<T>::getW() const noexcept
{
  return v_[3];
}

template <typename T>
inline void Quat<T>::setX(T x) noexcept
{
  v_[0] = x;
}

template <typename T>
inline void Quat<T>::setY(T y) noexcept
{
  v_[1] = y;
}

template <typename T>
inline void Quat<T>::setZ(T z) noexcept
{
  v_[2] = z;
}

template <typename T>
inline void Quat<T>::setW(T w) noexcept
{
  v_[3] = w;
}

template <typename T>
inline bool Quat<T>::zeroRotation() const noexcept
{
  return v_[0] == 0.0 && v_[1] == 0.0 && v_[2] == 0.0 && v_[3] == 1.0;
}

template <typename T>
inline Quat<T> Quat<T>::operator*(T rhs) const
{
  return {v_[0] * rhs, v_[1] * rhs, v_[2] * rhs, v_[3] * rhs};
}

template <typename T>
inline Quat<T>& Quat<T>::operator*=(T rhs) noexcept
{
  v_[0] *= rhs;
  v_[1] *= rhs;
  v_[2] *= rhs;
  v_[3] *= rhs;
  return *this;
}

template <typename T>
inline Quat<T> Quat<T>::operator*(const Quat& rhs) const noexcept
{
  return {rhs.v_[3] * v_[0] + rhs.v_[0] * v_[3] + rhs.v_[1] * v_[2] - rhs.v_[2] * v_[1],
          rhs.v_[3] * v_[1] - rhs.v_[0] * v_[2] + rhs.v_[1] * v_[3] + rhs.v_[2] * v_[0],
          rhs.v_[3] * v_[2] + rhs.v_[0] * v_[1] - rhs.v_[1] * v_[0] + rhs.v_[2] * v_[3],
          rhs.v_[3] * v_[3] - rhs.v_[0] * v_[0] - rhs.v_[1] * v_[1] - rhs.v_[2] * v_[2]};
}

template <typename T>
inline Quat<T>& Quat<T>::operator*=(const Quat& rhs) noexcept
{
  T x = rhs.v_[3] * v_[0] + rhs.v_[0] * v_[3] + rhs.v_[1] * v_[2] - rhs.v_[2] * v_[1];
  T y = rhs.v_[3] * v_[1] - rhs.v_[0] * v_[2] + rhs.v_[1] * v_[3] + rhs.v_[2] * v_[0];
  T z = rhs.v_[3] * v_[2] + rhs.v_[0] * v_[1] - rhs.v_[1] * v_[0] + rhs.v_[2] * v_[3];
  v_[3] = rhs.v_[3] * v_[3] - rhs.v_[0] * v_[0] - rhs.v_[1] * v_[1] - rhs.v_[2] * v_[2];

  v_[2] = z;
  v_[1] = y;
  v_[0] = x;

  return (*this);
}

template <typename T>
inline Quat<T> Quat<T>::operator/(T rhs) const noexcept
{
  T div = 1.0 / rhs;
  return {v_[0] * div, v_[1] * div, v_[2] * div, v_[3] * div};
}

template <typename T>
inline Quat<T>& Quat<T>::operator/=(T rhs) noexcept
{
  T div = 1.0 / rhs;
  v_[0] *= div;
  v_[1] *= div;
  v_[2] *= div;
  v_[3] *= div;
  return *this;
}

template <typename T>
inline Quat<T> Quat<T>::operator/(const Quat& denom) const noexcept
{
  return ((*this) * denom.inverse());
}

template <typename T>
inline Quat<T>& Quat<T>::operator/=(const Quat& denom) noexcept
{
  (*this) = (*this) * denom.inverse();
  return (*this);
}

template <typename T>
inline Quat<T> Quat<T>::operator+(const Quat& rhs) const noexcept
{
  return {v_[0] + rhs.v_[0], v_[1] + rhs.v_[1], v_[2] + rhs.v_[2], v_[3] + rhs.v_[3]};
}

template <typename T>
inline Quat<T>& Quat<T>::operator+=(const Quat& rhs) noexcept
{
  v_[0] += rhs.v_[0];
  v_[1] += rhs.v_[1];
  v_[2] += rhs.v_[2];
  v_[3] += rhs.v_[3];
  return *this;
}

template <typename T>
inline Quat<T> Quat<T>::operator-(const Quat& rhs) const noexcept
{
  return {v_[0] - rhs.v_[0], v_[1] - rhs.v_[1], v_[2] - rhs.v_[2], v_[3] - rhs.v_[3]};
}

template <typename T>
inline Quat<T>& Quat<T>::operator-=(const Quat& rhs) noexcept
{
  v_[0] -= rhs.v_[0];
  v_[1] -= rhs.v_[1];
  v_[2] -= rhs.v_[2];
  v_[3] -= rhs.v_[3];
  return *this;
}

template <typename T>
inline Quat<T> Quat<T>::operator-() const noexcept
{
  return {-v_[0], -v_[1], -v_[2], -v_[3]};
}

template <typename T>
T Quat<T>::dot(const Quat<T>& other) const noexcept
{
  return v_[0] * other.v_[0] + v_[1] * other.v_[1] + v_[2] * other.v_[2] + v_[3] * other.v_[3];
}

template <typename T>
inline T Quat<T>::length() const noexcept
{
  return sqrt(length2());
}

template <typename T>
inline T Quat<T>::length2() const noexcept
{
  return v_[0] * v_[0] + v_[1] * v_[1] + v_[2] * v_[2] + v_[3] * v_[3];
}

template <typename T>
inline Quat<T> Quat<T>::conj() const
{
  return {-v_[0], -v_[1], -v_[2], v_[3]};
}

template <typename T>
inline Quat<T> Quat<T>::inverse() const
{
  return conj() / length2();
}

template <typename T>
inline void Quat<T>::makeRotate(T angle, T x, T y, T z) noexcept
{
  constexpr T epsilon = 0.0000001;

  T length = std::sqrt(x * x + y * y + z * z);
  if (length < epsilon)
  {
    return;
  }

  T cosHalfAngle = cos(0.5 * angle);
  T sinHalfAngle = sin(0.5 * angle);

  T quatX = x * sinHalfAngle / length;
  T quatY = y * sinHalfAngle / length;
  T quatZ = z * sinHalfAngle / length;
  T quatW = cosHalfAngle;

  Quat rotationQuat {quatX, quatY, quatZ, quatW};

  *this = *this * rotationQuat;
}

template <typename T>
inline void Quat<T>::makeRotate(T angle, const Vec3<T>& vec) noexcept
{
  makeRotate(angle, vec.getX(), vec.getY(), vec.getZ());
}

template <typename T>
inline void Quat<T>::makeRotate(const Vec3<T>& from, const Vec3<T>& to) noexcept
{
  auto sourceVector = from;
  auto targetVector = to;

  T fromLen2 = from.length2();
  T fromLen;

  if ((fromLen2 < 1.0 - 1e-7) || (fromLen2 > 1.0 + 1e-7))
  {
    fromLen = sqrt(fromLen2);
    sourceVector /= fromLen;
  }
  else
  {
    fromLen = 1.0;
  }

  T toLen2 = to.length2();
  if ((toLen2 < 1.0 - 1e-7) || (toLen2 > 1.0 + 1e-7))
  {
    T toLen;
    if ((toLen2 > fromLen2 - 1e-7) && (toLen2 < fromLen2 + 1e-7))
    {
      toLen = fromLen;
    }
    else
    {
      toLen = sqrt(toLen2);
    }
    targetVector /= toLen;
  }

  T dotProdPlus1 = 1.0 + sourceVector * targetVector;

  if (dotProdPlus1 < 1e-7)
  {
    if (fabs(sourceVector.getX()) < 0.6)
    {
      const auto norm = sqrt(1.0 - sourceVector.getX() * sourceVector.getX());
      v_[0] = 0.0;
      v_[1] = sourceVector.getZ() / norm;
      v_[2] = -sourceVector.getY() / norm;
      v_[3] = 0.0;
    }
    else if (fabs(sourceVector.getY()) < 0.6)
    {
      const auto norm = sqrt(1.0 - sourceVector.getY() * sourceVector.getY());
      v_[0] = -sourceVector.getZ() / norm;
      v_[1] = 0.0;
      v_[2] = sourceVector.getX() / norm;
      v_[3] = 0.0;
    }
    else
    {
      const auto norm = sqrt(1.0 - sourceVector.getZ() * sourceVector.getZ());
      v_[0] = sourceVector.getY() / norm;
      v_[1] = -sourceVector.getX() / norm;
      v_[2] = 0.0;
      v_[3] = 0.0;
    }
  }
  else
  {
    const auto s = sqrt(0.5 * dotProdPlus1);
    const auto tmp = sourceVector ^ targetVector / (2.0 * s);
    v_[0] = tmp.getX();
    v_[1] = tmp.getY();
    v_[2] = tmp.getZ();
    v_[3] = s;
  }
}

template <typename T>
inline void Quat<T>::makeRotate(T angle1,
                                const Vec3<T>& axis1,
                                T angle2,
                                const Vec3<T>& axis2,
                                T angle3,
                                const Vec3<T>& axis3) noexcept
{
  Quat q1;
  q1.makeRotate(angle1, axis1);
  Quat q2;
  q2.makeRotate(angle2, axis2);
  Quat q3;
  q3.makeRotate(angle3, axis3);

  *this = q1 * q2 * q3;
}

template <typename T>
inline void Quat<T>::makeRotateFromEulerYPB(T yaw, T pitch, T bank) noexcept
{
  makeRotate(bank, {1.0, 0.0, 0.0}, pitch, {0.0, 1.0, 0.0}, yaw, {0.0, 0.0, 1.0});
}

template <typename T>
inline void Quat<T>::makeRotateFromEulerYPB(const Vec3<T>& eulerAngles) noexcept
{
  makeRotate(eulerAngles.z(), {1.0, 0.0, 0.0}, eulerAngles.y(), {0.0, 1.0, 0.0}, eulerAngles.x(), {0.0, 0.0, 1.0});
}

template <typename T>
inline void Quat<T>::getRotate(T& angle, Vec3<T>& vec) const noexcept
{
  T x;
  T y;
  T z;
  getRotate(angle, x, y, z);
  vec.setX(x);
  vec.setY(y);
  vec.setZ(z);
}

template <typename T>
inline void Quat<T>::getRotate(T& angle, T& x, T& y, T& z) const noexcept
{
  T sinhalfangle = std::sqrt(v_[0] * v_[0] + v_[1] * v_[1] + v_[2] * v_[2]);

  angle = 2.0 * std::atan2(sinhalfangle, v_[3]);

  if constexpr (std::is_same_v<f32, T>)
  {
    angle = angle > pif ? angle - 2.0f * pif : angle < -pif ? angle + 2.0f * pif : angle;
  }
  else
  {
    angle = angle > pi ? angle - 2.0 * pi : angle < -pi ? angle + 2.0 * pi : angle;
  }

  if (sinhalfangle != 0.0)
  {
    x = v_[0] / sinhalfangle;
    y = v_[1] / sinhalfangle;
    z = v_[2] / sinhalfangle;
  }
  else
  {
    x = 0.0;
    y = 0.0;
    z = 1.0;
  }
}

template <typename T>
inline Vec3<T> Quat<T>::getRotateInEulerYPB() const noexcept
{
  T sqw = v_[3] * v_[3];
  T sqx = v_[0] * v_[0];
  T sqy = v_[1] * v_[1];
  T sqz = v_[2] * v_[2];

  T yaw = atan2(2.0 * (v_[0] * v_[1] + v_[2] * v_[3]), (sqx - sqy - sqz + sqw));
  T pitch = asin(-2.0 * (v_[0] * v_[2] - v_[1] * v_[3]) / (sqx + sqy + sqz + sqw));
  T bank = atan2(2.0 * (v_[1] * v_[2] + v_[0] * v_[3]), (-sqx - sqy + sqz + sqw));

  return {yaw, pitch, bank};
}

template <typename T>
inline Vec3<T> Quat<T>::operator*(const Vec3<T>& v) const noexcept
{
  Vec3<T> uv;
  Vec3<T> uuv;
  Vec3<T> qvec(v_[0], v_[1], v_[2]);
  uv = qvec ^ v;
  uuv = qvec ^ uv;
  uv *= 2.0 * v_[3];
  uuv *= 2.0;
  return v + uv + uuv;
}

template <typename T>
inline void Quat<T>::slerp(T t, const Quat& from, const Quat& to) noexcept
{
  const double epsilon = 0.00001;
  double omega;
  double cosOmega;
  double sinOmega;
  double scaleFrom;
  double scaleTo;

  Quat quatTo(to);

  cosOmega = from.v_[0] * to.v_[0] + from.v_[1] * to.v_[1] + from.v_[2] * to.v_[2] + from.v_[3] * to.v_[3];

  if ((1.0 - cosOmega) > epsilon)
  {
    omega = acos(cosOmega);
    sinOmega = sin(omega);
    scaleFrom = sin((1.0 - t) * omega) / sinOmega;
    scaleTo = sin(t * omega) / sinOmega;
  }
  else
  {
    scaleFrom = 1.0 - t;
    scaleTo = t;
  }

  *this = (from * scaleFrom) + (quatTo * scaleTo);
}

}  // namespace sen::util

#endif  // SEN_LIBS_UTIL_SRC_DR_QUAT_H
