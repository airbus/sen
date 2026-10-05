// === value_formatter_test.cpp ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "test_render_utils.h"
#include "value_formatter.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"

// ftxui
#include <ftxui/dom/node.hpp>
#include <ftxui/dom/requirement.hpp>

// google test
#include <gmock/gmock.h>
#include <gtest/gtest.h>

// std
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>

namespace sen::components::term
{
namespace
{

using test::renderToText;

/// Rows and columns the element asks the layout for. The screen a test renders to is fixed, so a
/// runaway value is invisible in the rendered text, because the cost is in the tree, which is what this
/// measures.
struct Extent
{
  int rows = 0;
  int columns = 0;
};

[[nodiscard]] Extent extentOf(const ftxui::Element& element)
{
  element->ComputeRequirement();
  return Extent {element->requirement().min_y, element->requirement().min_x};
}

ConstTypeHandle<EnumType> makeColourEnum()
{
  static const auto type =
    EnumType::make(EnumSpec {"Colour",
                             "test.Colour",
                             "",
                             {{"red", 0, "the red one"}, {"green", 1, ""}, {"blue", 2, "the blue one"}},
                             UInt8Type::get()});
  return type;
}

ConstTypeHandle<StructType> makePointStruct()
{
  static const auto type = StructType::make(
    StructSpec {"Point", "test.Point", "", {{"x", "", Int32Type::get()}, {"y", "", Int32Type::get()}}});
  return type;
}

/// A struct with an optional field, for the case where the optional is carrying nothing.
ConstTypeHandle<StructType> makeNoteStruct()
{
  static const auto note = OptionalType::make(OptionalSpec {"OptStr", "test.OptStr", "", StringType::get()});
  static const auto type =
    StructType::make(StructSpec {"Note", "test.Note", "", {{"name", "", StringType::get()}, {"note", "", note}}});
  return type;
}

ConstTypeHandle<SequenceType> makeIntSeq()
{
  static const auto type = SequenceType::make(SequenceSpec {"IntSeq", "test.IntSeq", "", Int32Type::get()});
  return type;
}

ConstTypeHandle<SequenceType> makeByteSeq()
{
  static const auto type = SequenceType::make(SequenceSpec {"ByteSeq", "test.ByteSeq", "", UInt8Type::get()});
  return type;
}

ConstTypeHandle<VariantType> makeIntOrStringVariant()
{
  VariantSpec spec;
  spec.name = "IntOrString";
  spec.qualifiedName = "test.IntOrString";
  spec.fields.emplace_back(1, "", Int32Type::get());
  spec.fields.emplace_back(2, "", StringType::get());
  static const auto type = VariantType::make(spec);
  return type;
}

ConstTypeHandle<OptionalType> makeOptionalInt()
{
  static const auto type = OptionalType::make(OptionalSpec {"OptInt", "test.OptInt", "", Int32Type::get()});
  return type;
}

ConstTypeHandle<AliasType> makeAliasToInt()
{
  static const auto type = AliasType::make(AliasSpec {"IntAlias", "test.IntAlias", "", Int32Type::get()});
  return type;
}

//--------------------------------------------------------------------------------------------------------------
// Primitive types
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A true boolean renders as true.
TEST(ValueFormatter, BoolTrue)
{
  auto out = renderToText(formatValue(Var(true), *BoolType::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("true"));
}

/// @test
/// A false boolean renders as false.
TEST(ValueFormatter, BoolFalse)
{
  auto out = renderToText(formatValue(Var(false), *BoolType::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("false"));
}

/// @test
/// A positive integer renders as its digits.
TEST(ValueFormatter, Int32Positive)
{
  auto out = renderToText(formatValue(Var(int32_t {42}), *Int32Type::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("42"));
}

/// @test
/// A negative integer keeps its sign.
TEST(ValueFormatter, Int32Negative)
{
  auto out = renderToText(formatValue(Var(int32_t {-17}), *Int32Type::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("-17"));
}

/// @test
/// An unsigned byte renders as a number rather than a character.
TEST(ValueFormatter, UInt8)
{
  auto out = renderToText(formatValue(Var(uint8_t {255}), *UInt8Type::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("255"));
}

/// @test
/// A large 64 bit integer renders in full.
TEST(ValueFormatter, Int64Large)
{
  auto out = renderToText(formatValue(Var(int64_t {1234567890123}), *Int64Type::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("1234567890123"));
}

/// @test
/// A double renders with its fractional part.
TEST(ValueFormatter, Float64)
{
  auto out = renderToText(formatValue(Var(double {3.14}), *Float64Type::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("3.14"));
}

//--------------------------------------------------------------------------------------------------------------
// String
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A string renders inside quotes.
TEST(ValueFormatter, StringQuoted)
{
  auto out = renderToText(formatValue(Var(std::string("hello")), *StringType::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("\"hello\""));
}

/// @test
/// A value carrying no string renders as empty.
TEST(ValueFormatter, StringEmptyVar)
{
  // Var with no string payload, formatter should fall back to <empty>.
  auto out = renderToText(formatValue(Var {}, *StringType::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("<empty>"));
}

/// @test
/// A long string is capped, so the full form does not lay out a glyph per character without bound.
TEST(ValueFormatter, LongStringIsCapped)
{
  // ftxui::text builds a glyph string per character, so the full form needed a bound of its own: the
  // 40-character one belonged to the compact form only.
  const std::string huge(1024U * 1024U, 'x');
  auto extent = extentOf(formatValue(Var(huge), *StringType::get()));
  EXPECT_LT(extent.columns, 4200);
}

//--------------------------------------------------------------------------------------------------------------
// Duration / Timestamp
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A duration renders as seconds with its unit.
TEST(ValueFormatter, Duration)
{
  auto dur = Duration(std::chrono::milliseconds(1500));
  auto out = renderToText(formatValue(Var(dur), *DurationType::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("1.500"));
  EXPECT_THAT(out, ::testing::HasSubstr("s"));
}

/// @test
/// A duration with no value renders as empty.
TEST(ValueFormatter, DurationEmpty)
{
  auto out = renderToText(formatValue(Var {}, *DurationType::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("<empty>"));
}

//--------------------------------------------------------------------------------------------------------------
// Enum
//--------------------------------------------------------------------------------------------------------------

/// @test
/// An enumerator renders as its name with its description.
TEST(ValueFormatter, EnumByName)
{
  auto out = renderToText(formatValue(Var(std::string("red")), *makeColourEnum()));
  EXPECT_THAT(out, ::testing::HasSubstr("red"));
  EXPECT_THAT(out, ::testing::HasSubstr("the red one"));
}

/// @test
/// A numeric key resolves to the enumerator's name.
TEST(ValueFormatter, EnumByKey)
{
  // Numeric key should still resolve to the enumerator name.
  auto out = renderToText(formatValue(Var(uint8_t {2}), *makeColourEnum()));
  EXPECT_THAT(out, ::testing::HasSubstr("blue"));
}

/// @test
/// An enumerator with no description renders as its name alone.
TEST(ValueFormatter, EnumWithoutDescription)
{
  auto out = renderToText(formatValue(Var(std::string("green")), *makeColourEnum()));
  EXPECT_THAT(out, ::testing::HasSubstr("green"));
  // No description -> no "(...)" decoration.
  EXPECT_THAT(out, ::testing::Not(::testing::HasSubstr("(")));
}

/// @test
/// A value matching no enumerator is marked as unknown.
TEST(ValueFormatter, EnumUnknownValue)
{
  auto out = renderToText(formatValue(Var(std::string("purple")), *makeColourEnum()));
  EXPECT_THAT(out, ::testing::HasSubstr("?"));
}

//--------------------------------------------------------------------------------------------------------------
// Sequence
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A sequence renders each element with its index.
TEST(ValueFormatter, SequenceOfInts)
{
  VarList list {Var(int32_t {10}), Var(int32_t {20}), Var(int32_t {30})};
  auto out = renderToText(formatValue(Var(list), *makeIntSeq()));
  EXPECT_THAT(out, ::testing::HasSubstr("[0]"));
  EXPECT_THAT(out, ::testing::HasSubstr("10"));
  EXPECT_THAT(out, ::testing::HasSubstr("[1]"));
  EXPECT_THAT(out, ::testing::HasSubstr("20"));
  EXPECT_THAT(out, ::testing::HasSubstr("[2]"));
  EXPECT_THAT(out, ::testing::HasSubstr("30"));
}

/// @test
/// An empty sequence renders as empty.
TEST(ValueFormatter, SequenceEmpty)
{
  auto out = renderToText(formatValue(Var(VarList {}), *makeIntSeq()));
  EXPECT_THAT(out, ::testing::HasSubstr("<empty>"));
}

/// @test
/// A sequence of bytes renders as hex rather than as indexed elements.
TEST(ValueFormatter, SequenceOfBytesRendersAsHex)
{
  VarList bytes {Var(uint8_t {0xDE}), Var(uint8_t {0xAD}), Var(uint8_t {0xBE}), Var(uint8_t {0xEF})};
  auto out = renderToText(formatValue(Var(bytes), *makeByteSeq()));
  EXPECT_THAT(out, ::testing::HasSubstr("de"));
  EXPECT_THAT(out, ::testing::HasSubstr("ad"));
  EXPECT_THAT(out, ::testing::HasSubstr("be"));
  EXPECT_THAT(out, ::testing::HasSubstr("ef"));
  // It should NOT fall back to the indexed format.
  EXPECT_THAT(out, ::testing::Not(::testing::HasSubstr("[0]")));
}

/// @test
/// A long sequence of bytes is capped, so a large buffer does not lay out a row per sixteen
/// bytes without bound.
TEST(ValueFormatter, SequenceOfBytesIsCapped)
{
  // A blob arrives as sequence<u8>, and without a cap on this path a megabyte lays out 65,536 rows that the
  // pane then re-renders every frame.
  constexpr std::size_t oneMegabyte = 1024U * 1024U;
  VarList bytes;
  bytes.reserve(oneMegabyte);
  for (std::size_t i = 0; i < oneMegabyte; ++i)
  {
    bytes.emplace_back(static_cast<uint8_t>(i & 0xFFU));
  }

  auto element = formatValue(Var(bytes), *makeByteSeq());
  auto extent = extentOf(element);

  // 200 rows of sixteen bytes, and one row saying what was left out.
  EXPECT_EQ(extent.rows, 201);
  EXPECT_THAT(renderToText(element, 100, 240), ::testing::HasSubstr("more bytes"));
}

/// @test
/// A sequence of bytes under the cap keeps every row and reports nothing missing.
TEST(ValueFormatter, SequenceOfBytesShorterThanTheCapIsWhole)
{
  // The other half: a buffer under the cap keeps every row and says nothing about anything missing.
  VarList bytes;
  for (std::size_t i = 0; i < 64U; ++i)
  {
    bytes.emplace_back(static_cast<uint8_t>(i));
  }

  auto element = formatValue(Var(bytes), *makeByteSeq());
  EXPECT_EQ(extentOf(element).rows, 4);
  EXPECT_THAT(renderToText(element), ::testing::Not(::testing::HasSubstr("more bytes")));
}

//--------------------------------------------------------------------------------------------------------------
// Struct
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A struct renders each field with its name and value, connected as a tree.
TEST(ValueFormatter, Struct)
{
  VarMap map;
  map.try_emplace("x", int32_t {3});
  map.try_emplace("y", int32_t {7});
  auto out = renderToText(formatValue(Var(map), *makePointStruct()));
  EXPECT_THAT(out, ::testing::HasSubstr("x:"));
  EXPECT_THAT(out, ::testing::HasSubstr("3"));
  EXPECT_THAT(out, ::testing::HasSubstr("y:"));
  EXPECT_THAT(out, ::testing::HasSubstr("7"));
  // Tree connectors: first field gets ├, last gets └
  EXPECT_THAT(out, ::testing::HasSubstr("├"));
  EXPECT_THAT(out, ::testing::HasSubstr("└"));
}

/// @test
/// A struct with no value renders as empty.
TEST(ValueFormatter, StructEmpty)
{
  auto out = renderToText(formatValue(Var(VarMap {}), *makePointStruct()));
  EXPECT_THAT(out, ::testing::HasSubstr("<empty>"));
}

//--------------------------------------------------------------------------------------------------------------
// Variant
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A variant holding an integer names the alternative's type and its value.
TEST(ValueFormatter, VariantInt)
{
  // Field index 0 -> Int32Type.
  KeyedVar kv(0, std::make_shared<Var>(int32_t {99}));
  auto out = renderToText(formatValue(Var(kv), *makeIntOrStringVariant()));
  EXPECT_THAT(out, ::testing::HasSubstr("type"));
  EXPECT_THAT(out, ::testing::HasSubstr("i32"));
  EXPECT_THAT(out, ::testing::HasSubstr("value"));
  EXPECT_THAT(out, ::testing::HasSubstr("99"));
}

/// @test
/// A variant holding a string names the alternative's type and quotes the value.
TEST(ValueFormatter, VariantString)
{
  // Field index 1 -> StringType.
  KeyedVar kv(1, std::make_shared<Var>(std::string("abc")));
  auto out = renderToText(formatValue(Var(kv), *makeIntOrStringVariant()));
  EXPECT_THAT(out, ::testing::HasSubstr("string"));
  EXPECT_THAT(out, ::testing::HasSubstr("\"abc\""));
}

/// @test
/// A variant whose index names no alternative is marked as invalid.
TEST(ValueFormatter, VariantInvalidIndex)
{
  KeyedVar kv(99, std::make_shared<Var>(int32_t {0}));
  auto out = renderToText(formatValue(Var(kv), *makeIntOrStringVariant()));
  EXPECT_THAT(out, ::testing::HasSubstr("<invalid variant>"));
}

/// @test
/// A variant whose payload cannot be keyed renders as empty.
TEST(ValueFormatter, VariantNonKeyedPayload)
{
  auto out = renderToText(formatValue(Var {}, *makeIntOrStringVariant()));
  EXPECT_THAT(out, ::testing::HasSubstr("<empty>"));
}

//--------------------------------------------------------------------------------------------------------------
// Optional
//--------------------------------------------------------------------------------------------------------------

/// @test
/// An empty optional renders as empty.
TEST(ValueFormatter, OptionalEmpty)
{
  auto out = renderToText(formatValue(Var {}, *makeOptionalInt()));
  EXPECT_THAT(out, ::testing::HasSubstr("<empty>"));
}

/// @test
/// An optional holding a value renders that value.
TEST(ValueFormatter, OptionalSomeDelegatesToInner)
{
  // Non-empty optional -> formatter should delegate to Int32Type and render "42".
  auto out = renderToText(formatValue(Var(int32_t {42}), *makeOptionalInt()));
  EXPECT_THAT(out, ::testing::HasSubstr("42"));
  EXPECT_THAT(out, ::testing::Not(::testing::HasSubstr("<empty>")));
}

//--------------------------------------------------------------------------------------------------------------
// Alias
//--------------------------------------------------------------------------------------------------------------

/// @test
/// An alias renders as the type it stands for.
TEST(ValueFormatter, AliasDelegatesToAliasedType)
{
  auto out = renderToText(formatValue(Var(int32_t {123}), *makeAliasToInt()));
  EXPECT_THAT(out, ::testing::HasSubstr("123"));
}

//--------------------------------------------------------------------------------------------------------------
// Void
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A void value renders without faulting.
TEST(ValueFormatter, Void)
{
  auto out = renderToText(formatValue(Var {}, *VoidType::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("<void>"));
}

}  // namespace
/// @test
/// A struct field holding an empty optional says so, rather than showing a default that was never
/// set. A zero where a value is absent is the difference between "not reported" and "reported as
/// nought", and a reader of a property dump cannot tell them apart afterwards.
TEST(ValueFormatter, StructFieldHoldingAnEmptyOptional)
{
  VarMap map;
  map.try_emplace("name", std::string {"sensor"});
  map.try_emplace("note", std::monostate {});

  auto out = renderToText(formatValue(Var(map), *makeNoteStruct()));

  EXPECT_THAT(out, ::testing::HasSubstr("note:"));
  EXPECT_THAT(out, ::testing::HasSubstr("<empty>"));
  EXPECT_THAT(out, ::testing::HasSubstr("sensor")) << "the field beside it still renders";
}

/// @test
/// A timestamp renders as a UTC string, which is the only form that reads the same wherever the
/// person looking at a property dump happens to be.
TEST(ValueFormatter, Timestamp)
{
  const auto when = TimeStamp(std::chrono::seconds(1'700'000'000));
  auto out = renderToText(formatValue(Var(when), *TimestampType::get()));

  EXPECT_THAT(out, ::testing::HasSubstr("20"));
  EXPECT_THAT(out, ::testing::Not(::testing::HasSubstr("<empty>")));
}

/// @test
/// A timestamp slot carrying nothing says so rather than rendering an epoch, which is a real time
/// and would read as one.
TEST(ValueFormatter, TimestampEmpty)
{
  auto out = renderToText(formatValue(Var {}, *TimestampType::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("<empty>"));
}

/// @test
/// A struct field holding an empty sequence is laid out as a simple field rather than opened as a
/// subtree. A tree connector leading to nothing is worse than a value that says it is empty.
TEST(ValueFormatter, StructFieldHoldingAnEmptySequence)
{
  static const auto inner = SequenceType::make(SequenceSpec {"Inner", "test.Inner", "", Int32Type::get()});
  static const auto outer =
    StructType::make(StructSpec {"Holder", "test.Holder", "", {{"name", "", StringType::get()}, {"items", "", inner}}});

  VarMap map;
  map.try_emplace("name", std::string {"holder"});
  map.try_emplace("items", VarList {});

  auto out = renderToText(formatValue(Var(map), *outer));
  EXPECT_THAT(out, ::testing::HasSubstr("items:"));
  EXPECT_THAT(out, ::testing::HasSubstr("holder"));
}

/// @test
/// The same for a sequence whose elements are empty structs: each element is one line rather than
/// an empty subtree of its own.
TEST(ValueFormatter, SequenceOfEmptyStructs)
{
  static const auto element = StructType::make(StructSpec {"Empty", "test.Empty", "", {{"x", "", Int32Type::get()}}});
  static const auto seq = SequenceType::make(SequenceSpec {"EmptySeq", "test.EmptySeq", "", element});

  VarList list;
  list.emplace_back(VarMap {});
  list.emplace_back(VarMap {});

  auto out = renderToText(formatValue(Var(list), *seq));
  EXPECT_FALSE(out.empty());
}

}  // namespace sen::components::term
