// === enum_traits.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_ENUM_TRAITS_H
#define SEN_CORE_META_ENUM_TRAITS_H

#include "sen/core/meta/enum_type.h"

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
};

/// @}

}  // namespace sen

#endif  // SEN_CORE_META_ENUM_TRAITS_H
