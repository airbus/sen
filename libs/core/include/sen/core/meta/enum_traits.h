// === enum_traits.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_ENUM_TRAITS_H
#define SEN_CORE_META_ENUM_TRAITS_H

// sen
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/var.h"

// std
#include <cstdint>
#include <string>
#include <type_traits>

namespace sen
{

struct Var;

/// \addtogroup traits
/// @{

/// Base class for enum traits.
template <typename T>
struct EnumTraitsBase
{
  static constexpr bool available = true;
  using StorageType = std::underlying_type_t<T>;

  static void write(OutputStream& out, T val) { impl::writeEnum<T>(out, val); }
  static void read(InputStream& in, T& val) { impl::readEnum<T>(in, val); }
  static void valueToVariant(const T& val, Var& var) { var = static_cast<StorageType>(val); }
  static void variantToValue(const Var& var, T& val) { impl::enumVariantToValue<T>(var, val); }
  [[nodiscard]] static uint32_t serializedSize(T val) noexcept { return impl::enumSerializedSize<T>(val); }

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

#endif  // SEN_CORE_META_ENUM_TRAITS_H
