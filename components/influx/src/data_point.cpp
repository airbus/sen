// === data_point.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "data_point.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/detail/types_fwd.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"

// std
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace sen::components::influx
{

namespace
{

// -------------------------------------------------------------------------------------------------------------
// Helpers
// -------------------------------------------------------------------------------------------------------------

class ToFieldsVisitor: public TypeVisitor
{
  SEN_NOCOPY_NOMOVE(ToFieldsVisitor)

public:
  ToFieldsVisitor(const Var& value, DataPoint& point, const TimeStamp& time, std::string name)
    : value_(value), point_(point), time_(time), name_(std::move(name))
  {
  }

  ~ToFieldsVisitor() override = default;

public:
  void apply(const Type& type) override
  {
    std::string err;
    err.append("expecting type ");
    err.append(type.getName());
    throwRuntimeError(err);
  }

  void apply(const EnumType& type) override
  {
    const Enumerator* enumerator;

    if (value_.holds<std::string>())
    {
      enumerator = type.getEnumFromName(value_.get<std::string>());
      if (enumerator == nullptr)
      {
        std::string err;
        err.append("invalid enumerator ");
        err.append(value_.get<std::string>());
        err.append(" for enumeration ");
        err.append(type.getQualifiedName());
        throwRuntimeError(err);
      }
    }
    else
    {
      enumerator = type.getEnumFromKey(value_.getCopyAs<uint32_t>());
      if (enumerator == nullptr)
      {
        std::string err;
        err.append("invalid key ");
        err.append(std::to_string(value_.getCopyAs<uint32_t>()));
        err.append(" for enumeration ");
        err.append(type.getQualifiedName());
        throwRuntimeError(err);
      }
    }

    point_.addField(name_, enumerator->name);
    point_.addField(name_ + ".key", enumerator->key);
  }

  void apply(const BoolType& /*type*/) override { point_.addField(name_, value_.getCopyAs<bool>()); }

  void apply(const UInt8Type& /*type*/) override { point_.addField(name_, value_.getCopyAs<uint8_t>()); }

  void apply(const Int16Type& /*type*/) override { point_.addField(name_, value_.getCopyAs<int16_t>()); }

  void apply(const UInt16Type& /*type*/) override { point_.addField(name_, value_.getCopyAs<uint16_t>()); }

  void apply(const Int32Type& /*type*/) override { point_.addField(name_, value_.getCopyAs<int32_t>()); }

  void apply(const UInt32Type& /*type*/) override { point_.addField(name_, value_.getCopyAs<uint32_t>()); }

  void apply(const Int64Type& /*type*/) override { point_.addField(name_, value_.getCopyAs<int64_t>()); }

  void apply(const UInt64Type& /*type*/) override { point_.addField(name_, value_.getCopyAs<uint64_t>()); }

  void apply(const Float32Type& /*type*/) override { point_.addField(name_, value_.getCopyAs<float32_t>()); }

  void apply(const Float64Type& /*type*/) override { point_.addField(name_, value_.getCopyAs<float64_t>()); }

  void apply(const DurationType& /*type*/) override { point_.addField(name_, value_.getCopyAs<Duration>()); }

  void apply(const TimestampType& /*type*/) override { point_.addField(name_, value_.getCopyAs<TimeStamp>()); }

  void apply(const StringType& /*type*/) override { point_.addField(name_, value_.get<std::string>()); }

  void apply(const StructType& type) override
  {
    const auto& map = value_.get<VarMap>();

    // early exit
    if (map.empty())
    {
      return;
    }

    for (const auto& field: type.getFields())
    {
      auto itr = map.find(field.name);
      if (itr != map.end())
      {
        varToFields(point_, field.type.type(), itr->second, time_, name_ + "." + field.name);
      }
    }
  }

  void apply(const VariantType& type) override
  {
    if (value_.holds<KeyedVar>())
    {
      const auto& [key, varVal] = value_.get<KeyedVar>();
      const auto* field = type.getFieldFromKey(key);
      if (field == nullptr)
      {
        std::string err;
        err.append("invalid key ");
        err.append(std::to_string(key));
        err.append(" for variant ");
        err.append(type.getQualifiedName());
        throwRuntimeError(err);
      }

      point_.addField(name_ + ".type", std::string(field->type->getName()));
      varToFields(point_, field->type.type(), *varVal, time_, name_ + ".value");
    }
    else if (value_.holds<VarMap>())
    {
      const auto& map = value_.get<VarMap>();
      const auto fieldKey = findElement(
        map, "type", std::string("expecting type in variant value for ") + std::string(type.getQualifiedName()));
      const auto fieldValue = findElement(
        map, "value", std::string("expecting value in variant value for ") + std::string(type.getQualifiedName()));

      const auto key = fieldKey.getCopyAs<uint32_t>();
      const auto* field = type.getFieldFromKey(key);
      if (field == nullptr)
      {
        std::string err;
        err.append("invalid key ");
        err.append(std::to_string(key));
        err.append(" for variant ");
        err.append(type.getQualifiedName());
        throwRuntimeError(err);
      }

      point_.addField(name_ + ".type", std::string(field->type->getName()));
      varToFields(point_, field->type.type(), fieldValue, time_, name_ + ".value");
    }
    else
    {
      std::string err;
      err.append("invalid value for variant ");
      err.append(type.getQualifiedName());
      throwRuntimeError(err);
    }
  }

  void apply(const SequenceType& type) override
  {
    const auto& list = value_.get<VarList>();
    for (std::size_t i = 0U; i < list.size(); ++i)
    {
      varToFields(point_, type.getElementType().type(), list[i], time_, name_ + "[" + std::to_string(i) + "]");
    }
  }

  void apply(const QuantityType& type) override
  {
    varToFields(point_, type.getElementType().type(), value_, time_, name_);
  }

  void apply(const AliasType& type) override
  {
    varToFields(point_, type.getAliasedType().type(), value_, time_, name_);
  }

  void apply(const OptionalType& type) override
  {
    if (!value_.isEmpty())
    {
      varToFields(point_, type.getType().type(), value_, time_, name_);
    }
  }

private:
  const Var& value_;
  DataPoint& point_;
  const TimeStamp& time_;
  std::string name_;
};

}  // namespace

void varToFields(DataPoint& point, const Type* type, const Var& value, TimeStamp time, const std::string& fieldName)
{
  ToFieldsVisitor visitor(value, point, time, fieldName);
  type->accept(visitor);
}

}  // namespace sen::components::influx
