// === type.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_TYPE_H
#define SEN_CORE_META_TYPE_H

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/hash32.h"
#include "sen/core/base/result.h"
#include "sen/core/base/strong_type.h"
#include "sen/core/base/type_traits_extra.h"
#include "sen/core/meta/detail/type_impl.h"
#include "sen/core/meta/detail/types_fwd.h"

// std
#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>
#include <variant>

namespace sen
{

/// \addtogroup types
/// @{

/// The hash of a member.
struct MemberHash: public StrongType<uint32_t, MemberHash>
{
  using StrongType<uint32_t, MemberHash>::StrongType;
};

template <>
struct ShouldBePassedByValue<MemberHash>: std::true_type
{
};

template <>
struct impl::hash<MemberHash>
{
  inline u32 operator()(const ::sen::MemberHash& x) const noexcept { return hashIntegral(x.get()); }
};

/// How to transport information.
enum class TransportMode : uint8_t
{
  unicast = 0U,    ///< Directed to each receiver, unreliable, unordered, no congestion control
  multicast = 1U,  ///< Directed to all receivers, unreliable, unordered, no congestion control
  confirmed = 2U,  ///< Directed to each receiver, reliable, ordered, with congestion control, relatively heavyweight
};

// forward declarations
struct Var;

/// Represents a type that can be used to define variables and
/// arguments for methods or functions. Types use the GoF
/// Visitor pattern in order to allow easy and fast operation,
/// as double dispatching is faster and cleaner than a
/// (dynamic) cast-based approach.
class Type
{
public:  // special members
  SEN_NOCOPY_NOMOVE(Type)

public:
  virtual ~Type() = default;

public:
  /// The type name.
  [[nodiscard]] virtual std::string_view getName() const noexcept = 0;

  /// The type documentation.
  [[nodiscard]] virtual std::string_view getDescription() const noexcept = 0;

  /// Accepts a type visitor.
  virtual void accept(FullTypeVisitor& tv) const = 0;

  /// True if values of this type have a bounded memory footprint (do not grow or shrink).
  [[nodiscard]] virtual bool isBounded() const noexcept = 0;

  /// Returns the unique hash computed for the type at compile time
  [[nodiscard]] MemberHash getHash() const noexcept;

public:
  /// Checks that a lower case name (used for fields, enums, etc) is valid.
  static Result<void, std::string> validateLowerCaseName(std::string_view name);

  /// Checks that a upper case name (used for type names) is valid.
  static Result<void, std::string> validateTypeName(std::string_view name);

public:
  // Declaration of:
  //
  //   bool isX() const noexcept;
  //   virtual const X* asX() const noexcept;
  //
  // Where X is defined for all supported types. It returns a valid
  // pointer if the subclass is of type X.

  // native types
  DECL_IS_TYPE_FUNC(NativeType)
  DECL_IS_TYPE_FUNC(VoidType)
  DECL_IS_TYPE_FUNC(BoolType)
  DECL_IS_TYPE_FUNC(NumericType)
  DECL_IS_TYPE_FUNC(IntegralType)
  DECL_IS_TYPE_FUNC(RealType)
  DECL_IS_TYPE_FUNC(Float32Type)
  DECL_IS_TYPE_FUNC(Float64Type)
  DECL_IS_TYPE_FUNC(StringType)
  DECL_IS_TYPE_FUNC(UInt8Type)
  DECL_IS_TYPE_FUNC(Int16Type)
  DECL_IS_TYPE_FUNC(UInt16Type)
  DECL_IS_TYPE_FUNC(Int32Type)
  DECL_IS_TYPE_FUNC(UInt32Type)
  DECL_IS_TYPE_FUNC(Int64Type)
  DECL_IS_TYPE_FUNC(UInt64Type)
  DECL_IS_TYPE_FUNC(DurationType)
  DECL_IS_TYPE_FUNC(TimestampType)

  // custom types
  DECL_IS_TYPE_FUNC(CustomType)
  DECL_IS_TYPE_FUNC(SequenceType)
  DECL_IS_TYPE_FUNC(StructType)
  DECL_IS_TYPE_FUNC(EnumType)
  DECL_IS_TYPE_FUNC(ClassType)
  DECL_IS_TYPE_FUNC(VariantType)
  DECL_IS_TYPE_FUNC(QuantityType)
  DECL_IS_TYPE_FUNC(AliasType)
  DECL_IS_TYPE_FUNC(OptionalType)

public:
  [[nodiscard]] virtual bool equals(const Type& other) const noexcept = 0;

  /// Returns true if the lhs type is the same as the rhs type.
  friend bool operator==(const Type& lhs, const Type& rhs) { return lhs.equals(rhs); }

  /// Returns true if the lhs type is not the same as the rhs type.
  friend bool operator!=(const Type& lhs, const Type& rhs) { return !lhs.equals(rhs); }

protected:
  explicit Type(MemberHash hash) noexcept;

private:
  MemberHash hash_;
};

/// Handle around a type that, by default, ensures the lifetime of the underlying type.
///
/// Note: A type handle always contains a type (i.e., there are no empty handles).
template <typename SenTypeType>
class TypeHandle final
{
  using RawPtrType = SenTypeType*;
  using ManagedPtrType = std::shared_ptr<SenTypeType>;
  using StorageType = std::variant<RawPtrType, ManagedPtrType>;

private:
  template <typename T>
  explicit TypeHandle(T* type): type_(type)
  {
    SEN_ASSERT(type != nullptr && "handles should not be to nullptrs");
  }

public:
  template <typename T>
  friend TypeHandle<T> makeNonOwningTypeHandle(T* type);

  template <
    typename ArgT,
    typename... ArgTypes,
    std::enable_if_t<std::negation_v<sen::std_util::IsInstantiationOf<TypeHandle, std::decay_t<ArgT>>>, bool> = true>
  // NOLINTNEXTLINE(hicpp-explicit-conversions): implicit conversion wanted here
  TypeHandle(ArgT&& arg, ArgTypes&&... args)
    : type_(std::make_shared<SenTypeType>(std::forward<ArgT>(arg), std::forward<ArgTypes>(args)...))
  {
  }

  template <typename T>
  // NOLINTNEXTLINE(hicpp-explicit-conversions): implicit conversion wanted here
  TypeHandle(std::shared_ptr<T> type): type_(std::move(type))
  {
    SEN_ASSERT(std::get<ManagedPtrType>(type_).get() != nullptr && "Handles should not be to nullptrs");
  }

  template <typename OtherSenTypeType>
  // NOLINTNEXTLINE(hicpp-explicit-conversions): implicit conversion wanted here
  TypeHandle(const TypeHandle<OtherSenTypeType>& otherHandle)
    : type_(std::visit(sen::Overloaded {[](OtherSenTypeType* ptr) -> StorageType { return {ptr}; },
                                        [](std::shared_ptr<OtherSenTypeType> sharedPtr) -> StorageType
                                        { return {std::move(sharedPtr)}; }},
                       otherHandle.type_))
  {
  }

  template <typename OtherSenTypeType>
  // NOLINTNEXTLINE(hicpp-explicit-conversions): implicit conversion wanted here
  TypeHandle(TypeHandle<OtherSenTypeType>&& otherHandle)
    : type_(std::visit(sen::Overloaded {[](OtherSenTypeType* ptr) -> StorageType { return {ptr}; },
                                        [](std::shared_ptr<OtherSenTypeType> sharedPtr) -> StorageType
                                        { return {std::move(sharedPtr)}; }},
                       otherHandle.type_))
  {
  }

  SenTypeType& operator*() { return *type(); }
  const SenTypeType& operator*() const { return *type(); }

  SenTypeType* operator->() { return type(); }
  const SenTypeType* operator->() const { return type(); }

  SenTypeType* type()
  {
    if (std::holds_alternative<ManagedPtrType>(type_))
    {
      return std::get<ManagedPtrType>(type_).get();
    }

    return std::get<RawPtrType>(type_);
  }

  const SenTypeType* type() const
  {
    if (std::holds_alternative<ManagedPtrType>(type_))
    {
      return std::get<ManagedPtrType>(type_).get();
    }

    return std::get<RawPtrType>(type_);
  }

  //===--------------------------------------------------===//
  // Comparisons
  //===--------------------------------------------------===//
  template <typename U>
  friend bool operator==(const TypeHandle& lhs, const TypeHandle<U>& rhs)
  {
    return *lhs == *rhs;
  }
  friend bool operator==(const TypeHandle& lhs, const Type& rhs) { return *lhs == rhs; }
  friend bool operator==(const Type& lhs, const TypeHandle& rhs) { return lhs == *rhs; }

  template <typename U>
  friend bool operator!=(const TypeHandle& lhs, const TypeHandle<U>& rhs)
  {
    return *lhs != *rhs;
  }
  friend bool operator!=(const TypeHandle& lhs, const Type& rhs) { return *lhs != rhs; }
  friend bool operator!=(const Type& lhs, const TypeHandle& rhs) { return lhs != *rhs; }

private:
  StorageType type_;

  template <typename T>
  friend class TypeHandle;

  template <typename T, typename U>
  friend std::optional<TypeHandle<T>> dynamicTypeHandleCast(const TypeHandle<U>&);
};

template <typename T>
TypeHandle(std::shared_ptr<T> type) -> TypeHandle<T>;

/// Creates a non-owning type handle from a type pointer.
///
/// Note: This is an explicit opt-out of the type-lifetime management system of the TypeHandle. That is, the user is
/// responsible for ensuring a long enough lifetime of the underlying type (e.g., through a object with static storage
/// duration).
///
/// @param typePtr: which should be wrapped by the handle
template <typename T>
TypeHandle<T> makeNonOwningTypeHandle(T* typePtr)
{  // explicit factory function to prevent accidental creation/convertion from a type *
  return TypeHandle<T> {typePtr};
}

/// Downcasts the type inside the type handle to the specified one, if possible.
template <typename T, typename U>
std::optional<TypeHandle<T>> dynamicTypeHandleCast(const TypeHandle<U>& handle)
{
  // Note: Implemented as non-member functions to mirror the design of *_pointer_cast

  // Handle managed ptr
  if (std::holds_alternative<typename TypeHandle<U>::ManagedPtrType>(handle.type_))
  {
    if (auto convertedSharedPtr = std::dynamic_pointer_cast<typename TypeHandle<T>::ManagedPtrType::element_type>(
          std::get<typename TypeHandle<U>::ManagedPtrType>(handle.type_)))
    {
      return {TypeHandle(std::move(convertedSharedPtr))};
    }

    return std::nullopt;
  }

  // Handle unmanaged raw ptr
  if (auto convertedPtr =
        dynamic_cast<typename TypeHandle<T>::RawPtrType>(std::get<typename TypeHandle<U>::RawPtrType>(handle.type_)))
  {
    return {sen::makeNonOwningTypeHandle(convertedPtr)};
  }

  return std::nullopt;
}

/// Utility typedef.
template <typename T>
using ConstSharedPtr = std::shared_ptr<const T>;

template <typename T = Type>
using ConstTypeHandle = TypeHandle<const T>;

template <typename T = Type>
using MaybeConstTypeHandle = std::optional<ConstTypeHandle<T>>;

/// @}

}  // namespace sen

namespace std
{

template <typename Type>
struct hash<sen::TypeHandle<Type>>  // NOLINT(cert-dcl58-cpp): specializing std::hash is allowed
{
  size_t operator()(const sen::TypeHandle<Type>& typeHandle) const noexcept { return typeHandle->getHash().get(); }
};

}  // namespace std

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

#undef DECL_IS_TYPE_FUNC

/// Helper macro to implement some type methods.
/// Only to be used within this library.
/// NOLINTNEXTLINE
#define SEN_META_TYPE(classname)                                                                                       \
public:                                                                                                                \
  SEN_NOCOPY_NOMOVE(classname)                                                                                         \
                                                                                                                       \
public:                                                                                                                \
  void accept(FullTypeVisitor& tv) const override { tv.apply(*this); }                                                 \
                                                                                                                       \
  [[nodiscard]] const classname* as##classname() const noexcept override { return this; }                              \
                                                                                                                       \
private:

namespace std
{

template <>
struct hash<::sen::MemberHash>
{
  size_t operator()(const ::sen::MemberHash& x) const noexcept
  {
    return std::hash<::sen::MemberHash::ValueType>()(x.get());
  }
};

}  // namespace std

#endif  // SEN_CORE_META_TYPE_H
