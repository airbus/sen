// === type_visitor_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_match.h"
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/unit.h"
#include "sen/core/meta/variant_type.h"

// google test
#include <gtest/gtest.h>

// std
#include <vector>

namespace
{

template <typename T>
class TypeChecker final: public sen::TypeVisitor
{
  SEN_NOCOPY_NOMOVE(TypeChecker)

public:
  TypeChecker() = default;
  ~TypeChecker() final = default;

public:
  static void checkValid(const sen::Type& type)
  {
    TypeChecker visitor;
    type.accept(visitor);
    EXPECT_EQ(visitor.getVisitedType(), &type);
  }

  static void checkInvalid(const sen::Type& type)
  {
    TypeChecker visitor;
    type.accept(visitor);
    EXPECT_EQ(visitor.getVisitedType(), nullptr);
  }

public:
  void apply(const T& type) final { visitedType_ = &type; }

  [[nodiscard]] const sen::Type* getVisitedType() const noexcept { return visitedType_; }

private:
  const sen::Type* visitedType_ = nullptr;
};

const sen::StructType& getValidStructType()
{
  static std::vector<sen::StructField> fields = {{"field", " desc", sen::Int32Type::get()}};
  static sen::StructSpec spec = {"Struct", "ns.Struct", "desc", fields, {}};
  static auto type = sen::StructType::make(spec);
  return *type;
}

const sen::SequenceType& getValidSequenceType()
{
  static sen::SequenceSpec spec = {
    "TestSequence1", "ns.TestSequence1", "bounded sequence of strings", sen::StringType::get(), 10U};
  static auto type = sen::SequenceType::make(spec);
  return *type;
}

const sen::EnumType& getValidEnumType()
{
  const std::vector<sen::Enumerator> validEnums = {{"one", 1, "first"}, {"two", 2, "second"}, {"three", 3, "third"}};

  const sen::EnumSpec spec = {"TestEnum", "ns.TestEnum", "Valid enum", validEnums, sen::UInt8Type::get()};

  static auto type = sen::EnumType::make(spec);
  return *type;
}

const sen::VariantType& getValidVariantType()
{
  static std::vector<sen::VariantField> fields = {{1, " desc", sen::Int32Type::get()}};
  static sen::VariantSpec spec = {"Variant", "ns.Variant", "desc", fields};
  static auto type = sen::VariantType::make(spec);
  return *type;
}

const sen::QuantityType& getValidQuantityType()
{
  static auto unit = sen::Unit::make(sen::UnitSpec {sen::UnitCategory::length, "meter", "meters", "m", 1.0, 0.0, 0.0});
  static sen::QuantitySpec spec {"MyQuantity", "MyQuantity", "desc", sen::Float32Type::get(), unit.get(), 0.0, 1.0};

  static auto type = sen::QuantityType::make(spec);
  return *type;
}

const sen::AliasType& getValidAliasType()
{
  static sen::AliasSpec spec = {"Name", "ns.Name", "desc", sen::Float32Type::get()};
  static auto type = sen::AliasType::make(spec);
  return *type;
}

const sen::OptionalType& getValidOptionalType()
{
  static sen::OptionalSpec spec = {"MyOptional", "MyOptional", "desc", sen::Float64Type::get()};
  static auto type = sen::OptionalType::make(spec);
  return *type;
}

template <typename T, typename V>
void checkValidOrInvalidVisit(const T& type)
{
  using Checker = TypeChecker<V>;

  if (std::is_same_v<T, V>)
  {
    Checker::checkValid(type);
  }
  else
  {
    Checker::checkInvalid(type);
  }
}

template <typename T>
void checkSpecificVisit(const T& type)
{
  checkValidOrInvalidVisit<T, sen::UInt8Type>(type);
  checkValidOrInvalidVisit<T, sen::Int16Type>(type);
  checkValidOrInvalidVisit<T, sen::UInt16Type>(type);
  checkValidOrInvalidVisit<T, sen::Int32Type>(type);
  checkValidOrInvalidVisit<T, sen::UInt32Type>(type);
  checkValidOrInvalidVisit<T, sen::Int64Type>(type);
  checkValidOrInvalidVisit<T, sen::UInt64Type>(type);
  checkValidOrInvalidVisit<T, sen::Float32Type>(type);
  checkValidOrInvalidVisit<T, sen::Float64Type>(type);
  checkValidOrInvalidVisit<T, sen::StringType>(type);
  checkValidOrInvalidVisit<T, sen::BoolType>(type);
  checkValidOrInvalidVisit<T, sen::StructType>(type);
  checkValidOrInvalidVisit<T, sen::EnumType>(type);
  checkValidOrInvalidVisit<T, sen::VariantType>(type);
  checkValidOrInvalidVisit<T, sen::SequenceType>(type);
  checkValidOrInvalidVisit<T, sen::QuantityType>(type);
  checkValidOrInvalidVisit<T, sen::AliasType>(type);
  checkValidOrInvalidVisit<T, sen::OptionalType>(type);
}

template <typename T>
void checkNativeSpecificVisit()
{
  checkSpecificVisit<T>(*T::get());
}

}  // namespace

/// @test
/// Checks all type visitors
/// @requirements(SEN-575)
TEST(TypeVisitor, allTypes)
{
  using Checker = TypeChecker<sen::Type>;

  Checker::checkValid(*sen::UInt8Type::get());
  Checker::checkValid(*sen::Int16Type::get());
  Checker::checkValid(*sen::UInt16Type::get());
  Checker::checkValid(*sen::Int32Type::get());
  Checker::checkValid(*sen::UInt32Type::get());
  Checker::checkValid(*sen::Int64Type::get());
  Checker::checkValid(*sen::UInt64Type::get());
  Checker::checkValid(*sen::Float32Type::get());
  Checker::checkValid(*sen::Float64Type::get());
  Checker::checkValid(*sen::StringType::get());
  Checker::checkValid(*sen::BoolType::get());

  Checker::checkValid(getValidStructType());
  Checker::checkValid(getValidSequenceType());
  Checker::checkValid(getValidEnumType());
  Checker::checkValid(getValidVariantType());
  Checker::checkValid(getValidQuantityType());
  Checker::checkValid(getValidAliasType());
  Checker::checkValid(getValidOptionalType());
}

/// @test
/// Checks native type visitors
/// @requirements(SEN-575)
TEST(TypeVisitor, nativeTypes)
{
  using Checker = TypeChecker<sen::NativeType>;

  Checker::checkValid(*sen::UInt8Type::get());
  Checker::checkValid(*sen::Int16Type::get());
  Checker::checkValid(*sen::UInt16Type::get());
  Checker::checkValid(*sen::Int32Type::get());
  Checker::checkValid(*sen::UInt32Type::get());
  Checker::checkValid(*sen::Int64Type::get());
  Checker::checkValid(*sen::UInt64Type::get());
  Checker::checkValid(*sen::Float32Type::get());
  Checker::checkValid(*sen::Float64Type::get());
  Checker::checkValid(*sen::StringType::get());
  Checker::checkValid(*sen::BoolType::get());
  Checker::checkInvalid(getValidStructType());
  Checker::checkInvalid(getValidSequenceType());
  Checker::checkInvalid(getValidEnumType());
  Checker::checkInvalid(getValidVariantType());
  Checker::checkInvalid(getValidQuantityType());
  Checker::checkInvalid(getValidAliasType());
  Checker::checkInvalid(getValidOptionalType());
}

/// @test
/// Checks numeric type visitors
/// @requirements(SEN-575)
TEST(TypeVisitor, numericTypes)
{
  using Checker = TypeChecker<sen::NumericType>;

  Checker::checkValid(*sen::UInt8Type::get());
  Checker::checkValid(*sen::Int16Type::get());
  Checker::checkValid(*sen::UInt16Type::get());
  Checker::checkValid(*sen::Int32Type::get());
  Checker::checkValid(*sen::UInt32Type::get());
  Checker::checkValid(*sen::Int64Type::get());
  Checker::checkValid(*sen::UInt64Type::get());
  Checker::checkValid(*sen::Float32Type::get());
  Checker::checkValid(*sen::Float64Type::get());

  Checker::checkInvalid(*sen::StringType::get());
  Checker::checkInvalid(*sen::BoolType::get());
  Checker::checkInvalid(getValidStructType());
  Checker::checkInvalid(getValidSequenceType());
  Checker::checkInvalid(getValidEnumType());
  Checker::checkInvalid(getValidVariantType());
  Checker::checkInvalid(getValidQuantityType());
  Checker::checkInvalid(getValidAliasType());
  Checker::checkInvalid(getValidOptionalType());
}

/// @test
/// Checks integral type visitors
/// @requirements(SEN-575)
TEST(TypeVisitor, integralTypes)
{
  using Checker = TypeChecker<sen::IntegralType>;

  Checker::checkValid(*sen::UInt8Type::get());
  Checker::checkValid(*sen::Int16Type::get());
  Checker::checkValid(*sen::UInt16Type::get());
  Checker::checkValid(*sen::Int32Type::get());
  Checker::checkValid(*sen::UInt32Type::get());
  Checker::checkValid(*sen::Int64Type::get());
  Checker::checkValid(*sen::UInt64Type::get());

  Checker::checkInvalid(*sen::Float32Type::get());
  Checker::checkInvalid(*sen::Float64Type::get());
  Checker::checkInvalid(*sen::StringType::get());
  Checker::checkInvalid(*sen::BoolType::get());
  Checker::checkInvalid(getValidStructType());
  Checker::checkInvalid(getValidSequenceType());
  Checker::checkInvalid(getValidEnumType());
  Checker::checkInvalid(getValidVariantType());
  Checker::checkInvalid(getValidQuantityType());
  Checker::checkInvalid(getValidAliasType());
  Checker::checkInvalid(getValidOptionalType());
}

/// @test
/// Checks real type visitors
/// @requirements(SEN-575)
TEST(TypeVisitor, realTypes)
{
  using Checker = TypeChecker<sen::RealType>;

  Checker::checkInvalid(*sen::UInt8Type::get());
  Checker::checkInvalid(*sen::Int16Type::get());
  Checker::checkInvalid(*sen::UInt16Type::get());
  Checker::checkInvalid(*sen::Int32Type::get());
  Checker::checkInvalid(*sen::UInt32Type::get());
  Checker::checkInvalid(*sen::Int64Type::get());
  Checker::checkInvalid(*sen::UInt64Type::get());

  Checker::checkValid(*sen::Float32Type::get());
  Checker::checkValid(*sen::Float64Type::get());

  Checker::checkInvalid(*sen::StringType::get());
  Checker::checkInvalid(*sen::BoolType::get());
  Checker::checkInvalid(getValidStructType());
  Checker::checkInvalid(getValidSequenceType());
  Checker::checkInvalid(getValidEnumType());
  Checker::checkInvalid(getValidVariantType());
  Checker::checkInvalid(getValidQuantityType());
  Checker::checkInvalid(getValidAliasType());
  Checker::checkInvalid(getValidOptionalType());
}

/// @test
/// Checks specific type visitors
/// @requirements(SEN-575)
TEST(TypeVisitor, specificTypes)
{
  checkNativeSpecificVisit<sen::UInt8Type>();
  checkNativeSpecificVisit<sen::Int16Type>();
  checkNativeSpecificVisit<sen::UInt16Type>();
  checkNativeSpecificVisit<sen::Int32Type>();
  checkNativeSpecificVisit<sen::UInt32Type>();
  checkNativeSpecificVisit<sen::Int64Type>();
  checkNativeSpecificVisit<sen::UInt64Type>();
  checkNativeSpecificVisit<sen::Float32Type>();
  checkNativeSpecificVisit<sen::Float64Type>();
  checkNativeSpecificVisit<sen::StringType>();
  checkNativeSpecificVisit<sen::BoolType>();

  checkSpecificVisit<sen::StructType>(getValidStructType());
  checkSpecificVisit<sen::SequenceType>(getValidSequenceType());
  checkSpecificVisit<sen::EnumType>(getValidEnumType());
  checkSpecificVisit<sen::VariantType>(getValidVariantType());
  checkSpecificVisit<sen::QuantityType>(getValidQuantityType());
  checkSpecificVisit<sen::AliasType>(getValidAliasType());
  checkSpecificVisit<sen::OptionalType>(getValidOptionalType());
}

/// @test
/// Checks custom type visitors
/// @requirements(SEN-575)
TEST(TypeVisitor, customTypes)
{
  using Checker = TypeChecker<sen::CustomType>;
  Checker::checkValid(getValidStructType());
  Checker::checkValid(getValidSequenceType());
  Checker::checkValid(getValidEnumType());
  Checker::checkValid(getValidVariantType());
  Checker::checkValid(getValidQuantityType());
  Checker::checkValid(getValidAliasType());
  Checker::checkValid(getValidOptionalType());
}

/// @test
/// Checks conversion between different types
/// @requirements(SEN-575)
TEST(TypeMatch, canConvert)
{
  // from any type to string
  {
    EXPECT_TRUE(canConvert(getValidOptionalType(), *sen::StringType::get()).isOk());
    EXPECT_TRUE(canConvert(getValidAliasType(), *sen::StringType::get()).isOk());
    EXPECT_TRUE(canConvert(getValidEnumType(), *sen::StringType::get()).isOk());
    EXPECT_TRUE(canConvert(getValidQuantityType(), *sen::StringType::get()).isOk());
    EXPECT_TRUE(canConvert(getValidSequenceType(), *sen::StringType::get()).isOk());
    EXPECT_TRUE(canConvert(getValidStructType(), *sen::StringType::get()).isOk());
    EXPECT_TRUE(canConvert(getValidVariantType(), *sen::StringType::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::UInt8Type::get(), *sen::StringType::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::UInt16Type::get(), *sen::StringType::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::Int16Type::get(), *sen::StringType::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::UInt32Type::get(), *sen::StringType::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::Int32Type::get(), *sen::StringType::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::UInt64Type::get(), *sen::StringType::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::Int64Type::get(), *sen::StringType::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::Float32Type::get(), *sen::StringType::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::Float64Type::get(), *sen::StringType::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::TimestampType::get(), *sen::StringType::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::DurationType::get(), *sen::StringType::get()).isOk());
  }

  // from type to same type
  {
    EXPECT_TRUE(canConvert(getValidOptionalType(), getValidOptionalType()).isOk());
    EXPECT_TRUE(canConvert(getValidAliasType(), getValidAliasType()).isOk());
    EXPECT_TRUE(canConvert(getValidEnumType(), getValidEnumType()).isOk());
    EXPECT_TRUE(canConvert(getValidQuantityType(), getValidQuantityType()).isOk());
    EXPECT_TRUE(canConvert(getValidSequenceType(), getValidSequenceType()).isOk());
    EXPECT_TRUE(canConvert(getValidStructType(), getValidStructType()).isOk());
    EXPECT_TRUE(canConvert(getValidVariantType(), getValidVariantType()).isOk());
    EXPECT_TRUE(canConvert(*sen::UInt8Type::get(), *sen::UInt8Type::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::UInt16Type::get(), *sen::UInt16Type::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::Int16Type::get(), *sen::Int16Type::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::UInt32Type::get(), *sen::UInt32Type::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::Int32Type::get(), *sen::Int32Type::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::UInt64Type::get(), *sen::UInt64Type::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::Int64Type::get(), *sen::Int64Type::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::Float32Type::get(), *sen::Float32Type::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::Float64Type::get(), *sen::Float64Type::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::TimestampType::get(), *sen::TimestampType::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::DurationType::get(), *sen::DurationType::get()).isOk());
  }

  // valid conversion from typeX to typeY
  {
    EXPECT_TRUE(canConvert(getValidOptionalType(), *sen::UInt64Type::get()).isOk());
    EXPECT_TRUE(canConvert(getValidAliasType(), *sen::UInt8Type::get()).isOk());
    EXPECT_TRUE(canConvert(getValidEnumType(), *sen::Float32Type::get()).isOk());
    EXPECT_TRUE(canConvert(getValidQuantityType(), *sen::Float64Type::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::UInt8Type::get(), *sen::UInt32Type::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::UInt16Type::get(), *sen::Int64Type::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::Int16Type::get(), *sen::UInt8Type::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::UInt32Type::get(), *sen::Int32Type::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::Int32Type::get(), *sen::UInt32Type::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::TimestampType::get(), *sen::DurationType::get()).isOk());
    EXPECT_TRUE(canConvert(*sen::DurationType::get(), *sen::TimestampType::get()).isOk());
  }

  // invalid conversion from typeX to typeY
  {
    EXPECT_FALSE(canConvert(getValidOptionalType(), getValidStructType()).isOk());
    EXPECT_FALSE(canConvert(getValidAliasType(), getValidVariantType()).isOk());
    EXPECT_FALSE(canConvert(getValidEnumType(), getValidOptionalType()).isOk());
    EXPECT_FALSE(canConvert(getValidQuantityType(), *sen::DurationType::get()).isOk());
    EXPECT_FALSE(canConvert(getValidVariantType(), getValidQuantityType()).isOk());
    EXPECT_FALSE(canConvert(getValidVariantType(), *sen::Int16Type::get()).isOk());
    EXPECT_FALSE(canConvert(getValidStructType(), *sen::Int32Type::get()).isOk());
  }
}
