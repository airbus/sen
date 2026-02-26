// === quantity_type.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/quantity_type.h"

// implementation
#include "utils.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/unit.h"

// std
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace sen
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

class MinMaxChecker final: public TypeVisitor
{
  SEN_NOCOPY_NOMOVE(MinMaxChecker)

public:
  explicit MinMaxChecker(const QuantitySpec& theSpec): spec_(theSpec) {}

  ~MinMaxChecker() override = default;

  [[noreturn]] void apply(const NumericType& type) override
  {
    std::ignore = type;
    throwRuntimeError("unsupported numeric type");
  }

  void apply(const VoidType& type) override { std::ignore = type; }

  void apply(const UInt8Type& type) override { check<UInt8Type>(type); }

  void apply(const Int16Type& type) override { check<Int16Type>(type); }

  void apply(const UInt16Type& type) override { check<UInt16Type>(type); }

  void apply(const Int32Type& type) override { check<Int32Type>(type); }

  void apply(const UInt32Type& type) override { check<UInt32Type>(type); }

  void apply(const Int64Type& type) override { check<Int64Type>(type); }

  void apply(const UInt64Type& type) override { check<UInt64Type>(type); }

  void apply(const Float32Type& type) override { check<Float32Type>(type); }

  void apply(const Float64Type& type) override { check<Float64Type>(type); }

private:
  template <typename Meta>
  void check(const NumericType& type) const
  {
    using Native = typename Meta::NativeType;

    if (spec_.minValue && *spec_.minValue < static_cast<float64_t>(std::numeric_limits<Native>::lowest()))
    {
      std::string err = "minimum value (";
      err.append(std::to_string(*spec_.minValue));
      err.append(") cannot be represented using ");
      err.append(type.getName());

      throwRuntimeError(err);
    }
    if (spec_.maxValue && *spec_.maxValue > static_cast<float64_t>(std::numeric_limits<Native>::max()))
    {
      std::string err = "maximum value (";
      err.append(std::to_string(*spec_.maxValue));
      err.append(") cannot be represented using ");
      err.append(type.getName());

      throwRuntimeError(err);
    }
  }

private:
  const QuantitySpec& spec_;
};

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// QuantityType
//--------------------------------------------------------------------------------------------------------------

QuantityType::QuantityType(QuantitySpec spec, Private notUsable) noexcept
  : CustomType(impl::hash<QuantitySpec>()(spec)), spec_(std::move(spec))
{
  std::ignore = notUsable;
}

QuantityType::QuantityType(QuantitySpec spec) noexcept
  : CustomType(impl::hash<QuantitySpec>()(spec)), spec_(std::move(spec))
{
}

TypeHandle<QuantityType> QuantityType::make(QuantitySpec spec)
{
  impl::checkUserTypeName(spec.name);
  impl::checkIdentifier(spec.qualifiedName, true);

  if (spec.minValue && spec.maxValue && *spec.minValue > *spec.maxValue)
  {
    throwRuntimeError("minimum value is greater than maximum value");
  }

  MinMaxChecker checker(spec);
  spec.elementType->accept(checker);

  return {std::move(spec), Private {}};
}

ConstTypeHandle<NumericType> QuantityType::getElementType() const noexcept { return spec_.elementType; }

std::optional<const Unit*> QuantityType::getUnit() const noexcept { return spec_.unit; }

const Unit& QuantityType::getUnit(sen::Unit::EnsureTag /*unused*/) const
{
  if (spec_.unit.has_value())
  {
    return *spec_.unit.value();
  }

  ::sen::throwRuntimeError("Unit was required but no unit was present!");
}

std::optional<float64_t> QuantityType::getMaxValue() const noexcept { return spec_.maxValue; }

std::optional<float64_t> QuantityType::getMinValue() const noexcept { return spec_.minValue; }

std::string_view QuantityType::getQualifiedName() const noexcept { return spec_.qualifiedName; }

std::string_view QuantityType::getName() const noexcept { return spec_.name; }

std::string_view QuantityType::getDescription() const noexcept { return spec_.description; }

const QuantitySpec& QuantityType::getSpec() const noexcept { return spec_; }

bool QuantityType::equals(const Type& other) const noexcept
{
  return &other == this || (other.isQuantityType() && spec_ == other.asQuantityType()->spec_);
}

bool QuantityType::isBounded() const noexcept { return true; }

}  // namespace sen
