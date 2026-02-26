// === class_helpers.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_CLASS_HELPERS_H
#define SEN_CORE_BASE_CLASS_HELPERS_H

#include "sen/core/base/compiler_macros.h"

#include <type_traits>
#include <utility>
#include <variant>

/// \file class_helpers.h
/// Here we define a set of template meta-programming helpers to let the compiler take some
/// decisions based on what it knows about the types when instantiated.

namespace sen
{

//--------------------------------------------------------------------------------------------------------------
// Metaprogramming helpers
//--------------------------------------------------------------------------------------------------------------

/// \addtogroup templates
/// @{

/// Utility to indicate that your class wants to be passed by value in some of the library calls.
template <typename T>
struct ShouldBePassedByValue: std::false_type
{
};

template <typename T>
inline constexpr bool shouldBePassedByValueV = ShouldBePassedByValue<T>::value;

/// returns 'const T&'
template <typename T>
using AddConstRef = std::add_lvalue_reference_t<std::add_const_t<T>>;

/// returns 'const T&' or 'T' depending on the type
template <typename T>
using MaybeRef = std::conditional_t<std::is_arithmetic_v<T> || shouldBePassedByValueV<T>, T, AddConstRef<T>>;

/// return false if T has not an 'using ValueType' or a 'typedef ValueType'
template <typename, typename = std::void_t<>>
struct HasValueType: std::false_type
{
};

/// return true if T has an 'using ValueType' or a 'typedef ValueType'
template <typename T>
struct HasValueType<T, std::void_t<typename T::ValueType>>: std::true_type
{
};

/// Allows compile-time check for the presence of various operators.
template <typename S, typename T = S>
struct HasOperator
{
private:
  template <typename SS, typename TT>
  static auto eqTest(unsigned) -> decltype(std::declval<SS&>() == std::declval<TT>(), std::true_type());

  template <typename, typename>
  static auto eqTest(...) -> std::false_type;

  template <typename SS, typename TT>
  static auto neTest(unsigned) -> decltype(std::declval<SS&>() != std::declval<TT>(), std::true_type());

  template <typename, typename>
  static auto neTest(...) -> std::false_type;

  template <typename SS, typename TT>
  static auto ltTest(unsigned) -> decltype(std::declval<SS&>() < std::declval<TT>(), std::true_type());

  template <typename, typename>
  static auto ltTest(...) -> std::false_type;

  template <typename SS, typename TT>
  static auto leTest(unsigned) -> decltype(std::declval<SS&>() <= std::declval<TT>(), std::true_type());

  template <typename, typename>
  static auto leTest(...) -> std::false_type;

  template <typename SS, typename TT>
  static auto gtTest(unsigned) -> decltype(std::declval<SS&>() > std::declval<TT>(), std::true_type());

  template <typename, typename>
  static auto gtTest(...) -> std::false_type;

  template <typename SS, typename TT>
  static auto geTest(unsigned) -> decltype(std::declval<SS&>() >= std::declval<TT>(), std::true_type());

  template <typename, typename>
  static auto geTest(...) -> std::false_type;

  template <typename SS, typename TT>
  static auto mulTest(unsigned) -> decltype(std::declval<SS&>() * std::declval<TT&>(), std::true_type());

  template <typename, typename>
  static auto mulTest(...) -> std::false_type;

  template <typename SS>
  static auto incTest(unsigned) -> decltype(std::declval<SS&>()++, std::true_type());

  template <typename>
  static auto incTest(...) -> std::false_type;

  template <typename SS>
  static auto decTest(unsigned) -> decltype(std::declval<SS&>()--, std::true_type());

  template <typename>
  static auto decTest(...) -> std::false_type;

  template <typename SS, typename TT>
  static auto modTest(unsigned) -> decltype(std::declval<SS&>() % std::declval<TT&>(), std::true_type());

  template <typename, typename>
  static auto modTest(...) -> std::false_type;

public:
  static constexpr bool eq = decltype(eqTest<S, T>(0U))::value;
  static constexpr bool ne = decltype(neTest<S, T>(0U))::value;
  static constexpr bool lt = decltype(ltTest<S, T>(0U))::value;
  static constexpr bool le = decltype(leTest<S, T>(0U))::value;
  static constexpr bool gt = decltype(gtTest<S, T>(0U))::value;
  static constexpr bool ge = decltype(geTest<S, T>(0U))::value;
  static constexpr bool mul = decltype(mulTest<S, T>(0U))::value;
  static constexpr bool inc = decltype(incTest<S>(0U))::value;
  static constexpr bool dec = decltype(decTest<S>(0U))::value;
  static constexpr bool mod = decltype(modTest<S, T>(0U))::value;
};

/// Helper type for std::variant lambda visitors.
///
/// Example:
/// \code
/// std::visit(
///      sen::Overloaded {[&](uint8_t val) { ... },
///                       [&](int16_t val) { ... },
///                       [&](uint16_t val) { ... },
///                       [&](int32_t val) { ... },
///                       [&](uint32_t val) { ... },
///                       [&](int64_t val) { ... },
///                       [&](uint64_t val) { ... },
///                       [&](float32_t val) { ... },
///                       [&](float64_t val) { ... }},
///      variant);
///  };
///
/// \endcode
///
/// You need to handle all the cases for it to compile. You can also use auto
/// if you need to generalize it:
///
/// \code
/// std::visit(
///      sen::Overloaded {[&](uint8_t val) { doThis(val); },
///                       [&](int16_t val) { doThat(val); },
///                       [&](auto val) { doSomethingElse(val); }},
///      variant);
///  }
/// \endcode
template <class... Ts>
struct Overloaded: Ts...
{
  using Ts::operator()...;
};

/// Explicit deduction guide (not needed as of C++20).
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

/// Checks if T is on of the types in TypeList.
template <typename T, typename... TypeList>
struct IsContained: public std::disjunction<std::is_same<T, TypeList>...>
{
};

/// Checks if T is on of the member types of the given VariantType.
template <typename T, typename VariantType>
struct IsVariantMember;

/// Checks if T is on of the member types of the given VariantType.
template <typename T, typename... MemberTypes>
struct IsVariantMember<T, std::variant<MemberTypes...>>: public IsContained<T, MemberTypes...>
{
};

/// @}

}  // namespace sen

#endif  // SEN_CORE_BASE_CLASS_HELPERS_H
