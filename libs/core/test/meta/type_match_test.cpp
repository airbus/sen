// === type_match_test.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/class_type.h"
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
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

using sen::AliasSpec;
using sen::AliasType;
using sen::ClassSpec;
using sen::ClassType;
using sen::Enumerator;
using sen::EnumSpec;
using sen::EnumType;
using sen::OptionalSpec;
using sen::OptionalType;
using sen::QuantitySpec;
using sen::QuantityType;
using sen::SequenceSpec;
using sen::SequenceType;
using sen::StructField;
using sen::StructSpec;
using sen::StructType;
using sen::TypeMatchProblem;
using sen::UnitCategory;
using sen::UnitSpec;
using sen::VariantField;
using sen::VariantSpec;
using sen::VariantType;

namespace
{

const StructType& validStructType()
{
  static std::vector<StructField> fields = {{"field", " desc", sen::Int32Type::get()}};
  static StructSpec spec = {"Struct", "ns.Struct", "desc", fields, {}};
  static auto type = StructType::make(spec);
  return *type;
}

const SequenceType& validSequenceType()
{
  static SequenceSpec spec = {
    "TestSequence1", "ns.TestSequence1", "bounded sequence of strings", sen::StringType::get(), 10U};
  static auto type = SequenceType::make(spec);
  return *type;
}

const EnumType& validEnumType()
{
  const std::vector<Enumerator> enums = {{"one", 1, "first"}, {"two", 2, "second"}, {"three", 3, "third"}};
  const EnumSpec spec = {"TestEnum", "ns.TestEnum", "Valid enum", enums, sen::UInt8Type::get()};
  static auto type = EnumType::make(spec);
  return *type;
}

const VariantType& validVariantType()
{
  static std::vector<VariantField> fields = {{1, " desc", sen::Int32Type::get()}};
  static VariantSpec spec = {"Variant", "ns.Variant", "desc", fields};
  static auto type = VariantType::make(spec);
  return *type;
}

const QuantityType& validQuantityType()
{
  static auto unit = sen::Unit::make(UnitSpec {UnitCategory::length, "meter", "meters", "m", 1.0, 0.0, 0.0});
  static QuantitySpec spec {"MyQuantity", "MyQuantity", "desc", sen::Float32Type::get(), unit.get(), 0.0, 1.0};
  static auto type = QuantityType::make(spec);
  return *type;
}

const AliasType& validAliasType()
{
  static AliasSpec spec = {"Name", "ns.Name", "desc", sen::Float32Type::get()};
  static auto type = AliasType::make(spec);
  return *type;
}

const OptionalType& validOptionalType()
{
  static OptionalSpec spec = {"MyOptional", "MyOptional", "desc", sen::Float64Type::get()};
  static auto type = OptionalType::make(spec);
  return *type;
}

// Checks whether canConvert(from, to) produces the expected result and contains the expected problems
void checkExpectedIssues(const sen::Type& from,
                         const sen::Type& to,
                         bool expectOk,
                         std::vector<TypeMatchProblem> problems = {})
{
  auto match = sen::canConvert(from, to);

  const auto containsProblem = [](const auto& issues, TypeMatchProblem prob)
  {
    for (const auto& issue: issues)
    {
      if (issue.problem == prob)
      {
        return true;
      }
    }
    return false;
  };

  if (expectOk)
  {
    EXPECT_TRUE(match.isOk()) << "Expected canConvert(" << from.getName() << " -> " << to.getName() << ") to succeed";
    if (!problems.empty() && match.isOk())
    {
      const auto& issues = match.getValue().issues;
      for (const auto& prob: problems)
      {
        EXPECT_TRUE(containsProblem(issues, prob))
          << "Expected problem " << static_cast<int>(prob) << " not found in Ok issues";
      }
    }
  }
  else
  {
    EXPECT_FALSE(match.isOk()) << "Expected canConvert(" << from.getName() << " -> " << to.getName() << ") to fail";
    if (!match.isOk())
    {
      const auto& issues = match.getError().issues;
      for (const auto& prob: problems)
      {
        EXPECT_TRUE(containsProblem(issues, prob))
          << "Expected problem " << static_cast<int>(prob) << " not found in Error issues";
      }
    }
  }
}

class MockNumericType final: public sen::NumericType
{
public:
  MockNumericType(): sen::NumericType(123) {}
  void accept(sen::FullTypeVisitor& visitor) const override
  {
    visitor.apply(static_cast<const sen::NumericType&>(*this));
  }
  [[nodiscard]] std::string_view getName() const noexcept override { return "MockNum"; }
  [[nodiscard]] std::string_view getDescription() const noexcept override { return ""; }
  [[nodiscard]] bool equals([[maybe_unused]] const sen::Type& other) const noexcept override { return false; }
  [[nodiscard]] bool isSigned() const noexcept override { return false; }
  [[nodiscard]] bool hasInfinity() const noexcept override { return false; }
  [[nodiscard]] bool isIEC559() const noexcept override { return false; }
  [[nodiscard]] std::size_t getByteSize() const noexcept override { return 4; }
};

class MockCustomType final: public sen::CustomType
{
public:
  MockCustomType(): sen::CustomType(123) {}
  void accept(sen::FullTypeVisitor& visitor) const override
  {
    visitor.apply(static_cast<const sen::CustomType&>(*this));
  }
  [[nodiscard]] std::string_view getName() const noexcept override { return "MockCustom"; }
  [[nodiscard]] std::string_view getQualifiedName() const noexcept override { return "ns.MockCustom"; }
  [[nodiscard]] std::string_view getDescription() const noexcept override { return ""; }
  [[nodiscard]] bool equals([[maybe_unused]] const sen::Type& other) const noexcept override { return false; }
  [[nodiscard]] bool isBounded() const noexcept override { return true; }
};

}  // namespace

/// @test
/// Checks native-to-native and native-to-string type match conversions
/// @requirements(SEN-355)
TEST(TypeMatch, NativeConversions)
{
  // Any type to String
  EXPECT_TRUE(sen::canConvert(validOptionalType(), *sen::StringType::get()).isOk());
  EXPECT_TRUE(sen::canConvert(*sen::UInt32Type::get(), *sen::StringType::get()).isOk());

  const std::vector<const sen::Type*> allNatives = {sen::BoolType::get().type(),
                                                    sen::UInt8Type::get().type(),
                                                    sen::Int16Type::get().type(),
                                                    sen::UInt16Type::get().type(),
                                                    sen::Int32Type::get().type(),
                                                    sen::UInt32Type::get().type(),
                                                    sen::Int64Type::get().type(),
                                                    sen::UInt64Type::get().type(),
                                                    sen::Float32Type::get().type(),
                                                    sen::Float64Type::get().type(),
                                                    sen::VoidType::get().type()};

  for (const auto* from: allNatives)
  {
    for (const auto* to: allNatives)
    {
      if ((from->isVoidType() || to->isVoidType()) && from != to)
      {
        checkExpectedIssues(*from, *to, false, {TypeMatchProblem::incompatible});
      }
      else
      {
        checkExpectedIssues(*from, *to, true);
      }
    }
    // from native to string
    checkExpectedIssues(*from, *sen::StringType::get(), true);

    // from string to native
    checkExpectedIssues(*sen::StringType::get(), *from, false, {TypeMatchProblem::incompatible});
  }

  // structurally identical conversion
  checkExpectedIssues(*sen::TimestampType::get(), *sen::DurationType::get(), true);

  // non-native to native
  checkExpectedIssues(validStructType(), *sen::Int32Type::get(), false, {TypeMatchProblem::incompatible});
}

/// @test
/// Checks struct-to-struct type match conversions including field and parent mismatches
/// @requirements(SEN-355)
TEST(TypeMatch, StructConversions)
{
  const auto& struct1 = validStructType();
  // valid conversion
  checkExpectedIssues(struct1, struct1, true);

  // incompatible (to non struct)
  checkExpectedIssues(struct1, validSequenceType(), false, {TypeMatchProblem::incompatible});

  // different field count (missing problem)
  {
    std::vector<StructField> f1 = {{"f", "desc", sen::Int32Type::get()}};
    std::vector<StructField> f2 = {{"f", "desc", sen::Int32Type::get()}, {"f2", "desc", sen::Int32Type::get()}};
    auto s1 = StructType::make({"S1", "ns.s1", "", f1, {}});
    auto s2 = StructType::make({"S2", "ns.s2", "", f2, {}});
    checkExpectedIssues(*s2, *s1, false, {TypeMatchProblem::missing});
  }

  // field type incompatible
  {
    std::vector<StructField> f1 = {{"f", "desc", sen::Int32Type::get()}};
    std::vector<StructField> f2 = {{"f", "desc", sen::StringType::get()}};
    auto s1 = StructType::make({"S1", "ns.s1", "", f2, {}});
    auto s2 = StructType::make({"S2", "ns.s2", "", f1, {}});
    checkExpectedIssues(*s1, *s2, false, {TypeMatchProblem::incompatible});
  }

  // with parents
  {
    std::vector<StructField> f1 = {{"f", "desc", sen::Int32Type::get()}};
    auto parent1 = StructType::make({"P1", "ns.P1", "", {{"p1f", "desc", sen::Int32Type::get()}}, {}});
    auto parent2 = StructType::make({"P2", "ns.P2", "", {{"p2f", "desc", sen::Int32Type::get()}}, {}});
    auto s1 = StructType::make({"S1", "ns.S1", "", f1, parent1});
    auto sNoParent = StructType::make({"SNoParent", "ns.SNoParent", "", f1, {}});

    // parent but rhs no parent
    checkExpectedIssues(*s1, *sNoParent, false, {TypeMatchProblem::missing});

    // parent exists on both and is the same
    auto s2 = StructType::make({"S2", "ns.s2", "", f1, parent1});
    checkExpectedIssues(*s1, *s2, true);

    // incompatible parent
    auto parent3 = StructType::make({"P3", "ns.P3", "", {{"f2", "", sen::StringType::get()}}, {}});
    auto s3 = StructType::make({"S3", "ns.S3", "", f1, parent3});
    checkExpectedIssues(*s3, *s1, false, {TypeMatchProblem::incompatible});
  }
}

/// @test
/// Checks variant-to-variant type match conversions
/// @requirements(SEN-355)
TEST(TypeMatch, VariantConversions)
{
  const auto& var1 = validVariantType();
  // valid conversion
  checkExpectedIssues(var1, var1, true);

  // incompatible (to non variant)
  checkExpectedIssues(var1, validSequenceType(), false, {TypeMatchProblem::incompatible});

  // missing fields
  {
    std::vector<VariantField> f1 = {{1, "desc", sen::Int32Type::get()}, {2, "desc", sen::Float32Type::get()}};
    std::vector<VariantField> f2 = {{1, "desc", sen::Int32Type::get()}};
    auto v1 = VariantType::make({"V1", "ns.v1", "", f1});
    auto v2 = VariantType::make({"V2", "ns.v2", "", f2});

    checkExpectedIssues(*v1, *v2, false, {TypeMatchProblem::missing});
    // extra fields in target is OK
    checkExpectedIssues(*v2, *v1, true);
  }
}

/// @test
/// Checks sequence-to-sequence type match conversions including bounds and element compatibility
/// @requirements(SEN-355)
TEST(TypeMatch, SequenceConversions)
{
  const auto& seq1 = validSequenceType();
  checkExpectedIssues(seq1, seq1, true);
  checkExpectedIssues(seq1, validStructType(), false, {TypeMatchProblem::incompatible});

  // Check bounds constraints compatibility (bound vs unbound)
  auto seqBounded5 = SequenceType::make({"S5", "ns.s5", "", sen::StringType::get(), 5U});
  auto seqBounded15 = SequenceType::make({"S15", "ns.s15", "", sen::StringType::get(), 15U});
  auto seqUnbounded = SequenceType::make({"Su", "ns.su", "", sen::StringType::get(), std::nullopt});

  // different sizes bounded
  checkExpectedIssues(*seqBounded5, *seqBounded15, true);
  // unbounded source, bounded target
  checkExpectedIssues(*seqUnbounded, *seqBounded15, true);

  // bigger bounded source to smaller target
  checkExpectedIssues(*seqBounded15, *seqBounded5, true);
  // unbounded source to smaller bounded target
  checkExpectedIssues(*seqUnbounded, *seqBounded5, true);

  // element incompatibility
  auto seqInt = SequenceType::make({"Si", "ns.si", "", sen::Int32Type::get(), 10U});
  checkExpectedIssues(seq1, *seqInt, false, {TypeMatchProblem::incompatible});
}

/// @test
/// Checks enum-to-enum type match conversions including storage, name and key mismatches
/// @requirements(SEN-355)
TEST(TypeMatch, EnumConversions)
{
  const auto& enum1 = validEnumType();
  checkExpectedIssues(enum1, enum1, true);

  // enum storage check
  checkExpectedIssues(enum1, *sen::UInt8Type::get(), true);
  checkExpectedIssues(enum1, validStructType(), false, {TypeMatchProblem::incompatible});

  std::vector<Enumerator> enums1 = {{"one", 1, ""}};
  auto e1 = EnumType::make({"E1", "ns.e1", "", enums1, sen::UInt8Type::get()});

  // different names for same key (minor issue)
  std::vector<Enumerator> enums2 = {{"uno", 1, ""}};
  auto e2 = EnumType::make({"E2", "ns.e2", "", enums2, sen::UInt8Type::get()});
  checkExpectedIssues(*e1, *e2, true);

  // missing key
  std::vector<Enumerator> enums3 = {{"two", 2, ""}};
  auto e3 = EnumType::make({"E3", "ns.e3", "", enums3, sen::UInt8Type::get()});
  checkExpectedIssues(*e1, *e3, false, {TypeMatchProblem::missing});
}

/// @test
/// Checks quantity-to-quantity type match conversions
/// @requirements(SEN-355)
TEST(TypeMatch, QuantityConversions)
{
  const auto& q1 = validQuantityType();
  checkExpectedIssues(q1, q1, true);

  auto unitL = sen::Unit::make(UnitSpec {UnitCategory::length, "l", "l", "l", 1.0, 0.0, 0.0});
  auto unitM = sen::Unit::make(UnitSpec {UnitCategory::mass, "m", "m", "m", 1.0, 0.0, 0.0});

  auto qLength =
    QuantityType::make({"Ql", "ns.ql", "", sen::Float32Type::get(), unitL.get(), std::nullopt, std::nullopt});
  auto qMass =
    QuantityType::make({"Qm", "ns.qm", "", sen::Float32Type::get(), unitM.get(), std::nullopt, std::nullopt});
  auto qNoUnit =
    QuantityType::make({"Qnu", "ns.qnu", "", sen::Float32Type::get(), std::nullopt, std::nullopt, std::nullopt});

  // mismatching units
  checkExpectedIssues(*qLength, *qMass, false, {TypeMatchProblem::incompatible});

  // missing unit vs unit
  checkExpectedIssues(*qLength, *qNoUnit, false, {TypeMatchProblem::incompatible});
  checkExpectedIssues(*qNoUnit, *qLength, false, {TypeMatchProblem::incompatible});

  // bounds checking
  auto qRestricted = QuantityType::make({"Qr", "ns.qr", "", sen::Float32Type::get(), unitL.get(), 10.0, 20.0});
  checkExpectedIssues(*qLength, *qRestricted, true);

  auto qLowRange = QuantityType::make({"Qlow", "ns.qlow", "", sen::Float32Type::get(), unitL.get(), 0.0, 15.0});
  auto qHighRange = QuantityType::make({"Qhigh", "ns.qhigh", "", sen::Float32Type::get(), unitL.get(), 16.0, 30.0});
  checkExpectedIssues(*qLowRange, *qHighRange, true);

  auto qSmallRange = QuantityType::make({"Qsmall", "ns.qsmall", "", sen::Float32Type::get(), unitL.get(), 0.0, 5.0});
  auto qLargeRange = QuantityType::make({"Qlarge", "ns.qlarge", "", sen::Float32Type::get(), unitL.get(), 50.0, 60.0});
  checkExpectedIssues(*qLargeRange, *qLowRange, true);
  checkExpectedIssues(*qSmallRange, *qHighRange, true);

  // fallback to numeric conversion if not quantity type
  checkExpectedIssues(*qLength, *sen::Float64Type::get(), true);
}

/// @test
/// Checks alias, optional, void, and class type match conversions
/// @requirements(SEN-355)
TEST(TypeMatch, OtherConversions)
{
  // alias
  const auto& alias = validAliasType();
  checkExpectedIssues(alias, *sen::Float64Type::get(), true);

  // optional
  const auto& opt = validOptionalType();
  checkExpectedIssues(opt, *sen::Float64Type::get(), true);

  // void
  auto voidT = sen::VoidType::get();
  checkExpectedIssues(*voidT, *voidT, true);

  // classes
  const ClassSpec cs1 {"C1", "ns.c1", "", {}, {}, {}, std::nullopt, {}, false, {}, {}};
  const ClassSpec cs2 {"C2", "ns.c2", "", {}, {}, {}, std::nullopt, {}, false, {}, {}};
  auto class1 = ClassType::make(cs1);
  auto class2 = ClassType::make(cs2);

  const ClassSpec cs3 {"C3", "ns.c3", "", {}, {}, {}, std::nullopt, {class1}, false, {}, {}};
  auto class3 = ClassType::make(cs3);

  checkExpectedIssues(*class1, *class1, true);
  checkExpectedIssues(*class1, *class2, false, {TypeMatchProblem::incompatible});
  checkExpectedIssues(*class3, *class1, true);
  checkExpectedIssues(*class1, *class3, false, {TypeMatchProblem::incompatible});

  // incompatible class to native (early return in check())
  checkExpectedIssues(*class1, *sen::Int32Type::get(), false, {TypeMatchProblem::incompatible});

  // blocks throwing logic_error on unknown types
  EXPECT_THROW(static_cast<void>(sen::canConvert(MockNumericType {}, *sen::Int32Type::get())), std::logic_error);
  EXPECT_THROW(static_cast<void>(sen::canConvert(MockCustomType {}, *sen::Int32Type::get())), std::logic_error);
}
