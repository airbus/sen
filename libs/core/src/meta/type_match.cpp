// === type_match.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/type_match.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/result.h"
#include "sen/core/base/span.h"
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
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/unit.h"
#include "sen/core/meta/variant_type.h"

// std
#include <cstddef>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace sen
{

namespace
{

//-------------------------------------------------------------------------------------//
// Helpers
//-------------------------------------------------------------------------------------//

/// The severity of a conversion issue
enum class IssueType
{
  major,
  minor
};

/// Retuns an issue indicating a valid conversion
TypeMatchIssue validConversion(const Type& from, const Type& to)
{
  std::string msg = "converting from ";
  msg += from.getName();
  msg += "to ";
  msg += to.getName();

  return {TypeMatchProblem::compatible, std::move(msg)};
}

/// Retuns an issue indicating an invalid conversion
TypeMatchIssue invalidConversion(const Type& from, const Type& to)
{
  std::string msg = "invalid conversion from ";
  msg += from.getName();
  msg += "to ";
  msg += to.getName();

  return {TypeMatchProblem::incompatible, std::move(msg)};
}

// Collects type conversion issues
class IssueCollector
{
  SEN_NOCOPY_NOMOVE(IssueCollector)

public:
  IssueCollector() = default;
  ~IssueCollector() = default;

public:
  /// The resulting analysis
  [[nodiscard]] TypeMatch getResult() const noexcept
  {
    if (majorIssue_)
    {
      return Err(TypesDontMatch {issues_});
    }
    return Ok(TypesMatch {issues_});
  }

  [[nodiscard]] const std::vector<TypeMatchIssue>& getIssues() const noexcept { return issues_; }

  [[nodiscard]] bool majorIssueDetected() const noexcept { return majorIssue_; }

protected:
  /// Adds a conversion issue
  void addIssue(TypeMatchProblem problem, std::string&& msg, IssueType issueType)
  {
    issues_.push_back({problem, std::move(msg)});
    if (issueType == IssueType::major)
    {
      majorIssue_ = true;
    }
  }

  /// Adds a conversion issue
  void addIssue(TypeMatchIssue&& issue, IssueType issueType)
  {
    issues_.push_back(std::move(issue));
    if (issueType == IssueType::major)
    {
      majorIssue_ = true;
    }
  }

  /// Accepts a type conversion, adding a non-critical issue
  void acceptConversion(const Type& from, const Type& to) { addIssue(validConversion(from, to), IssueType::minor); }

  /// Rejects a type conversion.
  void rejectConversion(const Type& from, const Type& to) { addIssue(invalidConversion(from, to), IssueType::major); }

  void addIssues(const std::vector<TypeMatchIssue>& otherIssues, bool otherMajorIssue)
  {
    issues_.insert(issues_.end(), otherIssues.begin(), otherIssues.end());
    majorIssue_ = majorIssue_ || otherMajorIssue;
  }

  /// Adds any issues.
  /// Returns true if the conversion is acceptable.
  bool addIssuesForConversion(const Type& from, const Type& to)
  {
    auto conversion = canConvert(from, to);
    if (conversion.isOk())
    {
      addIssues(conversion.getValue().issues, false);
      return true;
    }

    addIssues(conversion.getError().issues, true);
    return false;
  }

private:
  std::vector<TypeMatchIssue> issues_;
  bool majorIssue_ = false;
};

//-------------------------------------------------------------------------------------//
// Checks for native types
//-------------------------------------------------------------------------------------//

// Checks conversions from native types
template <typename From>
class ToNativeTypeChecker final: protected TypeVisitor, public IssueCollector
{
  SEN_NOCOPY_NOMOVE(ToNativeTypeChecker)

public:
  using MetaType = NativeType;

  ToNativeTypeChecker() = default;
  ~ToNativeTypeChecker() override = default;

public:
  void check(const NativeType& from, const Type& to)
  {
    std::ignore = from;
    to.accept(*this);
  }

protected:
  void apply(const Type& type) override
  {
    rejectConversion(*From::get(), type);  // reject conversions to non-native types
  }

  void apply(const BoolType& type) override { checkTo<BoolType>(type); }

  void apply(const UInt8Type& type) override { checkTo<UInt8Type>(type); }

  void apply(const Int16Type& type) override { checkTo<Int16Type>(type); }

  void apply(const UInt16Type& type) override { checkTo<UInt16Type>(type); }

  void apply(const Int32Type& type) override { checkTo<Int32Type>(type); }

  void apply(const UInt32Type& type) override { checkTo<UInt32Type>(type); }

  void apply(const Int64Type& type) override { checkTo<Int64Type>(type); }

  void apply(const UInt64Type& type) override { checkTo<UInt64Type>(type); }

  void apply(const Float32Type& type) override { checkTo<Float32Type>(type); }

  void apply(const Float64Type& type) override { checkTo<Float64Type>(type); }

  void apply(const StringType& type) override
  {
    acceptConversion(*From::get(), type);  // all types can be converted to strings
  }

private:
  template <typename To>
  void checkTo(const Type& t)
  {
    using NativeFrom = typename From::NativeType;
    using NativeTo = typename To::NativeType;

    if (std::is_same_v<NativeFrom, NativeTo>)
    {
      return;
    }

    if (std::is_convertible_v<NativeFrom, NativeTo>)
    {
      acceptConversion(*From::get(), t);
    }
    else
    {
      rejectConversion(*From::get(), t);
    }
  }
};

//-------------------------------------------------------------------------------------//
// Structs
//-------------------------------------------------------------------------------------//

class StructChecker: public IssueCollector
{
  SEN_NOCOPY_NOMOVE(StructChecker)

public:
  StructChecker() = default;
  ~StructChecker() = default;

  using MetaType = StructType;

public:
  void check(const StructType& from, const Type& to)
  {
    if (!to.isStructType())
    {
      rejectConversion(from, to);
      return;
    }

    checkStructs(from, *to.asStructType());
  }

private:
  void checkStructs(const StructType& lhs, const StructType& rhs)
  {
    if (auto canContinue = checkParents(lhs, rhs); !canContinue)
    {
      return;
    }

    checkAllFields(lhs, rhs);
  }

  void checkAllFields(const StructType& lhs, const StructType& rhs)
  {
    const auto& lhsFields = lhs.getFields();
    const auto& rhsFields = rhs.getFields();

    if (lhsFields.size() != rhsFields.size())
    {
      std::string msg;
      msg = "struct ";
      msg += lhs.getQualifiedName();
      msg += " has a different field count ";
      msg += " (";
      msg += std::to_string(lhsFields.size());
      msg += " ) than struct ";
      msg += rhs.getQualifiedName();
      msg += " (";
      msg += std::to_string(rhsFields.size());
      msg += ")";

      addIssue(TypeMatchProblem::missing, std::move(msg), IssueType::major);
    }

    // all fields should be compatible
    for (std::size_t i = 0; i < rhsFields.size(); ++i)
    {
      checkFields(lhsFields[i], rhsFields[i]);
    }
  }

  /// Checks if the parents of both structs (if any) are compatible
  bool checkParents(const StructType& lhs, const StructType& rhs)
  {
    auto lhsParent = lhs.getParent();
    auto rhsParent = rhs.getParent();

    // if any has parents, both should have compatible parents
    if (lhsParent)
    {
      if (!rhsParent)
      {
        std::string msg;
        msg = "struct ";
        msg += lhs.getQualifiedName();
        msg += " has a parent (";
        msg += lhsParent.value()->getQualifiedName();
        msg += ") but struct ";
        msg += rhs.getQualifiedName();
        msg += " does not";

        addIssue(TypeMatchProblem::missing, std::move(msg), IssueType::major);
        return false;
      }

      // both have parents, let's check their compatiblity
      auto validConversion = addIssuesForConversion(*lhsParent.value(), *rhsParent.value());
      if (!validConversion)
      {
        // has daddy issues
        return false;
      }
    }

    return true;
  }

  /// Checks if two struct fields are compatible.
  void checkFields(const StructField& lhs, const StructField& rhs) { addIssuesForConversion(*lhs.type, *rhs.type); }
};

//-------------------------------------------------------------------------------------//
// Variants
//-------------------------------------------------------------------------------------//

class VariantChecker: public IssueCollector
{
  SEN_NOCOPY_NOMOVE(VariantChecker)

public:
  using MetaType = VariantType;

  VariantChecker() = default;
  ~VariantChecker() = default;

public:
  void check(const VariantType& from, const Type& to)
  {
    if (!to.isVariantType())
    {
      rejectConversion(from, to);
      return;
    }

    checkVariants(from, *to.asVariantType());
  }

private:
  void checkVariants(const VariantType& lhs, const VariantType& rhs)
  {
    // all lhs fields should match some field in the rhs
    for (const auto& lhsField: lhs.getFields())
    {
      bool fieldFound = false;
      for (const auto& rhsField: rhs.getFields())
      {
        // we found a correnspondent field
        if (lhsField.key == rhsField.key)
        {
          fieldFound = true;
          checkFields(lhsField, rhsField);
        }
      }

      // report a missing field
      if (!fieldFound)
      {
        std::string msg;
        msg = "variant ";
        msg += lhs.getName();
        msg += " has no correspondence for field with key ";
        msg += std::to_string(lhsField.key);
        msg += " in variant ";
        msg += rhs.getName();
        addIssue(TypeMatchProblem::missing, std::move(msg), IssueType::major);
      }
    }
  }

  /// Checks if two variant fields are compatible.
  void checkFields(const VariantField& lhs, const VariantField& rhs) { addIssuesForConversion(*lhs.type, *rhs.type); }
};

//-------------------------------------------------------------------------------------//
// Sequences
//-------------------------------------------------------------------------------------//

class SequenceChecker: public IssueCollector
{
  SEN_NOCOPY_NOMOVE(SequenceChecker)

public:
  using MetaType = SequenceType;

  SequenceChecker() = default;
  ~SequenceChecker() = default;

public:
  void check(const SequenceType& from, const Type& to)
  {
    if (!to.isSequenceType())
    {
      rejectConversion(from, to);
      return;
    }

    checkSequences(from, *to.asSequenceType());
  }

private:
  void checkSequences(const SequenceType& lhs, const SequenceType& rhs)
  {
    if (auto canContinue = addIssuesForConversion(*lhs.getElementType(), *rhs.getElementType()); !canContinue)
    {
      return;
    }

    checkBounds(lhs, rhs);
  }

  void checkBounds(const SequenceType& lhs, const SequenceType& rhs)
  {
    // check the maximum sizes
    const auto rhsMaxSize = rhs.getMaxSize();
    const auto lhsMaxSize = lhs.getMaxSize();

    if (rhsMaxSize.has_value())
    {
      if (lhsMaxSize.has_value())
      {
        // lhs and rhs are bounded, check if maxs are the same
        if (lhsMaxSize.value() != rhsMaxSize.value())
        {
          std::string msg;
          msg = "sequence ";
          msg += rhs.getName();
          msg += " has a maximum size ";
          msg += std::to_string(rhsMaxSize.value());
          msg += " but the sequence ";
          msg += lhs.getName();
          msg += " has a maximum size of ";
          msg += std::to_string(lhsMaxSize.value());

          addIssue(TypeMatchProblem::compatible, std::move(msg), IssueType::minor);
        }
      }
      else
      {
        // unbounded lhs and bounded rhs
        std::string msg;
        msg = "sequence ";
        msg += rhs.getName();
        msg += " has a maximum size of ";
        msg += std::to_string(rhsMaxSize.value());
        msg += " but the sequence ";
        msg += lhs.getName();
        msg += " is unbounded";

        addIssue(TypeMatchProblem::compatible, std::move(msg), IssueType::minor);
      }
    }
  }
};

//-------------------------------------------------------------------------------------//
// Enumerations
//-------------------------------------------------------------------------------------//

class EnumChecker: public IssueCollector
{
  SEN_NOCOPY_NOMOVE(EnumChecker)

public:
  using MetaType = EnumType;

  EnumChecker() = default;
  ~EnumChecker() = default;

public:
  void check(const EnumType& from, const Type& to)
  {
    if (!to.isEnumType())
    {
      checkStorageConversion(from, to);
      return;
    }

    checkEnums(from, *to.asEnumType());
  }

private:
  void checkStorageConversion(const EnumType& lhs, const Type& other)
  {
    // see if we can convert the integral value of the enum
    addIssuesForConversion(lhs.getStorageType(), other);
  }

  void checkEnumsStorage(const EnumType& lhs, const EnumType& rhs)
  {
    // see if we can convert the integral value of the enum
    addIssuesForConversion(lhs.getStorageType(), rhs.getStorageType());
  }

  void checkKeys(const EnumType& lhs, const EnumType& rhs)
  {
    // all our keys should be present in the other
    for (const auto& lhsEnum: lhs.getEnums())
    {
      bool keyFound = false;

      for (const auto& rhsEnum: rhs.getEnums())
      {
        if (rhsEnum.key == lhsEnum.key)
        {
          keyFound = true;

          // check for different enumeration names
          if (rhsEnum.name != lhsEnum.name)
          {
            std::string msg;
            msg = "enumeration with key ";
            msg += std::to_string(lhsEnum.key);
            msg += " is named as '";
            msg += lhsEnum.name;
            msg += "' in ";
            msg += lhs.getName();
            msg += " and '";
            msg += rhsEnum.name;
            msg += "' in ";
            msg += rhs.getName();

            addIssue({TypeMatchProblem::compatible, msg}, IssueType::minor);
          }

          break;
        }
      }

      if (!keyFound)
      {
        std::string msg;
        msg = "missing key ";
        msg += std::to_string(lhsEnum.key);
        msg += " in ";
        msg += rhs.getName();

        addIssue({TypeMatchProblem::missing, msg}, IssueType::major);
      }
    }
  }

  void checkEnums(const EnumType& lhs, const EnumType& rhs)
  {
    // check if we can store values in rhs
    checkEnumsStorage(lhs, rhs);

    // check for problems in the keys
    checkKeys(lhs, rhs);
  }
};

//-------------------------------------------------------------------------------------//
// Quantities
//-------------------------------------------------------------------------------------//

class QuantityChecker: public IssueCollector
{
  SEN_NOCOPY_NOMOVE(QuantityChecker)

public:
  using MetaType = QuantityType;

  QuantityChecker() = default;
  ~QuantityChecker() = default;

public:
  void check(const QuantityType& from, const Type& to)
  {
    // check if we are converting to another quantity type
    if (to.isQuantityType())
    {
      checkQuantities(from, *to.asQuantityType());
      return;
    }

    // otherwise simply try to convert the numeric type
    addIssuesForConversion(*from.getElementType(), to);
  }

private:
  void checkQuantities(const QuantityType& from, const QuantityType& to)
  {
    auto lhsUnit = from.getUnit();
    auto rhsUnit = to.getUnit();

    if (!lhsUnit && rhsUnit)
    {
      std::string msg;
      msg = "quantity of type ";
      msg += from.getQualifiedName();
      msg += " has no units while ";
      msg += to.getQualifiedName();
      msg += " is measured in ";
      msg += rhsUnit.value()->getNamePlural();

      addIssue({TypeMatchProblem::incompatible, msg}, IssueType::major);
      return;
    }
    if (!rhsUnit && lhsUnit)
    {
      std::string msg;
      msg = "quantity of type ";
      msg += to.getQualifiedName();
      msg += " has no units while ";
      msg += from.getQualifiedName();
      msg += " is measured in ";
      msg += lhsUnit.value()->getNamePlural();

      addIssue({TypeMatchProblem::incompatible, msg}, IssueType::major);
      return;
    }

    // check if the units are compatible
    if (rhsUnit && lhsUnit && lhsUnit.value()->getCategory() != rhsUnit.value()->getCategory())
    {
      std::string msg;
      msg = "quantity of type ";
      msg += from.getQualifiedName();
      msg += " has ";
      msg += lhsUnit.value()->getNamePlural();
      msg += " units while ";
      msg += to.getQualifiedName();
      msg += " is measured in ";
      msg += rhsUnit.value()->getNamePlural();

      addIssue({TypeMatchProblem::incompatible, msg}, IssueType::major);
      return;
    }

    // check maximum range
    if (to.getMaxValue())
    {
      auto toMaxVal = *to.getMaxValue();

      if (!from.getMaxValue().has_value() || *from.getMaxValue() > toMaxVal)
      {
        std::string msg;
        msg = "quantity of type ";
        msg += from.getQualifiedName();
        msg += " has a maximum value that is less restrictive than ";
        msg += to.getQualifiedName();

        addIssue({TypeMatchProblem::compatible, msg}, IssueType::minor);
      }

      if (from.getMinValue().has_value() && *from.getMinValue() > toMaxVal)
      {
        std::string msg;
        {
          msg = "quantity of type ";
          msg += from.getQualifiedName();
          msg += " has a minimum value that is greater than ";
          msg += to.getQualifiedName();
          msg = "'s maximum allowed value ";
        }

        addIssue({TypeMatchProblem::compatible, msg}, IssueType::minor);
      }
    }

    // check minimum range
    if (to.getMinValue())
    {
      auto toMinVal = *to.getMinValue();

      if (!from.getMinValue().has_value() || *from.getMinValue() < toMinVal)
      {
        std::string msg;
        msg = "quantity of type ";
        msg += from.getQualifiedName();
        msg += " has a minimum value that is less restrictive than ";
        msg += to.getQualifiedName();

        addIssue({TypeMatchProblem::compatible, msg}, IssueType::minor);
      }

      if (from.getMaxValue().has_value() && *from.getMaxValue() < toMinVal)
      {
        std::string msg;
        msg = "quantity of type ";
        msg += from.getQualifiedName();
        msg += " has a maximum value that is lower than ";
        msg += to.getQualifiedName();
        msg = "'s minimum allowed value ";

        addIssue({TypeMatchProblem::compatible, msg}, IssueType::minor);
      }
    }

    // check if the values can be converted
    addIssuesForConversion(*from.getElementType(), *to.getElementType());
  }
};

//-------------------------------------------------------------------------------------//
// Aliases
//-------------------------------------------------------------------------------------//

class AliasChecker: public IssueCollector
{
  SEN_NOCOPY_NOMOVE(AliasChecker)

public:
  using MetaType = AliasType;

  AliasChecker() = default;
  ~AliasChecker() = default;

public:
  void check(const AliasType& from, const Type& to)
  {
    // see if we can convert the internal type.
    addIssuesForConversion(*from.getAliasedType(), to);
  }
};

//-------------------------------------------------------------------------------------//
// Void
//-------------------------------------------------------------------------------------//

class VoidChecker: public IssueCollector
{
  SEN_NOCOPY_NOMOVE(VoidChecker)

public:
  using MetaType = VoidType;

  VoidChecker() = default;
  ~VoidChecker() = default;

public:
  void check(const VoidType& from, const Type& to)
  {
    // VoidType cannot be converted to any other data type
    rejectConversion(from, to);
  }
};

//-------------------------------------------------------------------------------------//
// Optional
//-------------------------------------------------------------------------------------//

class OptionalChecker: public IssueCollector
{
  SEN_NOCOPY_NOMOVE(OptionalChecker)

public:
  using MetaType = OptionalType;

  OptionalChecker() = default;
  ~OptionalChecker() = default;

public:
  void check(const OptionalType& from, const Type& to)
  {
    // see if we can convert the internal type.
    addIssuesForConversion(*from.getType(), to);
  }
};

//-------------------------------------------------------------------------------------//
// Classes
//-------------------------------------------------------------------------------------//

class ClassChecker: public IssueCollector
{
  SEN_NOCOPY_NOMOVE(ClassChecker)

public:
  using MetaType = ClassType;

  ClassChecker() = default;
  ~ClassChecker() = default;

public:
  void check(const ClassType& from, const Type& to)
  {
    if (!to.isClassType())
    {
      rejectConversion(from, to);
      return;
    }

    if (!from.isSameOrInheritsFrom(*to.asClassType()))
    {
      rejectConversion(from, to);
    }
  }
};

//-------------------------------------------------------------------------------------//
// Checker
//-------------------------------------------------------------------------------------//

/// Checks if a type can be converted to another
class Checker final: protected FullTypeVisitor, public IssueCollector
{
  SEN_NOCOPY_NOMOVE(Checker)

public:
  ~Checker() override = default;

public:
  static TypeMatch check(const Type& from, const Type& to)
  {
    Checker checker(to);
    from.accept(checker);
    return checker.getResult();
  }

protected:  // generalizations
  [[noreturn]] void apply(const Type& from) override
  {
    std::string err;
    err = from.getName();
    err.append(" type match check is not implemented");

    throw std::logic_error(err);
  }

  void apply(const NativeType& type) override  // NOSONAR
  {
    apply(static_cast<const Type&>(type));
  }

  void apply(const NumericType& type) override { apply(static_cast<const NativeType&>(type)); }

  void apply(const RealType& type) override { apply(static_cast<const NumericType&>(type)); }

  void apply(const IntegralType& type) override { apply(static_cast<const NumericType&>(type)); }

  void apply(const CustomType& type) override  // NOSONAR
  {
    apply(static_cast<const Type&>(type));
  }

public:
  void apply(const VoidType& type) override { applyChecker<VoidChecker>(type); }

  void apply(const BoolType& type) override { applyChecker<ToNativeTypeChecker<BoolType>>(type); }

  void apply(const UInt8Type& type) override { applyChecker<ToNativeTypeChecker<UInt8Type>>(type); }

  void apply(const Int16Type& type) override { applyChecker<ToNativeTypeChecker<Int16Type>>(type); }

  void apply(const UInt16Type& type) override { applyChecker<ToNativeTypeChecker<UInt16Type>>(type); }

  void apply(const Int32Type& type) override { applyChecker<ToNativeTypeChecker<Int32Type>>(type); }

  void apply(const UInt32Type& type) override { applyChecker<ToNativeTypeChecker<UInt32Type>>(type); }

  void apply(const Int64Type& type) override { applyChecker<ToNativeTypeChecker<Int64Type>>(type); }

  void apply(const UInt64Type& type) override { applyChecker<ToNativeTypeChecker<UInt64Type>>(type); }

  void apply(const Float32Type& type) override { applyChecker<ToNativeTypeChecker<Float32Type>>(type); }

  void apply(const Float64Type& type) override { applyChecker<ToNativeTypeChecker<Float64Type>>(type); }

  void apply(const StringType& type) override { applyChecker<ToNativeTypeChecker<StringType>>(type); }

  void apply(const EnumType& type) override { applyChecker<EnumChecker>(type); }

  void apply(const StructType& type) override { applyChecker<StructChecker>(type); }

  void apply(const VariantType& type) override { applyChecker<VariantChecker>(type); }

  void apply(const SequenceType& type) override { applyChecker<SequenceChecker>(type); }

  void apply(const ClassType& type) override { applyChecker<ClassChecker>(type); }

  void apply(const AliasType& type) override { applyChecker<AliasChecker>(type); }

  void apply(const OptionalType& type) override { applyChecker<OptionalChecker>(type); }

  void apply(const QuantityType& type) override { applyChecker<QuantityChecker>(type); }

  void apply(const DurationType& type) override { applyChecker<QuantityChecker>(type); }

  void apply(const TimestampType& type) override { applyChecker<QuantityChecker>(type); }

private:
  explicit Checker(const Type& to): to_(to) {}

  template <typename Checker>
  void applyChecker(const typename Checker::MetaType& from)
  {
    Checker checker;
    checker.check(from, to_);
    addIssues(checker.getIssues(), checker.majorIssueDetected());
  }

private:
  const Type& to_;
};

}  // namespace

//-------------------------------------------------------------------------------------//
// TypeMatch
//-------------------------------------------------------------------------------------//

TypeMatch canConvert(const Type& from, const Type& to)
{
  // same type, no issue
  if (from == to)
  {
    return Ok(TypesMatch {});
  }

  // we can always convert to strings
  if (to.isStringType())
  {
    return Ok(TypesMatch {{validConversion(from, to)}});
  }

  return Checker::check(from, to);
}

}  // namespace sen
