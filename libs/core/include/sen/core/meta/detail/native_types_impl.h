// === native_types_impl.h =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_DETAIL_NATIVE_TYPES_IMPL_H
#define SEN_CORE_META_DETAIL_NATIVE_TYPES_IMPL_H

#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/hash32.h"
#include "sen/core/io/detail/serialization_traits.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/basic_traits.h"
#include "sen/core/meta/type.h"

namespace sen::impl
{

//--------------------------------------------------------------------------------------------------------------
// IntegralTypeBase
//--------------------------------------------------------------------------------------------------------------

/// Base class for integral types
template <typename T, typename Parent>
class IntegralTypeBase: public Parent
{
public:
  using NativeType = T;

public:
  explicit IntegralTypeBase(MemberHash hash) noexcept: Parent(hash) {}

public:
  [[nodiscard]] bool isSigned() const noexcept final { return std::numeric_limits<NativeType>::is_signed; }

  [[nodiscard]] bool hasInfinity() const noexcept final { return std::numeric_limits<NativeType>::has_infinity; }

  [[nodiscard]] bool isIEC559() const noexcept final { return std::numeric_limits<NativeType>::is_iec559; }

  [[nodiscard]] std::size_t getByteSize() const noexcept final { return sizeof(T); }
};

//--------------------------------------------------------------------------------------------------------------
// RealTypeBase
//--------------------------------------------------------------------------------------------------------------

/// Base class for floating point types
template <typename T, typename Parent>
class RealTypeBase: public Parent
{
public:
  using NativeType = T;

public:
  explicit RealTypeBase(MemberHash hash) noexcept: Parent(hash) {}

public:
  [[nodiscard]] bool isSigned() const noexcept final { return std::numeric_limits<NativeType>::is_signed; }

public:
  [[nodiscard]] bool hasInfinity() const noexcept final { return std::numeric_limits<NativeType>::has_infinity; }

  [[nodiscard]] bool isIEC559() const noexcept final { return std::numeric_limits<NativeType>::is_iec559; }

  [[nodiscard]] double getMaxValue() const noexcept final
  {
    return std_util::checkedConversion<double>(std::numeric_limits<NativeType>::max());
  }

  [[nodiscard]] double getMinValue() const noexcept final
  {
    return std_util::checkedConversion<double>(std::numeric_limits<NativeType>::lowest());
  }

  [[nodiscard]] double getEpsilon() const noexcept final
  {
    return std_util::checkedConversion<double>(std::numeric_limits<NativeType>::epsilon());
  }

  [[nodiscard]] std::size_t getByteSize() const noexcept final { return sizeof(NativeType); }
};

}  // namespace sen::impl

//--------------------------------------------------------------------------------------------------------------
// SEN_NATIVE_TRAITS
//--------------------------------------------------------------------------------------------------------------

/// NOLINTNEXTLINE
#define SEN_NATIVE_TRAITS(native, classname, readfunc, writefunc)                                                      \
  template <>                                                                                                          \
  struct MetaTypeTrait<native>                                                                                         \
  {                                                                                                                    \
    static inline ConstTypeHandle<classname> meta() noexcept { return classname::get(); }                              \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct VariantTraits<native>: private BasicTraits<native>                                                            \
  {                                                                                                                    \
    using BasicTraits<native>::valueToVariant;                                                                         \
    using BasicTraits<native>::variantToValue;                                                                         \
  };                                                                                                                   \
  template <>                                                                                                          \
  struct SerializationTraits<native>: private BasicTraits<native>                                                      \
  {                                                                                                                    \
    static inline void write(OutputStream& out, native val) { out.writefunc(val); }                                    \
    static inline void read(InputStream& in, native& val) { in.readfunc(val); }                                        \
    using BasicTraits<native>::serializedSize;                                                                         \
  }

//--------------------------------------------------------------------------------------------------------------
// SEN_BUILTIN_NATIVE_TYPE
//--------------------------------------------------------------------------------------------------------------

/// NOLINTNEXTLINE
#define SEN_BUILTIN_NATIVE_TYPE(classname, native, name, description, readfunc, writefunc, is_bounded)                 \
  class classname final: public ::sen::NativeType                                                                      \
  {                                                                                                                    \
    SEN_META_TYPE(classname)                                                                                           \
                                                                                                                       \
  public:                                                                                                              \
    using NativeType = native;                                                                                         \
                                                                                                                       \
    classname() noexcept: ::sen::NativeType(hashCombine(hashSeed, std::string(name))) {}                               \
    ~classname() noexcept final = default;                                                                             \
                                                                                                                       \
    [[nodiscard]] std::string_view getName() const noexcept final { return name; }                                     \
                                                                                                                       \
    [[nodiscard]] std::string_view getDescription() const noexcept final { return description; }                       \
                                                                                                                       \
    [[nodiscard]] bool equals(const Type& other) const noexcept final { return other.is##classname(); }                \
                                                                                                                       \
    [[nodiscard]] static inline sen::TypeHandle<const classname> get() noexcept                                        \
    {                                                                                                                  \
      static classname instance;                                                                                       \
      return sen::makeNonOwningTypeHandle(&instance);                                                                  \
    }                                                                                                                  \
                                                                                                                       \
    [[nodiscard]] inline bool isBounded() const noexcept final { return is_bounded; }                                  \
  };                                                                                                                   \
  SEN_NATIVE_TRAITS(native, classname, readfunc, writefunc)

/// NOLINTNEXTLINE
#define SEN_IMPL_PARENT(native, baseclass, parentclass) baseclass<native, parentclass>

/// NOLINTNEXTLINE
#define SEN_IMPL_IDENTITY(thing) thing

//--------------------------------------------------------------------------------------------------------------
// SEN_NATIVE_TYPE
//--------------------------------------------------------------------------------------------------------------

/// NOLINTNEXTLINE
#define SEN_NATIVE_TYPE(classname, native, name, description, baseclass, parentclass, readfunc, writefunc)             \
  class classname final: public SEN_IMPL_IDENTITY(SEN_IMPL_PARENT(native, baseclass, parentclass))                     \
  {                                                                                                                    \
    SEN_META_TYPE(classname)                                                                                           \
                                                                                                                       \
  public:                                                                                                              \
    classname() noexcept                                                                                               \
      : SEN_IMPL_PARENT(native, baseclass, parentclass)(hashCombine(hashSeed, std::string(#name))) {}                  \
    ~classname() noexcept final = default;                                                                             \
                                                                                                                       \
  public:                                                                                                              \
    [[nodiscard]] std::string_view getName() const noexcept final { return #name; }                                    \
                                                                                                                       \
    [[nodiscard]] std::string_view getDescription() const noexcept final { return description; }                       \
                                                                                                                       \
    [[nodiscard]] bool equals(const Type& other) const noexcept final { return other.is##classname(); }                \
                                                                                                                       \
    [[nodiscard]] static inline sen::TypeHandle<const classname> get() noexcept                                        \
    {                                                                                                                  \
      static classname instance;                                                                                       \
      return sen::makeNonOwningTypeHandle(&instance);                                                                  \
    }                                                                                                                  \
  };                                                                                                                   \
  SEN_NATIVE_TRAITS(native, classname, readfunc, writefunc)

/// Helper to define integral types. Cannot be used outside this library.
/// NOLINTNEXTLINE
#define SEN_INTEGRAL_TYPE(classname, native, name, description, readfunc, writefunc)                                   \
  SEN_NATIVE_TYPE(classname, native, name, description, impl::IntegralTypeBase, IntegralType, readfunc, writefunc)

/// Helper to define floating point types. Cannot be used outside this library.
/// NOLINTNEXTLINE
#define SEN_REAL_TYPE(classname, native, name, description, readfunc, writefunc)                                       \
  SEN_NATIVE_TYPE(classname, native, name, description, impl::RealTypeBase, RealType, readfunc, writefunc)

#endif  // SEN_CORE_META_DETAIL_NATIVE_TYPES_IMPL_H
