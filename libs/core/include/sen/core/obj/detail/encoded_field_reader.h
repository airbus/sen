// === encoded_field_reader.h ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_DETAIL_ENCODED_FIELD_READER_H
#define SEN_CORE_OBJ_DETAIL_ENCODED_FIELD_READER_H

// sen
#include "sen/core/base/span.h"
#include "sen/core/lang/vm.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/type.h"

// std
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sen::impl
{

/// Reads one field of a property straight from its Sen encoding, so query filters work on objects
/// without generated code. It returns what a generated getter returns, and can also read optionals.
class EncodedFieldReader final
{
public:
  /// A reader for the field at `fields`, one index per struct field or variant alternative.
  /// Throws for a path a generated getter cannot read either. `propertyType` must outlive it.
  [[nodiscard]] static EncodedFieldReader resolve(const Type& propertyType, Span<const uint16_t> fields);

  /// The field's value in `value`, the property's encoding, or lang::VariantAccessError if it is
  /// absent or cut short. Throws for an enum key that names no enumerator.
  [[nodiscard]] lang::Value read(Span<const uint8_t> value) const;

private:
  enum class Op : uint8_t
  {
    skipFixed,  ///< skip `n` bytes
    skipValue,  ///< skip one value of `type`, whose size varies
    variant,    ///< a u32 key that must be `n`, else no value
    optional,   ///< a presence byte that must be set, else no value
  };

  struct Step
  {
    Op op = Op::skipFixed;
    uint32_t n = 0U;
    const Type* type = nullptr;
  };

  enum class Leaf : uint8_t
  {
    boolean,
    u8,
    i16,
    u16,
    i32,
    u32,
    i64,
    u64,
    f32,
    f64,
    string,
    enumName,         ///< the storage integer, given as the enumerator's name
    durationSeconds,  ///< i64 nanoseconds, given as f64 seconds
  };

  /// How a type is encoded and read; defined in the source file.
  struct Encoding;

  /// The bytes a scalar leaf reads; 0 for a string and for enumName.
  [[nodiscard]] static std::size_t sizeOf(Leaf leaf) noexcept;

  /// Appends the steps that skip a value of `type`.
  void appendSkip(const Type& type);

  /// Sets how the field is read; `nested` if the path goes below the property.
  void resolveLeaf(const Encoding& encoding, bool nested);

private:
  std::vector<Step> steps_;
  Leaf leaf_ = Leaf::boolean;
  Leaf storage_ = Leaf::boolean;  ///< for enumName, the integer read
  const EnumType* enumType_ = nullptr;
};

}  // namespace sen::impl

#endif  // SEN_CORE_OBJ_DETAIL_ENCODED_FIELD_READER_H
