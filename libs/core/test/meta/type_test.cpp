// === type_test.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/type.h"

// google test
#include <gtest/gtest.h>

using sen::TypeHandle;
using sen::VoidType;

/// @test
/// Checks that makeNonOwningTypeHandle wraps a raw pointer without taking ownership
/// @requirements(SEN-355)
TEST(TypeHandle, createFromNonOwning)
{
  const VoidType* type = sen::VoidType::get().type();

  TypeHandle<const VoidType> createdTypeHandle = sen::makeNonOwningTypeHandle(type);
  const TypeHandle<const VoidType> createdConstTypeHandle = sen::makeNonOwningTypeHandle(type);

  EXPECT_EQ(type, createdTypeHandle.type());
  EXPECT_EQ(type, createdConstTypeHandle.type());
}

/// @test
/// Checks implicit owning conversion from TypeHandle<Derived> to TypeHandle<Base>
/// @requirements(SEN-355)
TEST(TypeHandle, createFromConvertableType)
{
  auto testAlias = sen::AliasType::make(sen::AliasSpec("Test", "test", "test description", sen::VoidType::get()));

  TypeHandle<sen::CustomType> testCustomType = testAlias;

  EXPECT_EQ(testAlias.type(), testCustomType.type());
}

/// @test
/// Checks implicit non-owning conversion from TypeHandle<Deriver> to TypeHandle<Base>
/// @requirements(SEN-355)
TEST(TypeHandle, createFromConvertableTypeNonOwning)
{
  auto testAlias = sen::AliasType::make(sen::AliasSpec("Test", "test", "test description", sen::VoidType::get()));

  auto nonOwningTestAlias = sen::makeNonOwningTypeHandle(testAlias.type());
  TypeHandle<sen::CustomType> testCustomType = nonOwningTestAlias;

  EXPECT_EQ(testAlias.type(), testCustomType.type());
}

/// @test
/// Checks implicit owning conversion from temporary TypeHandle<Derived>
/// @requirements(SEN-355)
TEST(TypeHandle, createFromConvertableTypeFromTmp)
{
  TypeHandle<sen::CustomType> testCustomType =
    sen::AliasType::make(sen::AliasSpec("Test", "test", "test description", sen::VoidType::get()));

  EXPECT_EQ(sen::VoidType::get(), testCustomType->asAliasType()->getAliasedType());
}

/// @test
/// Checks that non-owning handle converted to TypeHandle<Base> preserves the raw pointer
/// @requirements(SEN-355)
TEST(TypeHandle, createFromConvertableTypeNonOwningFromTmp)
{
  auto testAlias = sen::AliasType::make(sen::AliasSpec("Test", "test", "test description", sen::VoidType::get()));

  TypeHandle<sen::CustomType> testCustomType = sen::makeNonOwningTypeHandle(testAlias.type());

  EXPECT_EQ(testAlias.type(), testCustomType.type());
}

/// @test
/// Verifies operator== and operator!=
/// @requirements(SEN-355)
TEST(TypeHandle, testComparisions)
{
  const VoidType* type = sen::VoidType::get().type();
  auto otherTypeHandle = sen::Int32Type::get();

  TypeHandle<const VoidType> createdTypeHandle = sen::makeNonOwningTypeHandle(type);
  const TypeHandle<const VoidType> createdConstTypeHandle = sen::makeNonOwningTypeHandle(type);

  // TypeHandle compared against TypeHandle
  EXPECT_TRUE(createdConstTypeHandle == createdTypeHandle);
  EXPECT_FALSE(createdConstTypeHandle != createdTypeHandle);

  EXPECT_FALSE(createdConstTypeHandle == otherTypeHandle);
  EXPECT_TRUE(createdConstTypeHandle != otherTypeHandle);
  EXPECT_FALSE(createdTypeHandle == otherTypeHandle);
  EXPECT_TRUE(createdTypeHandle != otherTypeHandle);

  EXPECT_FALSE(otherTypeHandle == createdConstTypeHandle);
  EXPECT_TRUE(otherTypeHandle != createdConstTypeHandle);
  EXPECT_FALSE(otherTypeHandle == createdTypeHandle);
  EXPECT_TRUE(otherTypeHandle != createdTypeHandle);

  // TypeHandle compared against Type
  EXPECT_TRUE(createdTypeHandle == *type);
  EXPECT_FALSE(createdTypeHandle != *type);
  EXPECT_TRUE(createdConstTypeHandle == *type);
  EXPECT_FALSE(createdConstTypeHandle != *type);

  EXPECT_FALSE(createdTypeHandle == *otherTypeHandle.type());
  EXPECT_TRUE(createdTypeHandle != *otherTypeHandle.type());
  EXPECT_FALSE(createdConstTypeHandle == *otherTypeHandle.type());
  EXPECT_TRUE(createdConstTypeHandle != *otherTypeHandle.type());

  // Type compared against TypeHandle
  EXPECT_TRUE(*type == createdTypeHandle);
  EXPECT_FALSE(*type != createdTypeHandle);
  EXPECT_TRUE(*type == createdConstTypeHandle);
  EXPECT_FALSE(*type != createdConstTypeHandle);

  EXPECT_FALSE(*otherTypeHandle.type() == createdTypeHandle);
  EXPECT_TRUE(*otherTypeHandle.type() != createdTypeHandle);
  EXPECT_FALSE(*otherTypeHandle.type() == createdConstTypeHandle);
  EXPECT_TRUE(*otherTypeHandle.type() != createdConstTypeHandle);
}

/// @test
/// Verifies type(), operator->() and operator*()
/// @requirements(SEN-355)
TEST(TypeHandle, testAccessCapabilities)
{
  const VoidType* type = sen::VoidType::get().type();
  TypeHandle<const VoidType> createdTypeHandle = sen::makeNonOwningTypeHandle(type);
  const TypeHandle<const VoidType> createdConstTypeHandle = sen::makeNonOwningTypeHandle(type);

  EXPECT_EQ(type, createdConstTypeHandle.type());
  EXPECT_EQ(type, createdConstTypeHandle.type());

  EXPECT_EQ(type, createdConstTypeHandle.operator->());
  EXPECT_EQ(type, createdConstTypeHandle.operator->());

  EXPECT_EQ(*type, *createdConstTypeHandle);
  EXPECT_EQ(*type, *createdConstTypeHandle);
}

/// @test
/// Checks dynamicTypeHandleCast succeeds when the managed type is convertible to the target
/// @requirements(SEN-355)
TEST(DynamicTypeHandleCast, correctUpwardsCast)
{
  auto testAlias = sen::AliasType::make(sen::AliasSpec("Test", "test", "test description", sen::VoidType::get()));

  TypeHandle<sen::CustomType> testCustomType = testAlias;

  auto maybeUpcastedAliasType = sen::dynamicTypeHandleCast<sen::AliasType>(testCustomType);

  ASSERT_TRUE(maybeUpcastedAliasType.has_value());
  EXPECT_EQ(testAlias, maybeUpcastedAliasType.value());
}

/// @test
/// Checks dynamicTypeHandleCast returns nullopt when the managed type is not convertible
/// @requirements(SEN-355)
TEST(DynamicTypeHandleCast, notallowedUpwardsCast)
{
  auto testAlias = sen::AliasType::make(sen::AliasSpec("Test", "test", "test description", sen::VoidType::get()));

  TypeHandle<sen::CustomType> testCustomType = testAlias;

  auto maybeUpcastedVoidType = sen::dynamicTypeHandleCast<sen::VoidType>(testCustomType);

  EXPECT_FALSE(maybeUpcastedVoidType.has_value());
}

/// @test
/// Checks all validation branches of validateLowerCaseName
/// @requirements(SEN-355)
TEST(Type, validateLowerCaseName)
{
  // valid name
  {
    auto result = sen::Type::validateLowerCaseName("validName123");
    EXPECT_TRUE(result.isOk());
  }

  // valid name with dot separator
  {
    auto result = sen::Type::validateLowerCaseName("some.name");
    EXPECT_TRUE(result.isOk());
  }

  // empty name
  {
    auto result = sen::Type::validateLowerCaseName("");
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.getError(), "empty name");
  }

  // starts with digit
  {
    auto result = sen::Type::validateLowerCaseName("1invalid");
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.getError(), "not allowed to start with digits");
  }

  // starts with uppercase letter
  {
    auto result = sen::Type::validateLowerCaseName("Invalid");
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.getError(), "not allowed to start with uppercase letter");
  }

  // starts with underscore
  {
    auto result = sen::Type::validateLowerCaseName("_invalid");
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.getError(), "not allowed to start with _");
  }

  // starts with dot
  {
    auto result = sen::Type::validateLowerCaseName(".invalid");
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.getError(), "not allowed to start with .");
  }

  // contains invalid character
  {
    auto result = sen::Type::validateLowerCaseName("hello!world");
    EXPECT_TRUE(result.isError());
    EXPECT_NE(result.getError().find("name must only contain alpha-numeric characters"), std::string::npos);
    EXPECT_NE(result.getError().find("!"), std::string::npos);
  }
}

/// @test
/// Checks all validation branches of validateTypeName
/// @requirements(SEN-355)
TEST(Type, validateTypeName)
{
  // valid name
  {
    auto result = sen::Type::validateTypeName("ValidName123");
    EXPECT_TRUE(result.isOk());
  }

  // valid name with dot separator
  {
    auto result = sen::Type::validateTypeName("Some.Qualified.Name");
    EXPECT_TRUE(result.isOk());
  }

  // empty name
  {
    auto result = sen::Type::validateTypeName("");
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.getError(), "empty name");
  }

  // starts with digit
  {
    auto result = sen::Type::validateTypeName("1Invalid");
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.getError(), "not allowed to start with digits");
  }

  // starts with lowercase letter
  {
    auto result = sen::Type::validateTypeName("invalid");
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.getError(), "not allowed to start with lowercase letter");
  }

  // starts with underscore
  {
    auto result = sen::Type::validateTypeName("_Invalid");
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.getError(), "not allowed to start with _");
  }

  // starts with dot
  {
    auto result = sen::Type::validateTypeName(".Invalid");
    EXPECT_TRUE(result.isError());
    EXPECT_EQ(result.getError(), "not allowed to start with .");
  }

  // contains invalid character
  {
    auto result = sen::Type::validateTypeName("Hello!World");
    EXPECT_TRUE(result.isError());
    EXPECT_NE(result.getError().find("name must only contain alpha-numeric characters"), std::string::npos);
    EXPECT_NE(result.getError().find("!"), std::string::npos);
  }
}
