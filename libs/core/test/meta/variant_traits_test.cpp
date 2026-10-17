// === variant_traits_test.cpp =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/base/span.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/lang/vm.h"
#include "sen/core/meta/var.h"

// generated code
#include "stl/test_variant_traits.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <array>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

using test_variant_traits::MaybeString;
using test_variant_traits::MockStruct;
using test_variant_traits::MockVariant;

namespace
{

template <typename VariantType>
MockVariant makeMockVariant()
{
  if constexpr (std::is_same_v<VariantType, MockStruct>)
  {
    return MockStruct {32, "testString"};
  }
  if constexpr (std::is_same_v<VariantType, f64>)
  {
    return f64 {64.0};
  }
  if constexpr (std::is_same_v<VariantType, u32>)
  {
    return u32 {32};
  }
  if constexpr (std::is_same_v<VariantType, bool>)
  {
    return true;
  }
  if constexpr (std::is_same_v<VariantType, MaybeString>)
  {
    return MaybeString {"maybeString"};
  }
  return {};
}

template <typename VariantType>
class TestVariantTraitsBaseViaVariantTraits: public ::testing::Test
{
};

using VariantTypes = ::testing::Types<MockStruct, f64, u32, bool, MaybeString>;
TYPED_TEST_SUITE(TestVariantTraitsBaseViaVariantTraits, VariantTypes);

/// @test
/// Asking for a field getter with no field index is refused.
TYPED_TEST(TestVariantTraitsBaseViaVariantTraits, ThrowsIfNoFieldIndexIsPassedToFieldValueGetterFunction)
{
  // arrange
  std::vector<uint16_t> fieldIndices {};
  const auto fields {sen::makeSpan(fieldIndices)};

  // act and assert
  ASSERT_TRUE(fields.empty());
  ASSERT_THROW(sen::VariantTraits<MockVariant>::getFieldValueGetterFunction(fields), std::runtime_error);
}

/// @test
/// A field whose type is not native cannot be read through the field getter.
TYPED_TEST(TestVariantTraitsBaseViaVariantTraits, ThrowsIfNonNativeFieldIsAccessedViaFieldValueGetterFunction)
{
  // arrange
  std::array<uint16_t, 1UL> fieldIndexWithNonNativeType {4};
  const auto fields {sen::makeSpan(fieldIndexWithNonNativeType)};

  // act and assert
  ASSERT_THROW(sen::VariantTraits<MockVariant>::getFieldValueGetterFunction(fields), std::runtime_error);
}

/// @test
/// A field index outside the variant's alternatives is refused.
TYPED_TEST(TestVariantTraitsBaseViaVariantTraits, ThrowsIfAnInvalidFieldIndexIsPassedToFieldValueGetterFunction)
{
  // arrange
  std::array<uint16_t, 1UL> invalidFieldIndex {10};
  const auto fields {sen::makeSpan(invalidFieldIndex)};

  // act and assert
  ASSERT_THROW(sen::VariantTraits<MockVariant>::getFieldValueGetterFunction(fields), std::runtime_error);
}

/// @test
/// A variant converts to a value carrying its alternative index and contents of that
/// alternative's type.
TYPED_TEST(TestVariantTraitsBaseViaVariantTraits, IsConvertibleFromValueToVar)
{
  // arrange
  sen::Var mockVar {};
  const MockVariant mockVariant {makeMockVariant<TypeParam>()};

  // act
  sen::VariantTraits<MockVariant>::valueToVariant(mockVariant, mockVar);

  // assert
  ASSERT_TRUE(mockVar.holds<sen::KeyedVar>());
  const auto [index, var] = mockVar.get<sen::KeyedVar>();
  ASSERT_EQ(index, mockVariant.index());
  if constexpr (std::is_same_v<TypeParam, MockStruct>)
  {
    ASSERT_TRUE(var->holds<sen::VarMap>());
  }
  else if constexpr (std::is_same_v<TypeParam, MaybeString>)
  {
    ASSERT_TRUE(var->holds<MaybeString::value_type>());
  }
  else
  {
    ASSERT_TRUE(var->holds<TypeParam>());
  }
}

/// @test
/// A value naming the alternative by index converts back to the variant.
TYPED_TEST(TestVariantTraitsBaseViaVariantTraits, IsConvertibleFromVarToValueIfVarHoldsTypeIndexAndvalue)
{
  // arrange
  sen::Var mockVar {};
  sen::VariantTraits<MockVariant>::valueToVariant(makeMockVariant<TypeParam>(), mockVar);
  MockVariant mockVariant {};

  // act
  sen::VariantTraits<MockVariant>::variantToValue(mockVar, mockVariant);

  // assert
  ASSERT_TRUE(std::holds_alternative<TypeParam>(mockVariant));
  EXPECT_EQ(mockVariant, makeMockVariant<TypeParam>());
}

/// @test
/// A value naming the alternative by name converts back to the variant.
TYPED_TEST(TestVariantTraitsBaseViaVariantTraits, IsConvertibleFromVarToValueIfVarHoldsTypeNameAndValue)
{
  // arrange
  const sen::Var mockVar = []() -> sen::VarMap
  {
    if constexpr (std::is_same_v<TypeParam, MockStruct>)
    {
      return {{"type", "MockStruct"}, {"value", sen::VarMap {{"field1", 32}, {"field2", "testString"}}}};
    }
    if constexpr (std::is_same_v<TypeParam, f64>)
    {
      return {{"type", "f64"}, {"value", 64.0}};
    }
    if constexpr (std::is_same_v<TypeParam, u32>)
    {
      return {{"type", "u32"}, {"value", 32}};
    }
    if constexpr (std::is_same_v<TypeParam, bool>)
    {
      return {{"type", "bool"}, {"value", true}};
    }
    if constexpr (std::is_same_v<TypeParam, MaybeString>)
    {
      return {{"type", "MaybeString"}, {"value", "maybeString"}};
    }
  }();

  MockVariant mockVariant {};

  // act
  sen::VariantTraits<MockVariant>::variantToValue(mockVar, mockVariant);

  // assert
  ASSERT_TRUE(std::holds_alternative<TypeParam>(mockVariant));
  EXPECT_EQ(mockVariant, makeMockVariant<TypeParam>());
}

/// @test
/// A value naming an alternative the variant does not hold is refused.
TYPED_TEST(TestVariantTraitsBaseViaVariantTraits, ThrowsDuringConversionFromVarToValueIfVarHoldsInvalidType)
{
  // arrange
  const sen::Var mockVar = sen::VarMap {{"type", "Duration"}, {"value", sen::Duration {64}}};
  MockVariant mockVariant {};

  // act and assert
  ASSERT_THROW(sen::VariantTraits<MockVariant>::variantToValue(mockVar, mockVariant), std::runtime_error);
}

template <typename VariantType>
class TestVariantTraitsBaseViaSerializationTraits: public ::testing::Test
{
};

using VariantTypes = ::testing::Types<MockStruct, f64, u32, bool, MaybeString>;
TYPED_TEST_SUITE(TestVariantTraitsBaseViaSerializationTraits, VariantTypes);

/// @test
/// A variant written to a stream reads back as the same variant.
TYPED_TEST(TestVariantTraitsBaseViaSerializationTraits, IsWritableToOutputStreamAndReadableByInputStream)
{
  // arrange
  MockVariant mockVariant {TypeParam {}};
  // we require a buffer for the output stream to write to and for the input stream to read from
  std::vector<uint8_t> buffer;
  sen::ResizableBufferWriter writer {buffer};
  sen::OutputStream outputStream {writer};

  // act (assuming idempotency between write and read operation)
  sen::SerializationTraits<MockVariant>::write(outputStream, makeMockVariant<TypeParam>());
  sen::InputStream inputStream {buffer};
  sen::SerializationTraits<MockVariant>::read(inputStream, mockVariant);

  // assert
  ASSERT_TRUE(std::holds_alternative<TypeParam>(mockVariant));
  ASSERT_EQ(mockVariant, makeMockVariant<TypeParam>());
}

/// @test
/// The printed form of a variant, pinned exactly. The indentation comes from stream width
/// manipulation, which is easy to change by accident and which nothing else here asserts.
TEST(AVariantBeingPrinted, keepsItsFieldIndentation)
{
  std::ostringstream out;
  out << makeMockVariant<u32>();

  EXPECT_EQ(out.str(), "  type:  u32\n  value: 32");
}

/// @test
/// A field getter for an alternative that is not held returns VariantAccessError without throwing.
TEST(VariantFieldValueGetter, ReturnsVariantAccessErrorForAnAlternativeNotHeld)
{
  // arrange
  const MockVariant holdingU32 {std::in_place_index<2>, 7U};
  std::array<uint16_t, 1UL> f64Alternative {1};
  std::array<uint16_t, 2UL> structField {0, 0};
  const auto readF64 = sen::VariantTraits<MockVariant>::getFieldValueGetterFunction(sen::makeSpan(f64Alternative));
  const auto readStructField = sen::VariantTraits<MockVariant>::getFieldValueGetterFunction(sen::makeSpan(structField));

  // act and assert
  sen::lang::Value value;
  ASSERT_NO_THROW(value = readF64(&holdingU32));
  EXPECT_TRUE(std::holds_alternative<sen::lang::VariantAccessError>(value));
  ASSERT_NO_THROW(value = readStructField(&holdingU32));
  EXPECT_TRUE(std::holds_alternative<sen::lang::VariantAccessError>(value));
}

/// @test
/// A field getter for the alternative that is held returns its value.
TEST(VariantFieldValueGetter, ReturnsTheValueOfTheAlternativeHeld)
{
  // arrange
  const MockVariant holdingF64 {std::in_place_index<1>, 2.5};
  const MockVariant holdingStruct {std::in_place_index<0>, MockStruct {42U, "x"}};
  std::array<uint16_t, 1UL> f64Alternative {1};
  std::array<uint16_t, 2UL> structField {0, 0};

  // act
  const auto f64Value =
    sen::VariantTraits<MockVariant>::getFieldValueGetterFunction(sen::makeSpan(f64Alternative))(&holdingF64);
  const auto fieldValue =
    sen::VariantTraits<MockVariant>::getFieldValueGetterFunction(sen::makeSpan(structField))(&holdingStruct);

  // assert
  ASSERT_TRUE(std::holds_alternative<float64_t>(f64Value));
  EXPECT_EQ(std::get<float64_t>(f64Value), 2.5);
  ASSERT_TRUE(std::holds_alternative<uint32_t>(fieldValue));
  EXPECT_EQ(std::get<uint32_t>(fieldValue), 42U);
}

}  // namespace
