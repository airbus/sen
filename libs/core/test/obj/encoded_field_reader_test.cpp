// === encoded_field_reader_test.cpp ===================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

/// Tests for EncodedFieldReader, on types built at runtime and encodings written by writeToStream.

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/span.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/io/util.h"
#include "sen/core/lang/vm.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"
#include "sen/core/obj/detail/encoded_field_reader.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace
{

using sen::impl::EncodedFieldReader;
using sen::lang::Value;
using sen::lang::VariantAccessError;

/// Types built at runtime, covering each kind of value a path can pass through or end at.
struct Types
{
  sen::ConstTypeHandle<sen::EnumType> force = sen::EnumType::make(
    {"Force", "ns.Force", "", {{"neutral", 0U, ""}, {"friendly", 1U, ""}, {"hostile", 2U, ""}}, sen::UInt8Type::get()});

  sen::ConstTypeHandle<sen::StructType> header = sen::StructType::make(
    {"Header", "ns.Header", "", {{"tag", "", sen::UInt16Type::get()}, {"label", "", sen::StringType::get()}}});

  sen::ConstTypeHandle<sen::SequenceType> names =
    sen::SequenceType::make({"Names", "ns.Names", "", sen::StringType::get()});

  sen::ConstTypeHandle<sen::QuantityType> height =
    sen::QuantityType::make({"Height", "ns.Height", "", sen::Float64Type::get()});

  sen::ConstTypeHandle<sen::AliasType> count = sen::AliasType::make({"Count", "ns.Count", "", sen::Int32Type::get()});

  sen::ConstTypeHandle<sen::OptionalType> maybe =
    sen::OptionalType::make({"MaybeFlag", "ns.MaybeFlag", "", sen::BoolType::get()});

  sen::ConstTypeHandle<sen::StructType> record = sen::StructType::make({"Record",
                                                                        "ns.Record",
                                                                        "",
                                                                        {{"names", "", names},
                                                                         {"maybe", "", maybe},
                                                                         {"force", "", force},
                                                                         {"elapsed", "", sen::DurationType::get()},
                                                                         {"height", "", height},
                                                                         {"count", "", count},
                                                                         {"ok", "", sen::BoolType::get()}},
                                                                        header});

  sen::ConstTypeHandle<sen::StructType> child =
    sen::StructType::make({"Child", "ns.Child", "", {{"value", "", sen::UInt32Type::get()}}, header});

  sen::ConstTypeHandle<sen::StructType> fixed = sen::StructType::make(
    {"FixedPos", "ns.FixedPos", "", {{"lat", "", sen::Float64Type::get()}, {"lon", "", sen::Float64Type::get()}}});

  sen::ConstTypeHandle<sen::VariantType> payload =
    sen::VariantType::make({"Payload", "ns.Payload", "", {{0U, "", record}, {1U, "", fixed}}});
};

const Types& types()
{
  static const Types instance;
  return instance;
}

/// A record's value: its header's fields, then its own.
sen::Var recordValue(sen::Var maybe, int32_t count)
{
  return sen::VarMap {{"tag", uint16_t {9U}},
                      {"label", std::string("a label longer than any small-string buffer")},
                      {"names", sen::VarList {std::string("ab"), std::string("")}},
                      {"maybe", std::move(maybe)},
                      {"force", uint8_t {2U}},
                      {"elapsed", sen::Duration {std::chrono::milliseconds {1500}}},
                      {"height", 120.25},
                      {"count", count},
                      {"ok", true}};
}

/// Sen's encoding of `value` as `type`.
std::vector<uint8_t> encode(const sen::Var& value, const sen::Type& type)
{
  std::vector<uint8_t> bytes;
  sen::ResizableBufferWriter writer(bytes);
  sen::OutputStream out(writer);
  sen::impl::writeToStream(value, out, type);
  return bytes;
}

/// The value at `fields` in `bytes`, an encoding of `type`.
Value readAt(const sen::Type& type, const std::vector<uint16_t>& fields, const std::vector<uint8_t>& bytes)
{
  return EncodedFieldReader::resolve(type, sen::makeConstSpan(fields)).read(bytes);
}

/// Encodes `value` as a whole property of `type` and reads it back.
template <typename T>
T readWhole(const sen::Type& type, T value)
{
  return std::get<T>(readAt(type, {}, encode(sen::Var(std::move(value)), type)));
}

bool isNoValue(const Value& value) { return std::holds_alternative<VariantAccessError>(value); }

}  // namespace

/// @test
/// Reads a bool, the integers, the reals and a string, each as a whole property.
TEST(EncodedFieldReader, readsAPropertyOfEachScalarKind)
{
  EXPECT_EQ(readWhole(*sen::BoolType::get(), true), true);
  EXPECT_EQ(readWhole(*sen::UInt8Type::get(), uint8_t {200U}), uint8_t {200U});
  EXPECT_EQ(readWhole(*sen::Int16Type::get(), int16_t {-300}), int16_t {-300});
  EXPECT_EQ(readWhole(*sen::UInt16Type::get(), uint16_t {60000U}), uint16_t {60000U});
  EXPECT_EQ(readWhole(*sen::Int32Type::get(), int32_t {-70000}), int32_t {-70000});
  EXPECT_EQ(readWhole(*sen::UInt32Type::get(), uint32_t {4000000000U}), uint32_t {4000000000U});
  EXPECT_EQ(readWhole(*sen::Int64Type::get(), int64_t {-5000000000}), int64_t {-5000000000});
  EXPECT_EQ(readWhole(*sen::UInt64Type::get(), uint64_t {10000000000000000000U}), uint64_t {10000000000000000000U});
  EXPECT_EQ(readWhole(*sen::Float32Type::get(), 1.5F), 1.5F);
  EXPECT_EQ(readWhole(*sen::Float64Type::get(), -2.25), -2.25);
  EXPECT_EQ(readWhole(*sen::StringType::get(), std::string("hello")), "hello");
}

/// @test
/// Reads a field that follows fixed-size fields, and one that follows a string and a sequence.
TEST(EncodedFieldReader, readsAFieldAfterFieldsOfFixedAndOfVaryingSize)
{
  const auto& t = types();

  const auto fixed = encode(sen::VarMap {{"lat", 12.5}, {"lon", 3.25}}, *t.fixed);
  EXPECT_EQ(std::get<float64_t>(readAt(*t.fixed, {1U}, fixed)), 3.25);

  const auto record = encode(recordValue(false, -7), *t.record);
  EXPECT_EQ(std::get<int32_t>(readAt(*t.record, {5U}, record)), -7);
  EXPECT_EQ(std::get<bool>(readAt(*t.record, {6U}, record)), true);
}

/// @test
/// Indexes a struct's own fields, not its parent's, and reads them after the parent's fields.
TEST(EncodedFieldReader, readsAStructsOwnFieldsAfterItsParents)
{
  const auto& t = types();
  const auto child =
    encode(sen::VarMap {{"tag", uint16_t {1U}}, {"label", std::string("parent")}, {"value", uint32_t {77U}}}, *t.child);

  EXPECT_EQ(std::get<uint32_t>(readAt(*t.child, {0U}, child)), 77U);
  EXPECT_THROW(std::ignore = EncodedFieldReader::resolve(*t.child, sen::makeConstSpan(std::vector<uint16_t> {1U})),
               std::exception);
}

/// @test
/// Reads a field of the variant's held alternative; a field of another alternative gives "no value".
TEST(EncodedFieldReader, readsTheHeldAlternativeAndNoValueForAnother)
{
  const auto& t = types();
  const auto holdingRecord = encode(sen::KeyedVar {0U, std::make_shared<sen::Var>(recordValue(true, 42))}, *t.payload);
  const auto holdingFixed =
    encode(sen::KeyedVar {1U, std::make_shared<sen::Var>(sen::VarMap {{"lat", 1.0}, {"lon", 2.0}})}, *t.payload);

  EXPECT_EQ(std::get<int32_t>(readAt(*t.payload, {0U, 5U}, holdingRecord)), 42);
  EXPECT_TRUE(isNoValue(readAt(*t.payload, {1U, 0U}, holdingRecord)));

  EXPECT_EQ(std::get<float64_t>(readAt(*t.payload, {1U, 1U}, holdingFixed)), 2.0);
  EXPECT_TRUE(isNoValue(readAt(*t.payload, {0U, 5U}, holdingFixed)));
}

/// @test
/// Reads an optional's value, or "no value" when it is empty.
TEST(EncodedFieldReader, givesAnOptionalsValueOrNoValue)
{
  const auto& t = types();

  EXPECT_EQ(std::get<bool>(readAt(*t.record, {1U}, encode(recordValue(false, 0), *t.record))), false);
  EXPECT_EQ(std::get<bool>(readAt(*t.record, {1U}, encode(recordValue(true, 0), *t.record))), true);
  EXPECT_TRUE(isNoValue(readAt(*t.record, {1U}, encode(recordValue(sen::Var {}, 0), *t.record))));

  // the fields after an empty optional are still found
  EXPECT_EQ(std::get<int32_t>(readAt(*t.record, {5U}, encode(recordValue(sen::Var {}, 5), *t.record))), 5);

  // and an optional property
  EXPECT_EQ(std::get<bool>(readAt(*t.maybe, {}, encode(true, *t.maybe))), true);
  EXPECT_TRUE(isNoValue(readAt(*t.maybe, {}, encode(sen::Var {}, *t.maybe))));
}

/// @test
/// Reads an enum property as its enumerator's name, and an enum inside a struct as its integer.
TEST(EncodedFieldReader, givesAnEnumPropertyItsNameAndANestedEnumItsInteger)
{
  const auto& t = types();

  EXPECT_EQ(std::get<std::string>(readAt(*t.force, {}, encode(uint8_t {1U}, *t.force))), "friendly");
  EXPECT_EQ(std::get<uint8_t>(readAt(*t.record, {2U}, encode(recordValue(false, 0), *t.record))), uint8_t {2U});
}

/// @test
/// Reads a duration as seconds, a quantity as its number, and an alias as the type it holds.
TEST(EncodedFieldReader, givesADurationInSecondsAQuantityItsNumberAndAnAliasWhatItHolds)
{
  const auto& t = types();
  const auto record = encode(recordValue(false, 11), *t.record);

  EXPECT_EQ(std::get<float64_t>(readAt(*t.record, {3U}, record)),
            sen::Duration {std::chrono::milliseconds {1500}}.toSeconds());
  EXPECT_EQ(std::get<float64_t>(readAt(*t.record, {4U}, record)), 120.25);
  EXPECT_EQ(std::get<int32_t>(readAt(*t.record, {5U}, record)), 11);

  const sen::Duration negative {std::chrono::milliseconds {-20}};
  EXPECT_EQ(std::get<float64_t>(readAt(*sen::DurationType::get(), {}, encode(negative, *sen::DurationType::get()))),
            negative.toSeconds());
}

/// @test
/// Returns "no value" for an encoding cut off anywhere before the value, never reading past it.
TEST(EncodedFieldReader, givesNoValueForAnEncodingThatEndsEarly)
{
  const auto& t = types();
  const auto whole = encode(sen::KeyedVar {0U, std::make_shared<sen::Var>(recordValue(true, 3))}, *t.payload);
  const auto reader = EncodedFieldReader::resolve(*t.payload, sen::makeConstSpan(std::vector<uint16_t> {0U, 6U}));

  ASSERT_EQ(std::get<bool>(reader.read(whole)), true);
  for (std::size_t size = 0U; size < whole.size(); ++size)
  {
    // a copy of exactly that size, so that a read past it is an error the sanitizers see
    const std::vector<uint8_t> truncated(whole.begin(), whole.begin() + static_cast<std::ptrdiff_t>(size));
    EXPECT_TRUE(isNoValue(reader.read(truncated))) << "truncated to " << size << " bytes";
  }
}

/// @test
/// A sequence count too large for the encoding gives "no value"; zero-byte elements take no bytes.
TEST(EncodedFieldReader, skipsASequenceOfElementsOfOneSizeInOneStep)
{
  const auto holding = [](const sen::ConstTypeHandle<>& items)
  {
    return sen::StructType::make(
      {"Holder", "ns.Holder", "", {{"items", "", items}, {"after", "", sen::UInt32Type::get()}}});
  };
  const auto bytes = holding(sen::SequenceType::make({"Bytes", "ns.Bytes", "", sen::UInt8Type::get()}));
  const auto empty = sen::StructType::make({"Nothing", "ns.Nothing", "", {}});
  const auto nothings = holding(sen::SequenceType::make({"Nothings", "ns.Nothings", "", empty}));

  // three elements, then the field after them
  const std::vector<uint8_t> three {3U, 0U, 0U, 0U, 1U, 2U, 3U, 7U, 0U, 0U, 0U};
  EXPECT_EQ(std::get<uint32_t>(readAt(*bytes, {1U}, three)), 7U);

  // a count of 2^32 - 1 with four bytes left
  const std::vector<uint8_t> corrupt {0xFFU, 0xFFU, 0xFFU, 0xFFU, 7U, 0U, 0U, 0U};
  EXPECT_TRUE(isNoValue(readAt(*bytes, {1U}, corrupt)));
  EXPECT_EQ(std::get<uint32_t>(readAt(*nothings, {1U}, corrupt)), 7U);
}

/// @test
/// resolve() throws for a missing field, or a path through or ending at a type it cannot read.
TEST(EncodedFieldReader, refusesAPathItCannotRead)
{
  const auto& t = types();
  auto resolve = [](const sen::Type& type, std::vector<uint16_t> fields)
  { return EncodedFieldReader::resolve(type, sen::makeConstSpan(fields)); };

  EXPECT_THROW(std::ignore = resolve(*t.record, {}), std::exception);
  EXPECT_THROW(std::ignore = resolve(*t.payload, {}), std::exception);
  EXPECT_THROW(std::ignore = resolve(*t.payload, {1U}), std::exception);
  EXPECT_THROW(std::ignore = resolve(*t.record, {0U}), std::exception);
  EXPECT_THROW(std::ignore = resolve(*sen::TimestampType::get(), {}), std::exception);
  EXPECT_THROW(std::ignore = resolve(*t.record, {5U, 0U}), std::exception);
  EXPECT_THROW(std::ignore = resolve(*t.record, {7U}), std::exception);
  EXPECT_THROW(std::ignore = resolve(*t.payload, {2U}), std::exception);
}

/// @test
/// Throws when an enum property's key names no enumerator, as a generated getter does.
TEST(EncodedFieldReader, throwsForAnEnumKeyWithNoEnumerator)
{
  const auto& t = types();
  const auto reader = EncodedFieldReader::resolve(*t.force, {});
  EXPECT_THROW(std::ignore = reader.read(std::vector<uint8_t> {7U}), std::exception);
}
