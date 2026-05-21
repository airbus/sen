// === quantity_traits.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_QUANTITY_TRAITS_H
#define SEN_CORE_META_QUANTITY_TRAITS_H

// sen
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/type_traits.h"
#include "sen/core/meta/var.h"

// std
#include <cstdint>
#include <string>

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

  static std::string toJsonString(T val)
  {
    Var var;
    valueToVariant(val, var);
    return toJson(var);
  }

  static void fromJsonString(const std::string& str, T& val)
  {
    const Var var = fromJson(str);
    variantToValue(var, val);
  }
};

/// @}

}  // namespace sen

#endif  // SEN_CORE_META_QUANTITY_TRAITS_H
