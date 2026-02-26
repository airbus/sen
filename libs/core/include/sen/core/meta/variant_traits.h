// === variant_traits.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_VARIANT_TRAITS_H
#define SEN_CORE_META_VARIANT_TRAITS_H

// sen
#include "sen/core/base/span.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/basic_traits.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/variant_type.h"

// std
#include <cstring>
#include <iomanip>
#include <tuple>

namespace sen
{

struct Var;

/// \addtogroup traits
/// @{

struct VariantTraitsBaseBase
{
protected:
  [[noreturn]] static void throwEmptyStructError(const char* name);
  static void expectAtLeastOneField(const char* name, const Span<uint16_t>& fields);
  [[noreturn]] static void throwNonNativeField(const char* variantName, const char* fieldName);
  [[noreturn]] static void throwInvalidFieldIndex(const char* variantName, uint16_t index);
  [[nodiscard]] static std::tuple<const char*, const Var*> getTypeAndValue(const Var& var,
                                                                           ConstTypeHandle<VariantType> meta);
};

/// Base class for variant traits.
template <typename T>
struct VariantTraitsBase: public VariantTraitsBaseBase
{
  static constexpr bool available = true;

  template <std::size_t index>
  [[nodiscard]] static bool tryPrintField(std::ostream& out, const char* typeName, const T& val, bool requiresNewline);

protected:
  template <std::size_t index>
  [[nodiscard]] static bool assignFieldByName(const char* currentType,
                                              const char* expectedTypeLong,
                                              const char* expectedTypeShort,
                                              const char* expectedAliasTypeShort,
                                              const Var& fieldValue,
                                              T& val);

  template <std::size_t index>
  static void assignField(const Var& fieldValue, T& val);

  template <std::size_t index>
  [[nodiscard]] static bool getFieldSerializedSize(const T& val, uint32_t key, uint32_t& result);

  template <std::size_t index>
  [[nodiscard]] static bool tryWriteField(OutputStream& out, const T& val, uint32_t key);

  template <std::size_t index>
  static void readField(InputStream& in, T& val);

  template <std::size_t index>
  [[nodiscard]] static bool tryFieldValueToVariant(const T& val, Var& var, std::shared_ptr<Var> valueVar, uint32_t key);
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

template <typename T>
template <std::size_t index>
inline void VariantTraitsBase<T>::assignField(const Var& fieldValue, T& val)
{
  using F = std::variant_alternative_t<index, T>;
  F fieldVal {};
  VariantTraits<F>::variantToValue(fieldValue, fieldVal);
  val.template emplace<index>(std::move(fieldVal));
}

template <typename T>
template <std::size_t index>
inline bool VariantTraitsBase<T>::assignFieldByName(const char* currentType,
                                                    const char* expectedTypeLong,
                                                    const char* expectedTypeShort,
                                                    const char* expectedAliasTypeShort,
                                                    const Var& fieldValue,
                                                    T& val)
{
  if (strcmp(currentType, expectedTypeShort) == 0U || strcmp(currentType, expectedTypeLong) == 0U ||
      (expectedAliasTypeShort != nullptr && strcmp(currentType, expectedAliasTypeShort) == 0U))
  {
    assignField<index>(fieldValue, val);
    return true;
  }
  return false;
}

template <typename T>
template <std::size_t index>
inline bool VariantTraitsBase<T>::getFieldSerializedSize(const T& val, uint32_t key, uint32_t& result)
{
  using F = std::variant_alternative_t<index, T>;
  if (val.index() == index)
  {
    result =
      SerializationTraits<uint32_t>::serializedSize(key) + SerializationTraits<F>::serializedSize(std::get<index>(val));
    return true;
  }
  return false;
}

template <typename T>
template <std::size_t index>
inline bool VariantTraitsBase<T>::tryWriteField(OutputStream& out, const T& val, uint32_t key)
{
  using F = std::variant_alternative_t<index, T>;
  if (val.index() == index)
  {
    out.writeUInt32(key);
    SerializationTraits<F>::write(out, std::get<index>(val));
    return true;
  }
  return false;
}

template <typename T>
template <std::size_t index>
inline void VariantTraitsBase<T>::readField(InputStream& in, T& val)
{
  using F = std::variant_alternative_t<index, T>;
  F fieldVal;
  SerializationTraits<F>::read(in, fieldVal);
  val.template emplace<index>(std::move(fieldVal));
}

template <typename T>
template <std::size_t index>
inline bool VariantTraitsBase<T>::tryFieldValueToVariant(const T& val,
                                                         Var& var,
                                                         std::shared_ptr<Var> valueVar,
                                                         uint32_t key)
{
  using F = std::variant_alternative_t<index, T>;
  if (val.index() == index)
  {
    VariantTraits<F>::valueToVariant(std::get<index>(val), *valueVar);
    var = KeyedVar {key, std::move(valueVar)};
    return true;
  }
  return false;
}

template <typename T>
template <std::size_t index>
inline bool VariantTraitsBase<T>::tryPrintField(std::ostream& out,
                                                const char* typeName,
                                                const T& val,
                                                bool requiresNewline)
{
  if (val.index() == index)
  {
    out.setf(std::ios::left, std::ios::adjustfield);
    const auto indent = std::setw(static_cast<int>(out.width() + 2U));
    out << indent << ' ' << "type:  " << typeName << "\n";

    if (auto valPtr = std::get_if<index>(&val); valPtr)
    {
      if (requiresNewline)
      {
        out << indent << ' ' << "value:\n" << indent << *valPtr;
      }
      else
      {
        out << indent << ' ' << "value: " << *valPtr;
      }
    }
    return true;
  }
  return false;
}

}  // namespace sen

#endif  // SEN_CORE_META_VARIANT_TRAITS_H
