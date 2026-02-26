// === util.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_IO_UTIL_H
#define SEN_CORE_IO_UTIL_H

// sen
#include "sen/core/base/result.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_traits.h"
#include "sen/core/meta/unit.h"
#include "sen/core/meta/var.h"

// std
#include <optional>
#include <string>

namespace sen
{

// Forward declarations
class Callable;
class Property;
class Type;

namespace impl
{

//--------------------------------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------------------------------

template <typename T>
void structFieldToVarMap(const char* name, const T& value, VarMap& map);

template <typename T>
void extractStructFieldFromMap(const char* name, T& value, const VarMap* map);

//--------------------------------------------------------------------------------------------------------------
// Sequences
//--------------------------------------------------------------------------------------------------------------

template <typename T>
void writeSequence(sen::OutputStream& out, const T& val);

template <typename T>
void writeArray(sen::OutputStream& out, const T& val);

template <typename T>
void readSequence(sen::InputStream& in, T& val);

template <typename T>
void readArray(sen::InputStream& in, T& val);

template <typename T>
[[nodiscard]] uint32_t sequenceSerializedSize(const T& val) noexcept;

template <typename T>
[[nodiscard]] uint32_t arraySerializedSize(const T& val) noexcept;

template <typename T>
void sequenceToVariant(const T& val, sen::Var& var);

template <typename T>
void arrayToVariant(const T& val, sen::Var& var);

template <typename T>
void variantToSequence(const sen::Var& var, T& val);

template <typename T>
void variantToArray(const sen::Var& var, T& val);

//--------------------------------------------------------------------------------------------------------------
// Quantity
//--------------------------------------------------------------------------------------------------------------

template <typename T>
void writeQuantity(sen::OutputStream& out, const T& val);

template <typename T>
void readQuantity(sen::InputStream& in, T& val);

template <typename T>
void quantityToVariant(const T& val, Var& var);

template <typename T>
void variantToQuantity(const Var& var, T& val);

//--------------------------------------------------------------------------------------------------------------
// Enum
//--------------------------------------------------------------------------------------------------------------

template <typename T>
void readEnum(sen::InputStream& in, T& val);

template <typename T>
void writeEnum(sen::OutputStream& out, T val);

template <typename T>
[[nodiscard]] uint32_t enumSerializedSize(T val);

template <typename T>
void enumVariantToValue(const Var& var, T& val);

//--------------------------------------------------------------------------------------------------------------
// Variant
//--------------------------------------------------------------------------------------------------------------

template <typename F, typename V>
void readVariantField(sen::InputStream& in, V& val);

template <typename F, typename V>
uint32_t getVariantFieldSerializedSize(uint32_t key, const V& val);

void readFromStream(Var& var, InputStream& in, const Type& type);

void writeToStream(const Var& var,
                   OutputStream& out,
                   const Type& type,
                   MaybeConstTypeHandle<> originType = std::nullopt);

/// Adapts the input variant to the target type. Optionally, the origin metatype from which the variant was created can
/// be specified.
Result<void, std::string> adaptVariant(const Type& type,
                                       Var& variant,
                                       MaybeConstTypeHandle<> originType = std::nullopt,
                                       bool useStrings = false);

[[nodiscard]] uint32_t getSerializedSize(const Var& var, const Type* type);

//--------------------------------------------------------------------------------------------------------------
// General purpose functions
//--------------------------------------------------------------------------------------------------------------

template <typename T>
[[nodiscard]] std::ostream& toOstream(std::ostream& out, const T& val);

[[nodiscard]] std::vector<std::string> split(const std::string& text, char sep = '.');

}  // namespace impl

/// \addtogroup io
/// @{

template <typename T>
[[nodiscard]] T toValue(const Var& var)
{
  T val = {};
  VariantTraits<T>::variantToValue(var, val);
  return val;
}

template <typename T>
[[nodiscard]] Var toVariant(const T& val)
{
  Var var;
  VariantTraits<T>::valueToVariant(val, var);
  return var;
}

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

namespace impl
{

//--------------------------------------------------------------------------------------------------------------
// Structs
//--------------------------------------------------------------------------------------------------------------

template <typename T>
inline void structFieldToVarMap(const char* name, const T& value, VarMap& map)
{
  Var fieldVar;
  ::sen::VariantTraits<T>::valueToVariant(value, fieldVar);
  map.try_emplace(name, std::move(fieldVar));
}

template <typename T>
inline void extractStructFieldFromMap(const char* name, T& value, const VarMap* map)
{
  const auto itr = map->find(name);
  if (itr != map->end())
  {
    ::sen::VariantTraits<T>::variantToValue(itr->second, value);
  }
}

//--------------------------------------------------------------------------------------------------------------
// Sequences
//--------------------------------------------------------------------------------------------------------------

template <typename T>
inline void writeSequencePayload(::sen::OutputStream& out, const T& val, std::size_t size)
{
  using ValueType = typename T::value_type;

  if constexpr (::sen::impl::allowsContiguousIO<ValueType>())
  {
    const auto bytesToWrite = sizeof(ValueType) * size;
    auto buffer = out.getWriter().advance(bytesToWrite);
    std::memcpy(buffer, val.data(), bytesToWrite);
  }
  else
  {
    for (const auto& elem: val)
    {
      ::sen::SerializationTraits<ValueType>::write(out, elem);
    }
  }
}

template <typename T>
inline void writeSequence(::sen::OutputStream& out, const T& val)
{
  const auto size = val.size();
  out.writeUInt32(static_cast<uint32_t>(size));

  if (size == 0U)
  {
    return;
  }

  writeSequencePayload(out, val, size);
}

template <typename T>
inline void writeArray(::sen::OutputStream& out, const T& val)
{
  if (val.size() == 0U)
  {
    return;
  }

  writeSequencePayload(out, val, val.size());
}

template <typename T>
inline void readSequencePayload(::sen::InputStream& in, T& val, std::size_t size)
{
  val.clear();

  // allocate space for the data
  val.resize(size);

  using ValueType = typename T::value_type;

  // read the data
  if constexpr (::sen::impl::allowsContiguousIO<ValueType>())
  {
    const auto bytes = size * sizeof(ValueType);
    std::memcpy(val.data(), in.advance(bytes), bytes);
  }
  else if constexpr (std::is_same_v<bool, ValueType>)
  {
    for (std::size_t i = 0; i < val.size(); ++i)
    {
      uint8_t elem = 0;
      in.readUInt8(elem);
      val[i] = (elem != 0);
    }
  }
  else
  {
    for (auto& elem: val)
    {
      ::sen::SerializationTraits<ValueType>::read(in, elem);
    }
  }
}

template <typename T>
inline void readSequence(::sen::InputStream& in, T& val)
{
  uint32_t size = 0U;
  in.readUInt32(size);

  if (size == 0U)
  {
    val.clear();
    return;
  }

  readSequencePayload(in, val, size);
}

template <typename T>
inline void readArray(::sen::InputStream& in, T& val)
{
  if (val.size() == 0U)
  {
    return;
  }

  using ValueType = typename T::value_type;

  // read the data
  if constexpr (::sen::impl::allowsContiguousIO<ValueType>())
  {
    const auto bytes = val.size() * sizeof(ValueType);
    std::memcpy(val.data(), in.advance(bytes), bytes);
  }
  else
  {
    for (auto& elem: val)
    {
      ::sen::SerializationTraits<ValueType>::read(in, elem);
    }
  }
}

template <typename T>
inline uint32_t sequenceSerializedSize(const T& val) noexcept
{
  using ValueType = typename T::value_type;

  const auto valSize = static_cast<uint32_t>(val.size());
  uint32_t total = ::sen::impl::getSerializedSize<uint32_t>(valSize);

  if constexpr (::sen::impl::allowsContiguousIO<ValueType>())
  {
    total += static_cast<uint32_t>(sizeof(ValueType)) * valSize;
  }
  else
  {
    for (const auto& elem: val)
    {
      total += ::sen::SerializationTraits<ValueType>::serializedSize(elem);
    }
  }
  return total;
}

template <typename T>
inline uint32_t arraySerializedSize(const T& val) noexcept
{
  using ValueType = typename T::value_type;
  if constexpr (::sen::impl::allowsContiguousIO<ValueType>())
  {
    return static_cast<uint32_t>(sizeof(ValueType)) * static_cast<uint32_t>(val.size());
  }
  else
  {
    uint32_t total = 0U;
    for (const auto& elem: val)
    {
      total += ::sen::SerializationTraits<ValueType>::serializedSize(elem);
    }
    return total;
  }
}

template <typename T>
inline void sequenceToVariant(const T& val, ::sen::Var& var)
{
  var = ::sen::VarList {};
  auto& list = var.get<::sen::VarList>();

  list.reserve(val.size());
  for (const auto& itemVal: val)
  {
    ::sen::Var itemVar;
    ::sen::VariantTraits<typename T::value_type>::valueToVariant(itemVal, itemVar);
    list.push_back(std::move(itemVar));
  }
}

template <typename T>
inline void arrayToVariant(const T& val, ::sen::Var& var)
{
  var = ::sen::VarList {};
  auto& list = var.get<::sen::VarList>();

  list.reserve(val.size());
  for (const auto& itemVal: val)
  {
    ::sen::Var itemVar;
    ::sen::VariantTraits<typename T::value_type>::valueToVariant(itemVal, itemVar);
    list.push_back(std::move(itemVar));
  }
}

template <typename T>
inline void variantToSequence(const ::sen::Var& var, T& val)
{
  using ValueType = typename T::value_type;

  if (auto* list = var.getIf<::sen::VarList>())
  {
    val.clear();
    if constexpr (std::is_same_v<T, std::vector<ValueType>>)
    {
      val.reserve(list->size());
    }

    for (const auto& itemVar: *list)
    {
      ValueType itemVal {};
      VariantTraits<ValueType>::variantToValue(itemVar, itemVal);
      val.push_back(std::move(itemVal));
    }
  }
}

template <typename T>
inline void variantToArray(const ::sen::Var& var, T& val)
{
  using ValueType = typename T::value_type;

  if (auto* list = var.getIf<::sen::VarList>())
  {
    std::size_t index = 0;
    for (const auto& itemVar: *list)
    {
      if (index >= val.size())
      {
        break;
      }

      VariantTraits<ValueType>::variantToValue(itemVar, val[index]);  // NOLINT
      ++index;
    }
  }
  else
  {
    ::sen::throwRuntimeError("var is not a sequence");
  }
}

//--------------------------------------------------------------------------------------------------------------
// Quantity
//--------------------------------------------------------------------------------------------------------------

template <typename T>
inline void writeQuantity(::sen::OutputStream& out, const T& val)
{
  ::sen::SerializationTraits<typename T::ValueType>::write(out, val.get());
}

template <typename T>
inline void readQuantity(::sen::InputStream& in, T& val)
{
  typename T::ValueType tmp {};
  ::sen::SerializationTraits<typename T::ValueType>::read(in, tmp);
  val.set(tmp);
}

template <typename T>
inline void quantityToVariant(const T& val, Var& var)
{
  ::sen::VariantTraits<typename T::ValueType>::valueToVariant(val.get(), var);
}

template <typename T>
inline void variantToQuantity(const Var& var, T& val)
{
  // try to convert from string, maybe with a custom unit
  if (var.holds<std::string>())
  {
    auto result = ::sen::QuantityTraits<T>::unit(::sen::Unit::ensurePresent).fromString(var.get<std::string>());
    if (result.isOk())
    {
      val.set(static_cast<typename T::ValueType>(result.getValue()));
      return;
    }
    throwRuntimeError(result.getError());
  }

  typename T::ValueType tmp {};
  ::sen::VariantTraits<typename T::ValueType>::variantToValue(var, tmp);
  val.set(tmp);
}

//--------------------------------------------------------------------------------------------------------------
// Enum
//--------------------------------------------------------------------------------------------------------------

template <typename T>
inline void readEnum(::sen::InputStream& in, T& val)
{
  using Num = std::underlying_type_t<T>;
  Num number;
  ::sen::SerializationTraits<Num>::read(in, number);
  val = static_cast<T>(number);
}

template <typename T>
inline void writeEnum(::sen::OutputStream& out, T val)
{
  using Num = std::underlying_type_t<T>;
  ::sen::SerializationTraits<Num>::write(out, static_cast<Num>(val));
}

template <typename T>
inline uint32_t enumSerializedSize(T val)
{
  using Num = std::underlying_type_t<T>;
  return ::sen::SerializationTraits<Num>::serializedSize(static_cast<Num>(val));
}

template <typename T>
inline void enumVariantToValue(const Var& var, T& val)
{
  if (var.holds<std::string>())
  {
    val = ::sen::StringConversionTraits<T>::fromString(var.get<std::string>());
  }
  else
  {
    val = static_cast<T>(getCopyAs<std::underlying_type_t<T>>(var));
  }
}

//--------------------------------------------------------------------------------------------------------------
// Variant
//--------------------------------------------------------------------------------------------------------------

template <typename F, typename V>
inline void readVariantField(::sen::InputStream& in, V& val)
{
  F fieldValue {};
  ::sen::SerializationTraits<F>::read(in, fieldValue);
  val = std::move(fieldValue);
}

template <typename F, typename V>
inline uint32_t getVariantFieldSerializedSize(uint32_t key, const V& val)
{
  const auto keySize = ::sen::impl::getSerializedSize<uint32_t>(key);
  const auto valSize = ::sen::SerializationTraits<F>::serializedSize(std::get<F>(val));
  const auto sizeSize = ::sen::impl::getSerializedSize<uint32_t>(valSize);
  return keySize + sizeSize + valSize;
}

//--------------------------------------------------------------------------------------------------------------
// General purpose functions
//--------------------------------------------------------------------------------------------------------------

template <typename T>
inline std::ostream& toOstream(std::ostream& out, const T& val)
{
  ::sen::Var variant;
  ::sen::VariantTraits<T>::valueToVariant(val, variant);
  return out << getCopyAs<std::string>(variant);
}

inline std::vector<std::string> split(const std::string& text, char sep)
{
  std::vector<std::string> tokens;
  std::size_t start = 0;
  std::size_t end = 0;
  while ((end = text.find(sep, start)) != std::string::npos)
  {
    tokens.push_back(text.substr(start, end - start));
    start = end + 1;
  }
  tokens.push_back(text.substr(start));
  return tokens;
}

}  // namespace impl
}  // namespace sen

#endif  // SEN_CORE_IO_UTIL_H
