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
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
#include <string>

namespace sen::components::term
{
namespace
{

using test::renderToText;

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

TEST(ValueFormatter, BoolTrue)
{
  auto out = renderToText(formatValue(Var(true), *BoolType::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("true"));
}

TEST(ValueFormatter, BoolFalse)
{
  auto out = renderToText(formatValue(Var(false), *BoolType::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("false"));
}

TEST(ValueFormatter, Int32Positive)
{
  auto out = renderToText(formatValue(Var(int32_t {42}), *Int32Type::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("42"));
}

TEST(ValueFormatter, Int32Negative)
{
  auto out = renderToText(formatValue(Var(int32_t {-17}), *Int32Type::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("-17"));
}

TEST(ValueFormatter, UInt8)
{
  auto out = renderToText(formatValue(Var(uint8_t {255}), *UInt8Type::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("255"));
}

TEST(ValueFormatter, Int64Large)
{
  auto out = renderToText(formatValue(Var(int64_t {1234567890123}), *Int64Type::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("1234567890123"));
}

TEST(ValueFormatter, Float64)
{
  auto out = renderToText(formatValue(Var(double {3.14}), *Float64Type::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("3.14"));
}

//--------------------------------------------------------------------------------------------------------------
// String
//--------------------------------------------------------------------------------------------------------------

TEST(ValueFormatter, StringQuoted)
{
  auto out = renderToText(formatValue(Var(std::string("hello")), *StringType::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("\"hello\""));
}

TEST(ValueFormatter, StringEmptyVar)
{
  // Var with no string payload, formatter should fall back to <empty>.
  auto out = renderToText(formatValue(Var {}, *StringType::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("<empty>"));
}

//--------------------------------------------------------------------------------------------------------------
// Duration / Timestamp
//--------------------------------------------------------------------------------------------------------------

TEST(ValueFormatter, Duration)
{
  auto dur = Duration(std::chrono::milliseconds(1500));
  auto out = renderToText(formatValue(Var(dur), *DurationType::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("1.500"));
  EXPECT_THAT(out, ::testing::HasSubstr("s"));
}

TEST(ValueFormatter, DurationEmpty)
{
  auto out = renderToText(formatValue(Var {}, *DurationType::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("<empty>"));
}

//--------------------------------------------------------------------------------------------------------------
// Enum
//--------------------------------------------------------------------------------------------------------------

TEST(ValueFormatter, EnumByName)
{
  auto out = renderToText(formatValue(Var(std::string("red")), *makeColourEnum()));
  EXPECT_THAT(out, ::testing::HasSubstr("red"));
  EXPECT_THAT(out, ::testing::HasSubstr("the red one"));
}

TEST(ValueFormatter, EnumByKey)
{
  // Numeric key should still resolve to the enumerator name.
  auto out = renderToText(formatValue(Var(uint8_t {2}), *makeColourEnum()));
  EXPECT_THAT(out, ::testing::HasSubstr("blue"));
}

TEST(ValueFormatter, EnumWithoutDescription)
{
  auto out = renderToText(formatValue(Var(std::string("green")), *makeColourEnum()));
  EXPECT_THAT(out, ::testing::HasSubstr("green"));
  // No description -> no "(...)" decoration.
  EXPECT_THAT(out, ::testing::Not(::testing::HasSubstr("(")));
}

TEST(ValueFormatter, EnumUnknownValue)
{
  auto out = renderToText(formatValue(Var(std::string("purple")), *makeColourEnum()));
  EXPECT_THAT(out, ::testing::HasSubstr("?"));
}

//--------------------------------------------------------------------------------------------------------------
// Sequence
//--------------------------------------------------------------------------------------------------------------

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

TEST(ValueFormatter, SequenceEmpty)
{
  auto out = renderToText(formatValue(Var(VarList {}), *makeIntSeq()));
  EXPECT_THAT(out, ::testing::HasSubstr("<empty>"));
}

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

//--------------------------------------------------------------------------------------------------------------
// Struct
//--------------------------------------------------------------------------------------------------------------

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

TEST(ValueFormatter, StructEmpty)
{
  auto out = renderToText(formatValue(Var(VarMap {}), *makePointStruct()));
  EXPECT_THAT(out, ::testing::HasSubstr("<empty>"));
}

//--------------------------------------------------------------------------------------------------------------
// Variant
//--------------------------------------------------------------------------------------------------------------

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

TEST(ValueFormatter, VariantString)
{
  // Field index 1 -> StringType.
  KeyedVar kv(1, std::make_shared<Var>(std::string("abc")));
  auto out = renderToText(formatValue(Var(kv), *makeIntOrStringVariant()));
  EXPECT_THAT(out, ::testing::HasSubstr("string"));
  EXPECT_THAT(out, ::testing::HasSubstr("\"abc\""));
}

TEST(ValueFormatter, VariantInvalidIndex)
{
  KeyedVar kv(99, std::make_shared<Var>(int32_t {0}));
  auto out = renderToText(formatValue(Var(kv), *makeIntOrStringVariant()));
  EXPECT_THAT(out, ::testing::HasSubstr("<invalid variant>"));
}

TEST(ValueFormatter, VariantNonKeyedPayload)
{
  auto out = renderToText(formatValue(Var {}, *makeIntOrStringVariant()));
  EXPECT_THAT(out, ::testing::HasSubstr("<empty>"));
}

//--------------------------------------------------------------------------------------------------------------
// Optional
//--------------------------------------------------------------------------------------------------------------

TEST(ValueFormatter, OptionalEmpty)
{
  auto out = renderToText(formatValue(Var {}, *makeOptionalInt()));
  EXPECT_THAT(out, ::testing::HasSubstr("<empty>"));
}

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

TEST(ValueFormatter, AliasDelegatesToAliasedType)
{
  auto out = renderToText(formatValue(Var(int32_t {123}), *makeAliasToInt()));
  EXPECT_THAT(out, ::testing::HasSubstr("123"));
}

//--------------------------------------------------------------------------------------------------------------
// Void
//--------------------------------------------------------------------------------------------------------------

TEST(ValueFormatter, Void)
{
  auto out = renderToText(formatValue(Var {}, *VoidType::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("<void>"));
}

}  // namespace
}  // namespace sen::components::term
