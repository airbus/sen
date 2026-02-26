// === native_types.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_NATIVE_TYPES_H
#define SEN_CORE_META_NATIVE_TYPES_H

// sen
#include "sen/core/base/hash32.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/span.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/detail/native_types_impl.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_visitor.h"

// std
#include <cstdint>
#include <string>
#include <string_view>

namespace sen
{

/// \addtogroup types
/// @{

/// Represents a native (built-in) type that can be created on the
/// stack or sent in a message.
class NativeType: public Type
{
  SEN_META_TYPE(NativeType)

protected:
  explicit NativeType(MemberHash hash) noexcept;
  ~NativeType() noexcept override = default;
};

/// Represents a numeric native type. \ingroup meta
class NumericType: public NativeType
{
  SEN_META_TYPE(NumericType)

public:
  /// Tells if this is a signed numeric type.
  ///
  /// @return true if it's signed, false otherwise.
  [[nodiscard]] virtual bool isSigned() const noexcept = 0;

  /// Tells if the type has a representation for positive
  /// infinity.
  ///
  /// @return true if the type has a representation for positive infinity
  [[nodiscard]] virtual bool hasInfinity() const noexcept = 0;

  /// An IEC-559 type always has has_infinity, has_quiet_NaN and
  /// has_signaling_NaN set to true; And infinity, quiet_NaN and
  /// signaling_NaN return some non-zero value.
  ///
  /// @return true if the type adheres to IEC-559 / IEEE-754 standard.
  [[nodiscard]] virtual bool isIEC559() const noexcept = 0;

  /// The number of bytes used by a value.
  [[nodiscard]] virtual std::size_t getByteSize() const noexcept = 0;

public:  // implements Type
  [[nodiscard]] bool isBounded() const noexcept final { return true; }

protected:
  explicit NumericType(MemberHash hash) noexcept;
  ~NumericType() noexcept override = default;
};

/// Represents an integral numeric type. \ingroup meta
class IntegralType: public NumericType
{
  SEN_META_TYPE(IntegralType)

public:
  explicit IntegralType(MemberHash hash) noexcept;
  ~IntegralType() noexcept override = default;
};

/// Represents a floating point numeric type.
class RealType: public NumericType
{
  SEN_META_TYPE(RealType)

public:
  explicit RealType(MemberHash hash) noexcept;
  ~RealType() noexcept override = default;

public:
  /// Maximum finite value.
  /// Equivalent to FLT_MAX, DBL_MAX or LDBL_MAX, depending on
  /// type.
  ///
  /// @return maximum finite value.
  [[nodiscard]] virtual double getMaxValue() const noexcept = 0;

  /// Minimum finite value.
  /// If de-normalization available (variable number of exponent
  /// bits): minimum positive normalized value. Equivalent to
  /// FLT_MIN, DBL_MIN, LDBL_MIN or 0, depending on type.
  ///
  /// @return minimum finite value.
  [[nodiscard]] virtual double getMinValue() const noexcept = 0;

  /// Machine epsilon (the difference between 1 and the least
  /// value greater than 1 that is representable).
  /// Equivalent to FLT_EPSILON, DBL_EPSILON or LDBL_EPSILON for
  /// floating types.
  ///
  /// @return machine epsilon
  [[nodiscard]] virtual double getEpsilon() const noexcept = 0;
};

/// @cond

// integral numbers
SEN_INTEGRAL_TYPE(UInt8Type, uint8_t, u8, "8 bit unsigned integer", readUInt8, writeUInt8);
SEN_INTEGRAL_TYPE(Int16Type, int16_t, i16, "16 bit signed integer", readInt16, writeInt16);
SEN_INTEGRAL_TYPE(UInt16Type, uint16_t, u16, "16 bit unsigned integer", readUInt16, writeUInt16);
SEN_INTEGRAL_TYPE(Int32Type, int32_t, i32, "32 bit signed integer", readInt32, writeInt32);
SEN_INTEGRAL_TYPE(UInt32Type, uint32_t, u32, "32 bit unsigned integer", readUInt32, writeUInt32);
SEN_INTEGRAL_TYPE(Int64Type, int64_t, i64, "64 bit signed integer", readInt64, writeUInt64);
SEN_INTEGRAL_TYPE(UInt64Type, uint64_t, u64, "64 bit unsigned integer", readUInt64, writeUInt64);

// real numbers
SEN_REAL_TYPE(Float32Type, float32_t, f32, "32 bit floating point number", readFloat32, writeFloat32);
SEN_REAL_TYPE(Float64Type, float64_t, f64, "64 bit floating point number", readFloat64, writeFloat64);

// other native types
SEN_BUILTIN_NATIVE_TYPE(BoolType, bool, "bool", "classic boolean", readBool, writeBool, true);
SEN_BUILTIN_NATIVE_TYPE(StringType, std::string, "string", "unbounded string", readString, writeString, false);

/// @endcond

/// All the native types
[[nodiscard]] const Span<ConstTypeHandle<> const>& getNativeTypes();

/// @}

/// Used for indicating that no result value is provided.
class VoidType final: public Type
{
public:
  using NativeType = void;

  VoidType(): Type(hashCombine(hashSeed, std::string("void"))) {}

  void accept(FullTypeVisitor& tv) const override { tv.apply(*this); }

public:
  [[nodiscard]] const VoidType* asVoidType() const noexcept override { return this; }

public:  // Implements Type
  [[nodiscard]] std::string_view getName() const noexcept final { return "void"; }
  [[nodiscard]] std::string_view getDescription() const noexcept final { return "Classic void"; }
  [[nodiscard]] bool equals(const Type& other) const noexcept final { return other.isVoidType(); }
  [[nodiscard]] bool isBounded() const noexcept final { return true; }

  /// Unique instance
  [[nodiscard]] static sen::TypeHandle<const VoidType> get();
};

}  // namespace sen

#undef SEN_NATIVE_TYPE
#undef SEN_BUILTIN_NATIVE_TYPE
#undef SEN_INTEGRAL_TYPE
#undef SEN_REAL_TYPE
#undef SEN_IMPL_PARENT
#undef SEN_IMPL_IDENTITY

#endif  // SEN_CORE_META_NATIVE_TYPES_H
