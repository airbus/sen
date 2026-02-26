// === optional_traits.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_OPTIONAL_TRAITS_H
#define SEN_CORE_META_OPTIONAL_TRAITS_H

// sen
#include "sen/core/io/detail/serialization_traits.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/optional_type.h"

// std
#include <optional>

namespace sen
{

struct Var;

/// \addtogroup traits
/// @{

/// Base class for enum traits.
template <typename T>
struct OptionalTraitsBase
{
  static constexpr bool available = true;

  static void write(OutputStream& out, T val);
  static void read(InputStream& in, T& val);
  static void valueToVariant(const T& val, Var& var);
  static void variantToValue(const Var& var, T& val);
  [[nodiscard]] static uint32_t serializedSize(T val) noexcept;
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

template <typename T>
inline void OptionalTraitsBase<T>::write(OutputStream& out, T val)
{
  auto hasValue = val.has_value();
  out.writeBool(hasValue);
  if (hasValue)
  {
    SerializationTraits<typename T::value_type>::write(out, *val);
  }
}

template <typename T>
inline void OptionalTraitsBase<T>::read(InputStream& in, T& val)
{
  bool hasValue = false;
  in.readBool(hasValue);

  if (hasValue)
  {
    typename T::value_type content;
    SerializationTraits<typename T::value_type>::read(in, content);
    val = T(content);
  }
  else
  {
    val = T(std::nullopt);
  }
}

template <typename T>
inline uint32_t OptionalTraitsBase<T>::serializedSize(T val) noexcept
{
  auto hasValue = val.has_value();
  auto result = impl::getSerializedSize(hasValue);

  if (hasValue)
  {
    result += SerializationTraits<typename T::value_type>::serializedSize(*val);
  }

  return result;
}

template <typename T>
inline void OptionalTraitsBase<T>::valueToVariant(const T& val, Var& var)
{
  if (val.has_value())
  {
    VariantTraits<typename T::value_type>::valueToVariant(*val, var);
  }
  else
  {
    var = {};
  }
}

template <typename T>
inline void OptionalTraitsBase<T>::variantToValue(const Var& var, T& val)
{
  // the var might be empty or hold an empty map
  if (var.isEmpty() || (var.holds<VarMap>() && var.get<VarMap>().empty()))
  {
    val = T(std::nullopt);
  }
  else
  {
    typename T::value_type content;
    VariantTraits<typename T::value_type>::variantToValue(var, content);
    val = T(content);
  }
}

}  // namespace sen

#endif  // SEN_CORE_META_OPTIONAL_TRAITS_H
