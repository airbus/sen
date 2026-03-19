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

TYPED_TEST(StructTraitsBaseTest, ThrowsIfNoFieldIndexIsPassedToFieldValueGetterFunctionOnNonEmptyStruct)
{
  // arrange
  std::vector<uint16_t> fieldIndices {};
  const auto fields {sen::makeSpan(fieldIndices)};
  ASSERT_TRUE(fields.empty());

  // act and assert
  if constexpr (std::is_same_v<TypeParam, MyEmptyStruct>)
  {
    GTEST_SKIP();
  }
  else
  {
#ifndef NDEBUG
    ASSERT_THROW(sen::VariantTraits<TypeParam>::getFieldValueGetterFunction({}), cpptrace::runtime_error);
#else
    ASSERT_THROW(sen::VariantTraits<TypeParam>::getFieldValueGetterFunction(fields), std::runtime_error);
#endif
  }
}

TYPED_TEST(StructTraitsBaseTest, ThrowsIfAnInvalidFieldIndexIsPassedToFieldValueGetterFunctionOnNonEmptyStruct)
{
  // arrange
  std::array<uint16_t, 3UL> invalidFieldIndex {10};
  const auto fields {sen::makeSpan(invalidFieldIndex)};

  // act and assert
  if constexpr (std::is_same_v<TypeParam, MyEmptyStruct>)
  {
    GTEST_SKIP();
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

TYPED_TEST(StructTraitsBaseTest, ThrowsIfNonNativeFieldIsAccessedViaFieldValueGetterFunction)
{
  // arrange
  std::array<uint16_t, 3UL> fieldIndexWithNonNativeType {0};
  const auto fields {sen::makeSpan(fieldIndexWithNonNativeType)};

  // act and assert
  if constexpr (std::is_same_v<TypeParam, MyEmptyStruct>)
  {
    GTEST_SKIP();
  }
  else if constexpr (std::is_same_v<TypeParam, MyStructWithNativeFieldsOnly>)
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

TYPED_TEST(StructTraitsBaseTest, ThrowsIfVarDoesNotHoldAVarMapWhenConvertingFromVarToValue)
{
  // arrange
  sen::Var myVar {};
  sen::Var myInvalidVar {true};
  TypeParam myGenericStruct {};

  // act and assert
  if constexpr (std::is_same_v<TypeParam, MyEmptyStruct>)
  {
    GTEST_SKIP();
  }
  else
  {
    ASSERT_THROW(sen::VariantTraits<TypeParam>::variantToValue(myInvalidVar, myGenericStruct), std::runtime_error);
    sen::VariantTraits<TypeParam>::valueToVariant(myGenericStruct, myVar);
    ASSERT_NO_THROW(sen::VariantTraits<TypeParam>::variantToValue(myVar, myGenericStruct));
  }
}

}  // namespace
