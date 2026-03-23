// === basic_traits.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_BASIC_TRAITS_H
#define SEN_CORE_META_BASIC_TRAITS_H

#include "sen/core/base/duration.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/detail/serialization_traits.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/type.h"

namespace sen
{

struct Var;

/// \addtogroup traits
/// @{

/// Mixin base for traits classes whose type is a native or trivially-serialisable scalar.
///
/// Inherit from this in a `*Traits<T>` specialisation to get a default implementation of
/// `variantToValue`, `valueToVariant`, and `serializedSize`:
/// @code{.cpp}
/// template <>
/// struct MyTraits<int32_t>: public BasicTraits<int32_t> {};
/// @endcode
///
/// @tparam T Native or trivially-copyable type to provide default trait operations for.
template <typename T>
struct BasicTraits
{
  static constexpr bool available = true;  ///< Marks the type as having a complete traits implementation.

  /// Extracts a value of type `T` from a `Var`.
  /// @param var Source variant value.
  /// @param val Output reference that receives the extracted value.
  static void variantToValue(const Var& var, T& val) { val = getCopyAs<T>(var); }

  /// Stores a value of type `T` into a `Var`.
  /// @param val Value to store.
  /// @param var Output variant that receives the value.
  static void valueToVariant(T val, Var& var) { var = val; }

  /// @param val Value whose serialised byte count is requested.
  /// @return Number of bytes required to serialise `val`.
  static constexpr uint32_t serializedSize(T val) noexcept { return impl::getSerializedSize(val); }
};

/// @}

}  // namespace sen

#endif  // SEN_CORE_META_BASIC_TRAITS_H
