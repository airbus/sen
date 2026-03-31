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
#include "sen/core/meta/var.h"

// generated code
#include "stl/test_variant_traits.stl.h"

// google test
#include <gtest/gtest.h>

// cpptrace
#ifndef NDEBUG
#  include "cpptrace/exceptions.hpp"
#endif

// std
#include <array>
#include <cstdint>
#include <tuple>
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

TYPED_TEST(TestVariantTraitsBaseViaVariantTraits, ThrowsIfNoFieldIndexIsPassedToFieldValueGetterFunction)
{
  // arrange
  std::vector<uint16_t> fieldIndices {};
  const auto fields {sen::makeSpan(fieldIndices)};

  // act and assert
  ASSERT_TRUE(fields.empty());
#ifndef NDEBUG
  ASSERT_THROW(sen::VariantTraits<MockVariant>::getFieldValueGetterFunction(fieldIndices), cpptrace::runtime_error);
#else
  ASSERT_THROW(sen::VariantTraits<MockVariant>::getFieldValueGetterFunction(fields), std::runtime_error);
#endif
}

TYPED_TEST(TestVariantTraitsBaseViaVariantTraits, ThrowsIfNonNativeFieldIsAccessedViaFieldValueGetterFunction)
{
  // arrange
  std::array<uint16_t, 1UL> fieldIndexWithNonNativeType {4};
  const auto fields {sen::makeSpan(fieldIndexWithNonNativeType)};

  // act and assert
#ifndef NDEBUG
  ASSERT_THROW(sen::VariantTraits<MockVariant>::getFieldValueGetterFunction(fields), cpptrace::runtime_error);
#else
  ASSERT_THROW(sen::VariantTraits<MockVariant>::getFieldValueGetterFunction(fields), std::runtime_error);
#endif
}

TYPED_TEST(TestVariantTraitsBaseViaVariantTraits, ThrowsIfAnInvalidFieldIndexIsPassedToFieldValueGetterFunction)
{
  // arrange
  std::array<uint16_t, 1UL> invalidFieldIndex {10};
  const auto fields {sen::makeSpan(invalidFieldIndex)};

  // act and assert
#ifndef NDEBUG
  ASSERT_THROW(sen::VariantTraits<MockVariant>::getFieldValueGetterFunction(fields), cpptrace::runtime_error);
#else
  ASSERT_THROW(sen::VariantTraits<MockVariant>::getFieldValueGetterFunction(fields), std::runtime_error);
#endif
}

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

TYPED_TEST(TestVariantTraitsBaseViaVariantTraits, ThrowsDuringConversionFromVarToValueIfVarHoldsInvalidType)
{
  // arrange
  const sen::Var mockVar = sen::VarMap {{"type", "Duration"}, {"value", sen::Duration {64}}};
  MockVariant mockVariant {};

  // act and assert
#ifndef NDEBUG
  ASSERT_THROW(sen::VariantTraits<MockVariant>::variantToValue(mockVar, mockVariant), cpptrace::runtime_error);
#else
  ASSERT_THROW(sen::VariantTraits<MockVariant>::variantToValue(mockVar, mockVariant), std::runtime_error);
#endif
}

template <typename VariantType>
class TestVariantTraitsBaseViaSerializationTraits: public ::testing::Test
{
};

using VariantTypes = ::testing::Types<MockStruct, f64, u32, bool, MaybeString>;
TYPED_TEST_SUITE(TestVariantTraitsBaseViaSerializationTraits, VariantTypes);

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

}  // namespace
