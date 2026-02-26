// === sequence_traits.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_SEQUENCE_TRAITS_H
#define SEN_CORE_META_SEQUENCE_TRAITS_H

// sen
#include "sen/core/io/util.h"
#include "sen/core/meta/sequence_type.h"

namespace sen
{

struct Var;

/// \addtogroup traits
/// @{

/// Base class for sequence traits.
template <typename T>
struct SequenceTraitsBase
{
  static constexpr bool available = true;

  static void write(OutputStream& out, const T& val) { impl::writeSequence<T>(out, val); }
  static void read(InputStream& in, T& val) { impl::readSequence<T>(in, val); }
  static void valueToVariant(const T& val, Var& var) { impl::sequenceToVariant<T>(val, var); }
  static void variantToValue(const Var& var, T& val) { impl::variantToSequence<T>(var, val); }
  [[nodiscard]] static uint32_t serializedSize(const T& val) noexcept { return impl::sequenceSerializedSize<T>(val); }
};

/// Base class for sequence traits.
template <typename T>
struct ArrayTraitsBase
{
  static constexpr bool available = true;

  static void write(OutputStream& out, const T& val) { impl::writeArray(out, val); }
  static void read(InputStream& in, T& val) { impl::readArray<T>(in, val); }
  static void valueToVariant(const T& val, Var& var) { impl::arrayToVariant<T>(val, var); }
  static void variantToValue(const Var& var, T& val) { impl::variantToArray<T>(var, val); }
  [[nodiscard]] static uint32_t serializedSize(const T& val) noexcept { return impl::arraySerializedSize<T>(val); }
};

/// @}

}  // namespace sen

#endif  // SEN_CORE_META_SEQUENCE_TRAITS_H
