// === quantity_traits.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_QUANTITY_TRAITS_H
#define SEN_CORE_META_QUANTITY_TRAITS_H

#include "sen/core/io/util.h"
#include "sen/core/meta/quantity_type.h"

namespace sen
{

struct Var;

/// \addtogroup traits
/// @{

/// Base class for quantity traits.
template <typename T>
struct QuantityTraitsBase
{
  using ValueType = typename T::ValueType;

  static constexpr bool available = true;

  static void write(OutputStream& out, T val) { impl::writeQuantity<T>(out, val); }
  static void read(InputStream& in, T& val) { impl::readQuantity<T>(in, val); }
  static void valueToVariant(T val, Var& var) { impl::quantityToVariant<T>(val, var); }
  static void variantToValue(const Var& var, T& val) { impl::variantToQuantity<T>(var, val); }
  [[nodiscard]] static uint32_t serializedSize(const T& val) noexcept
  {
    return SerializationTraits<ValueType>::serializedSize(val.get());
  }
};

/// @}

}  // namespace sen

#endif  // SEN_CORE_META_QUANTITY_TRAITS_H
