// === encoded_field_reader.cpp ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/obj/detail/encoded_field_reader.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/span.h"
#include "sen/core/io/detail/endianness.h"
#include "sen/core/lang/vm.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/variant_type.h"

// std
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>

namespace sen::impl
{

namespace
{

/// Reads little-endian values from an encoding, never past its end.
class Cursor
{
public:
  explicit Cursor(Span<const uint8_t> bytes) noexcept: bytes_(bytes) {}

public:
  /// Whether at least `count` bytes are left.
  [[nodiscard]] bool has(std::size_t count) const noexcept { return remaining() >= count; }

  /// How many bytes are left.
  [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - at_; }

  /// Moves past `count` bytes; false, without moving, if fewer are left.
  [[nodiscard]] bool skip(std::size_t count) noexcept
  {
    if (!has(count))
    {
      return false;
    }
    at_ += count;
    return true;
  }

  /// Reads the next T. Call only after has(sizeof(T)).
  template <typename T>
  [[nodiscard]] T take() noexcept
  {
    T value {};
    std::memcpy(&value, here(), sizeof(T));
    at_ += sizeof(T);
    swapBytesIfNeeded(value, LittleEndian {});
    return value;
  }

  /// Reads the next `count` bytes as a string. Call only after has(count).
  [[nodiscard]] std::string takeString(std::size_t count)
  {
    std::string value(count, '\0');
    std::memcpy(value.data(), here(), count);
    at_ += count;
    return value;
  }

private:
  [[nodiscard]] const uint8_t* here() const noexcept
  {
    return bytes_.data() + at_;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }

private:
  Span<const uint8_t> bytes_;
  std::size_t at_ = 0U;
};

}  // namespace

/// How a type is encoded, with aliases and quantities seen through, and how a query reads it.
struct EncodedFieldReader::Encoding
{
  enum class Kind : uint8_t
  {
    scalar,  ///< a bool, a number, a duration or a timestamp
    string,
    enumeration,
    structure,
    sequence,
    variant,
    optional,
    none,  ///< never part of a value
  };

  Kind kind = Kind::none;
  const Type* type = nullptr;            ///< aliases and quantities seen through
  std::optional<std::size_t> fixedSize;  ///< set when every value has the same size
  std::optional<Leaf> leaf;              ///< set when a query can read it; an enum's is its storage's

  [[nodiscard]] static Encoding of(const Type& type);

  /// Moves past one value of `type`; false if the encoding ends first.
  [[nodiscard]] static bool skip(const Type& type, Cursor& cursor);

private:
  class Visitor;

  [[nodiscard]] static bool skipStruct(const StructType& type, Cursor& cursor);
  [[nodiscard]] static bool skipSequence(const SequenceType& type, Cursor& cursor);
};

/// Computes an Encoding. It handles every kind of Sen type, so a new kind cannot be missed.
class EncodedFieldReader::Encoding::Visitor final: public FullTypeVisitor
{
public:
  SEN_NOCOPY_NOMOVE(Visitor)

public:
  Visitor() = default;
  ~Visitor() override = default;

  [[nodiscard]] const Encoding& result() const noexcept { return result_; }

public:  // abstract types, and types that are never part of a value
  void apply(const Type& type) override { set(type, Kind::none); }
  void apply(const NativeType& type) override { set(type, Kind::none); }
  void apply(const NumericType& type) override { set(type, Kind::none); }
  void apply(const IntegralType& type) override { set(type, Kind::none); }
  void apply(const RealType& type) override { set(type, Kind::none); }
  void apply(const CustomType& type) override { set(type, Kind::none); }
  void apply(const VoidType& type) override { set(type, Kind::none); }
  void apply(const ClassType& type) override { set(type, Kind::none); }

public:  // encoded as the type they hold
  void apply(const AliasType& type) override { type.getAliasedType().type()->accept(*this); }
  void apply(const QuantityType& type) override { type.getElementType().type()->accept(*this); }

public:  // scalars
  void apply(const BoolType& type) override { scalar(type, 1U, Leaf::boolean); }
  void apply(const UInt8Type& type) override { scalar(type, 1U, Leaf::u8); }
  void apply(const Int16Type& type) override { scalar(type, 2U, Leaf::i16); }
  void apply(const UInt16Type& type) override { scalar(type, 2U, Leaf::u16); }
  void apply(const Int32Type& type) override { scalar(type, 4U, Leaf::i32); }
  void apply(const UInt32Type& type) override { scalar(type, 4U, Leaf::u32); }
  void apply(const Int64Type& type) override { scalar(type, 8U, Leaf::i64); }
  void apply(const UInt64Type& type) override { scalar(type, 8U, Leaf::u64); }
  void apply(const Float32Type& type) override { scalar(type, 4U, Leaf::f32); }
  void apply(const Float64Type& type) override { scalar(type, 8U, Leaf::f64); }
  void apply(const DurationType& type) override { scalar(type, 8U, Leaf::durationSeconds); }
  void apply(const TimestampType& type) override { scalar(type, 8U, std::nullopt); }  // queries cannot read one

public:  // the rest
  void apply(const StringType& type) override
  {
    set(type, Kind::string);
    result_.leaf = Leaf::string;
  }

  void apply(const EnumType& type) override
  {
    const auto storage = of(type.getStorageType());
    set(type, Kind::enumeration, storage.fixedSize);
    result_.leaf = storage.leaf;
  }

  void apply(const StructType& type) override { set(type, Kind::structure, structSize(type)); }
  void apply(const SequenceType& type) override { set(type, Kind::sequence, arraySize(type)); }
  void apply(const VariantType& type) override { set(type, Kind::variant); }
  void apply(const OptionalType& type) override { set(type, Kind::optional); }

private:
  void set(const Type& type, Kind kind, std::optional<std::size_t> fixedSize = std::nullopt)
  {
    result_ = {kind, &type, fixedSize, std::nullopt};
  }

  void scalar(const Type& type, std::size_t size, std::optional<Leaf> leaf)
  {
    result_ = {Kind::scalar, &type, size, leaf};
  }

  /// A struct's size, parents' fields first, when all its fields have fixed sizes.
  [[nodiscard]] static std::optional<std::size_t> structSize(const StructType& type)
  {
    std::size_t total = 0U;
    if (const auto parent = type.getParent())
    {
      const auto size = structSize(*parent.value());
      if (!size)
      {
        return std::nullopt;
      }
      total = *size;
    }

    for (const auto& field: type.getFields())
    {
      const auto size = of(*field.type).fixedSize;
      if (!size)
      {
        return std::nullopt;
      }
      total += *size;
    }
    return total;
  }

  /// An array's size when its elements have fixed sizes; other sequences vary.
  [[nodiscard]] static std::optional<std::size_t> arraySize(const SequenceType& type)
  {
    if (!type.hasFixedSize())
    {
      return std::nullopt;
    }

    const auto count = type.getMaxSize().value();
    if (count == 0U)
    {
      return 0U;
    }

    const auto element = of(*type.getElementType()).fixedSize;
    if (!element)
    {
      return std::nullopt;
    }
    return *element * count;
  }

private:
  Encoding result_;
};

EncodedFieldReader::Encoding EncodedFieldReader::Encoding::of(const Type& type)
{
  Visitor visitor;
  type.accept(visitor);
  return visitor.result();
}

bool EncodedFieldReader::Encoding::skip(const Type& type, Cursor& cursor)
{
  const auto encoding = of(type);
  if (encoding.fixedSize)
  {
    return cursor.skip(*encoding.fixedSize);
  }

  switch (encoding.kind)
  {
    case Kind::string:
      return cursor.has(4U) && cursor.skip(cursor.take<uint32_t>());

    case Kind::structure:
      return skipStruct(*encoding.type->asStructType(), cursor);

    case Kind::sequence:
      return skipSequence(*encoding.type->asSequenceType(), cursor);

    case Kind::variant:
    {
      if (!cursor.has(4U))
      {
        return false;
      }
      const auto* field = encoding.type->asVariantType()->getFieldFromKey(cursor.take<uint32_t>());
      return field != nullptr && skip(*field->type, cursor);
    }

    case Kind::optional:
      return cursor.has(1U) &&
             (cursor.take<uint8_t>() == 0U || skip(*encoding.type->asOptionalType()->getType(), cursor));

    case Kind::scalar:
    case Kind::enumeration:
    case Kind::none:
      break;  // scalars and enums have fixed sizes, and the rest is never encoded
  }
  return false;
}

bool EncodedFieldReader::Encoding::skipStruct(const StructType& type, Cursor& cursor)
{
  if (const auto parent = type.getParent())
  {
    if (!skipStruct(*parent.value(), cursor))
    {
      return false;
    }
  }

  for (const auto& field: type.getFields())
  {
    if (!skip(*field.type, cursor))
    {
      return false;
    }
  }
  return true;
}

bool EncodedFieldReader::Encoding::skipSequence(const SequenceType& type, Cursor& cursor)
{
  // an array has no count in its encoding; any other sequence starts with a u32
  std::size_t count = 0U;
  if (type.hasFixedSize())
  {
    count = type.getMaxSize().value();
  }
  else
  {
    if (!cursor.has(4U))
    {
      return false;
    }
    count = cursor.take<uint32_t>();
  }

  const auto elementType = type.getElementType();
  const auto& element = *elementType;

  // fixed-size elements are checked and skipped at once, so a corrupt count fails without looping
  if (const auto elementSize = of(element).fixedSize)
  {
    if (*elementSize != 0U && count > cursor.remaining() / *elementSize)
    {
      return false;
    }
    return cursor.skip(count * *elementSize);
  }

  // any other element takes at least one byte, so a corrupt count runs out of bytes
  for (std::size_t k = 0U; k < count; ++k)
  {
    if (!skip(element, cursor))
    {
      return false;
    }
  }
  return true;
}

std::size_t EncodedFieldReader::sizeOf(Leaf leaf) noexcept
{
  switch (leaf)
  {
    case Leaf::boolean:
    case Leaf::u8:
      return 1U;
    case Leaf::i16:
    case Leaf::u16:
      return 2U;
    case Leaf::i32:
    case Leaf::u32:
    case Leaf::f32:
      return 4U;
    case Leaf::i64:
    case Leaf::u64:
    case Leaf::f64:
    case Leaf::durationSeconds:
      return 8U;
    case Leaf::string:
    case Leaf::enumName:
      return 0U;
  }
  return 0U;
}

void EncodedFieldReader::appendSkip(const Type& type)
{
  const auto size = Encoding::of(type).fixedSize;
  if (!size)
  {
    steps_.push_back({Op::skipValue, 0U, &type});
    return;
  }

  if (*size == 0U)
  {
    return;
  }

  // consecutive fixed sizes fold into one offset
  if (!steps_.empty() && steps_.back().op == Op::skipFixed)
  {
    steps_.back().n += static_cast<uint32_t>(*size);
  }
  else
  {
    steps_.push_back({Op::skipFixed, static_cast<uint32_t>(*size), nullptr});
  }
}

void EncodedFieldReader::resolveLeaf(const Encoding& encoding, bool nested)
{
  if (!encoding.leaf)
  {
    throwRuntimeError("a query cannot read " + std::string(encoding.type->getName()) + " from a property's encoding");
  }
  leaf_ = *encoding.leaf;

  // as in generated getters: an enum property reads as its enumerator's name, a nested one as its integer
  if (encoding.kind == Encoding::Kind::enumeration && !nested)
  {
    storage_ = leaf_;
    leaf_ = Leaf::enumName;
    enumType_ = encoding.type->asEnumType();
  }
}

EncodedFieldReader EncodedFieldReader::resolve(const Type& propertyType, Span<const uint16_t> fields)
{
  EncodedFieldReader reader;

  auto encoding = Encoding::of(propertyType);
  for (const auto index: fields)
  {
    if (encoding.kind == Encoding::Kind::structure)
    {
      const auto& structType = *encoding.type->asStructType();
      const auto own = structType.getFields();
      if (index >= own.size())
      {
        throwRuntimeError("no field " + std::to_string(index) + " in " + std::string(structType.getQualifiedName()));
      }

      // a parent's fields come first, and the index counts only the struct's own
      if (const auto parent = structType.getParent())
      {
        reader.appendSkip(*parent.value());
      }
      for (std::size_t k = 0U; k < index; ++k)
      {
        reader.appendSkip(*own[k].type);
      }
      encoding = Encoding::of(*own[index].type);
    }
    else if (encoding.kind == Encoding::Kind::variant)
    {
      const auto& variant = *encoding.type->asVariantType();
      const auto alternatives = variant.getFields();
      if (index >= alternatives.size())
      {
        throwRuntimeError("no alternative " + std::to_string(index) + " in " + std::string(variant.getQualifiedName()));
      }
      reader.steps_.push_back({Op::variant, alternatives[index].key, nullptr});
      encoding = Encoding::of(*alternatives[index].type);
    }
    else
    {
      throwRuntimeError("a query path goes through " + std::string(encoding.type->getName()) +
                        ", which is neither a struct nor a variant");
    }
  }

  // an optional gives its value, or no value when it is empty
  if (encoding.kind == Encoding::Kind::optional)
  {
    reader.steps_.push_back({Op::optional, 0U, nullptr});
    encoding = Encoding::of(*encoding.type->asOptionalType()->getType());
  }

  reader.resolveLeaf(encoding, !fields.empty());
  return reader;
}

lang::Value EncodedFieldReader::read(Span<const uint8_t> value) const
{
  Cursor cursor(value);
  for (const auto& step: steps_)
  {
    switch (step.op)
    {
      case Op::skipFixed:
        if (!cursor.skip(step.n))
        {
          return lang::VariantAccessError {};
        }
        break;
      case Op::skipValue:
        if (!Encoding::skip(*step.type, cursor))
        {
          return lang::VariantAccessError {};
        }
        break;
      case Op::variant:
        if (!cursor.has(4U) || cursor.take<uint32_t>() != step.n)
        {
          return lang::VariantAccessError {};
        }
        break;
      case Op::optional:
        if (!cursor.has(1U) || cursor.take<uint8_t>() == 0U)
        {
          return lang::VariantAccessError {};
        }
        break;
    }
  }

  if (leaf_ == Leaf::string)
  {
    if (!cursor.has(4U))
    {
      return lang::VariantAccessError {};
    }
    const auto length = cursor.take<uint32_t>();
    if (!cursor.has(length))
    {
      return lang::VariantAccessError {};
    }
    return cursor.takeString(length);
  }

  if (!cursor.has(sizeOf(leaf_ == Leaf::enumName ? storage_ : leaf_)))
  {
    return lang::VariantAccessError {};
  }

  if (leaf_ == Leaf::enumName)
  {
    uint32_t key = 0U;
    switch (storage_)
    {
      case Leaf::u8:
        key = cursor.take<uint8_t>();
        break;
      case Leaf::i16:
        key = static_cast<uint32_t>(cursor.take<int16_t>());
        break;
      case Leaf::u16:
        key = cursor.take<uint16_t>();
        break;
      case Leaf::i32:
        key = static_cast<uint32_t>(cursor.take<int32_t>());
        break;
      case Leaf::u32:
        key = cursor.take<uint32_t>();
        break;
      case Leaf::i64:
        key = static_cast<uint32_t>(cursor.take<int64_t>());
        break;
      case Leaf::u64:
        key = static_cast<uint32_t>(cursor.take<uint64_t>());
        break;
      default:
        return lang::VariantAccessError {};
    }

    const auto* enumerator = enumType_->getEnumFromKey(key);
    if (enumerator == nullptr)
    {
      // a generated getter throws here too, and the VM fails the query
      throwRuntimeError("no enumerator of " + std::string(enumType_->getQualifiedName()) + " has the key " +
                        std::to_string(key));
    }
    return enumerator->name;
  }

  switch (leaf_)
  {
    case Leaf::boolean:
      return cursor.take<uint8_t>() != 0U;
    case Leaf::u8:
      return cursor.take<uint8_t>();
    case Leaf::i16:
      return cursor.take<int16_t>();
    case Leaf::u16:
      return cursor.take<uint16_t>();
    case Leaf::i32:
      return cursor.take<int32_t>();
    case Leaf::u32:
      return cursor.take<uint32_t>();
    case Leaf::i64:
      return cursor.take<int64_t>();
    case Leaf::u64:
      return cursor.take<uint64_t>();
    case Leaf::f32:
      return cursor.take<float32_t>();
    case Leaf::f64:
      return cursor.take<float64_t>();
    case Leaf::durationSeconds:
      return Duration {cursor.take<int64_t>()}.toSeconds();
    case Leaf::string:
    case Leaf::enumName:
      break;
  }
  return lang::VariantAccessError {};
}

}  // namespace sen::impl
