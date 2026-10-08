// === struct_traits_test.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/span.h"
#include "sen/core/meta/type_traits.h"
#include "sen/core/meta/var.h"

// generated code
#include "stl/test_struct_traits.stl.h"

// google test
#include <gtest/gtest.h>

// cpptrace
#ifndef NDEBUG
#  include "cpptrace/exceptions.hpp"
#endif

// std
#include <array>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <vector>

using test_struct_traits::MyEmptyStruct;
using test_struct_traits::MyStructWithNativeFieldsOnly;
using test_struct_traits::MyStructWithNonNativeFields;

namespace
{

template <typename T>
class StructTraitsBaseTest: public ::testing::Test
{
};

using GeneratedSenClasses = ::testing::Types<MyEmptyStruct, MyStructWithNativeFieldsOnly, MyStructWithNonNativeFields>;
TYPED_TEST_SUITE(StructTraitsBaseTest, GeneratedSenClasses);

/// The cases below are about field indices, and a struct with no fields has no index to get
/// wrong. Instantiating them over the structs that have fields says that in the type list, where
/// a reader sees it. Asking the empty struct and skipping left four cases reported as skipped in
/// every run, which reads as something unfinished rather than as something that does not apply.
template <typename T>
class StructWithFieldsTraitsTest: public ::testing::Test
{
};

using GeneratedSenClassesWithFields = ::testing::Types<MyStructWithNativeFieldsOnly, MyStructWithNonNativeFields>;
TYPED_TEST_SUITE(StructWithFieldsTraitsTest, GeneratedSenClassesWithFields);

/// @test
/// Refuses a field getter request on a struct with no fields and accepts the same request on
/// structs that have fields.
TYPED_TEST(StructTraitsBaseTest, ThrowsIfTryingToAccessFieldValueGetterFunctionOnEmptyStruct)
{
  // arrange
  std::array<uint16_t, 1UL> fieldIndices {2};
  const auto fields {sen::makeSpan(fieldIndices)};

  // act and assert
  if constexpr (std::is_same_v<TypeParam, MyEmptyStruct>)
  {
#ifndef NDEBUG
    ASSERT_THROW(sen::VariantTraits<MyEmptyStruct>::getFieldValueGetterFunction(fields), cpptrace::runtime_error);
#else
    ASSERT_THROW(sen::VariantTraits<MyEmptyStruct>::getFieldValueGetterFunction(fields), std::runtime_error);
#endif
  }
  else
  {
    ASSERT_NO_THROW(sen::VariantTraits<TypeParam>::getFieldValueGetterFunction(fields));
  }
}

/// @test
/// Asking for a field getter with no field index is refused.
TYPED_TEST(StructWithFieldsTraitsTest, ThrowsIfNoFieldIndexIsPassedToFieldValueGetterFunctionOnNonEmptyStruct)
{
  // arrange
  std::vector<uint16_t> fieldIndices {};
  const auto fields {sen::makeSpan(fieldIndices)};
  ASSERT_TRUE(fields.empty());

  // act and assert
#ifndef NDEBUG
  ASSERT_THROW(sen::VariantTraits<TypeParam>::getFieldValueGetterFunction({}), cpptrace::runtime_error);
#else
  ASSERT_THROW(sen::VariantTraits<TypeParam>::getFieldValueGetterFunction(fields), std::runtime_error);
#endif
}

/// @test
/// A field index outside the struct's fields is refused.
TYPED_TEST(StructWithFieldsTraitsTest, ThrowsIfAnInvalidFieldIndexIsPassedToFieldValueGetterFunctionOnNonEmptyStruct)
{
  // arrange
  std::array<uint16_t, 3UL> invalidFieldIndex {10};
  const auto fields {sen::makeSpan(invalidFieldIndex)};

  // act and assert
#ifndef NDEBUG
  ASSERT_THROW(sen::VariantTraits<TypeParam>::getFieldValueGetterFunction(fields), cpptrace::runtime_error);
#else
  ASSERT_THROW(sen::VariantTraits<TypeParam>::getFieldValueGetterFunction(fields), std::runtime_error);
#endif
}

/// @test
/// Refuses a field getter for a field whose type is not native and produces one when the field
/// at the same index is native.
TYPED_TEST(StructWithFieldsTraitsTest, ThrowsIfNonNativeFieldIsAccessedViaFieldValueGetterFunction)
{
  // arrange
  std::array<uint16_t, 3UL> fieldIndexWithNonNativeType {0};
  const auto fields {sen::makeSpan(fieldIndexWithNonNativeType)};

  // act and assert
  if constexpr (std::is_same_v<TypeParam, MyStructWithNativeFieldsOnly>)
  {
    ASSERT_NO_THROW(sen::VariantTraits<TypeParam>::getFieldValueGetterFunction(fields));
  }
  else
  {
#ifndef NDEBUG
    ASSERT_THROW(sen::VariantTraits<TypeParam>::getFieldValueGetterFunction(fields), cpptrace::runtime_error);
#else
    ASSERT_THROW(sen::VariantTraits<TypeParam>::getFieldValueGetterFunction(fields), std::runtime_error);
#endif
  }
}

/// @test
/// Converting to a struct from a value that is not a map is refused, while a value written from
/// the struct converts back.
TYPED_TEST(StructWithFieldsTraitsTest, ThrowsIfVarDoesNotHoldAVarMapWhenConvertingFromVarToValue)
{
  // arrange
  sen::Var myVar {};
  sen::Var myInvalidVar {true};
  TypeParam myGenericStruct {};

  // act and assert
  ASSERT_THROW(sen::VariantTraits<TypeParam>::variantToValue(myInvalidVar, myGenericStruct), std::runtime_error);
  sen::VariantTraits<TypeParam>::valueToVariant(myGenericStruct, myVar);
  ASSERT_NO_THROW(sen::VariantTraits<TypeParam>::variantToValue(myVar, myGenericStruct));
}

}  // namespace
