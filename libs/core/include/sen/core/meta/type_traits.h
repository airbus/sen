// === type_traits.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_TYPE_TRAITS_H
#define SEN_CORE_META_TYPE_TRAITS_H

// sen
#include "sen/core/meta/type_utils.h"

// std
#include <type_traits>

namespace sen
{

/// \addtogroup traits
/// @{

template <typename T>
struct MetaTypeTrait
{
  // -- Provided Trait Interface --
  // static SpecificTypeInfo meta();

  static_assert(true, "MetaTypeTrait is not defined for type T.");
};

template <typename T>
struct VariantTraits
{
  // -- Provided Traits Interface --
  // static void valueToVariant(const classname& val, Var& var);
  // static void variantToValue(const Var& var, classname& val);

  // static std::function<lang::Value(const void*)> getFieldValueGetterFunction(Span<uint16_t> fields);
  // bool tryPrintField(std::ostream& out, const char* typeName, const T& val, bool requiresNewline);

  static_assert(true, "VariantTraits is not defined for type T.");
};

template <typename T>
struct SerializationTraits
{
  // -- Provided Traits Interface --
  // static void write(OutputStream& out, const T& val);
  // static void read(InputStream& in, T& val);
  // static uint32_t serializedSize(const T& val) noexcept;

  static_assert(true, "SerializationTraits is not defined for type T.");
};

template <typename T>
struct StringConversionTraits
{
  // -- Provided Traits Interface --
  // static std::string_view toString(T val);
  // static T fromString(std::string_view val);

  static_assert(true, "StringConversionTraits is not defined for type T.");
};

template <typename T>
struct QuantityTraits
{
  // -- Provided Traits Interface --
  // using ValueType = ...;

  // static constexpr ValueType max = ...;
  // static constexpr ValueType min = ...;

  // static ::std::optional<const sen::Unit*> unit() noexcept;
  // static const sen::Unit& unit(sen::Unit::EnsureTag);

  static_assert(true, "QuantityTraits is not defined for type T.");
};

/// Compile-time information about a generated (or built-in) type.
template <typename T>
struct SenClassRelation
{
  // -- Provided Traits Interface --
  using BaseType = void;                             ///< gives the base type corresponding to T
  static constexpr bool isBaseTypeTemplate = false;  ///< whether the base types' base class is a template parameter
  using InterfaceType = void;                        ///< gives the interface type corresponding to T
  using LocalProxyType = void;                       ///< gives the local proxy type corresponding to T
  using RemoteProxyType = void;                      ///< gives the remote proxy type corresponding to T

  static_assert(true, "SenClassRelation is not defined for type T.");
};

//===--------------------------------------------------------------------------------
// Specializations to support cvref qualified types.
template <typename T>
struct SenClassRelation<const T&>: private SenClassRelation<remove_cvref_t<T>>
{
private:
  using Base = SenClassRelation<remove_cvref_t<T>>;

public:
  using Base::BaseType;
  using Base::InterfaceType;
  using Base::isBaseTypeTemplate;
  using Base::LocalProxyType;
  using Base::RemoteProxyType;
};

template <typename T>
struct SenClassRelation<T&>: private SenClassRelation<remove_cvref_t<T>>
{
private:
  using Base = SenClassRelation<remove_cvref_t<T>>;

public:
  using Base::BaseType;
  using Base::InterfaceType;
  using Base::isBaseTypeTemplate;
  using Base::LocalProxyType;
  using Base::RemoteProxyType;
};

template <typename T>
struct SenClassRelation<const T>: private SenClassRelation<remove_cvref_t<T>>
{
private:
  using Base = SenClassRelation<remove_cvref_t<T>>;

public:
  using Base::BaseType;
  using Base::InterfaceType;
  using Base::isBaseTypeTemplate;
  using Base::LocalProxyType;
  using Base::RemoteProxyType;
};

/// Determines the interface type for the given type T.
template <typename T>
using SenInterfaceTypeT = typename SenClassRelation<T>::InterfaceType;

/// Checks if the given sen type has a templated base class.
template <typename T>
constexpr bool isSenBaseTypeTemplateV = SenClassRelation<T>::isBaseTypeTemplate;

namespace detail
{
template <typename T, typename BaseClassType, bool baseIsTemplate = isSenBaseTypeTemplateV<T>>
struct TemplatedBaseTypeDispatcher
{  // handles the base case where the base type is not a templated class
  using DefaultBaseType = typename SenClassRelation<T>::BaseType;
};

template <typename T, typename BaseClassType>
struct TemplatedBaseTypeDispatcher<T, BaseClassType, true>
{  // handles the case where the base type is a templated class
  using DefaultBaseType = typename SenClassRelation<T>::template BaseType<BaseClassType>;
  static_assert(!std::is_void_v<BaseClassType>, "No correct base class has been specified for T");
};
}  // namespace detail

/// Determines the base type for the given type T.
template <typename T, typename BaseClassType = void>
using SenBaseTypeT = typename detail::TemplatedBaseTypeDispatcher<T, BaseClassType>::DefaultBaseType;

/// Determines the local proxy type for the given type T.
template <typename T>
using SenLocalProxyTypeT = typename SenClassRelation<T>::LocalProxyType;

/// Determines the remote proxy type for the given type T.
template <typename T>
using SenRemoteProxyTypeT = typename SenClassRelation<T>::RemoteProxyType;

/// @}

}  // namespace sen

#endif  // SEN_CORE_META_TYPE_TRAITS_H
