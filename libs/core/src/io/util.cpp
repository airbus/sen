// === util.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/io/util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/result.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
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
#include "sen/core/meta/type_traits.h"
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/unit.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"

// spdlog
#include <spdlog/logger.h>

// std
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>

namespace sen::impl
{

namespace
{

std::shared_ptr<spdlog::logger> getLogger()
{
  static auto logger = std::make_shared<spdlog::logger>("io");
  return logger;
}

//--------------------------------------------------------------------------------------------------------------
// VariantAdapter
//--------------------------------------------------------------------------------------------------------------

class VariantAdapter: public FullTypeVisitor
{
  SEN_NOCOPY_NOMOVE(VariantAdapter)

public:
  explicit VariantAdapter(Var& var, const Type* originType = nullptr, bool useStrings = false)
    : var_(var), originType_(originType), useStrings_(useStrings), result_(sen::Ok())
  {
  }
  ~VariantAdapter() override = default;

public:
  [[nodiscard]] const Result<void, std::string>& getResult() const noexcept { return result_; }

public:
  void apply(const Type& type) override { std::ignore = type; }

  void apply(const VoidType& type) override { std::ignore = type; }

  // native types
  void apply(const NativeType& type) override { apply(static_cast<const Type&>(type)); }

  void apply(const EnumType& type) override
  {
    // handle possible enum inside an optional
    if (var_.holds<std::monostate>())
    {
      return;
    }

    // var is a string
    if (var_.holds<std::string>())
    {
      if (useStrings_)
      {
        return;
      }

      const auto& enumeratorName = var_.get<std::string>();

      // if the enumeratorName is empty and the originType is a string, replace it with the first enum key (0)
      if (originType_ != nullptr && originType_->isStringType() && enumeratorName.empty())
      {
        var_ = type.getEnumFromKey(0U)->key;
        return;
      }

      const auto* enumerator = type.getEnumFromName(enumeratorName);
      if (enumerator == nullptr)
      {
        std::string err;
        err.append("invalid enumerator '");
        err.append(enumeratorName);
        err.append("' for enum ");
        err.append(type.getQualifiedName());
        getLogger()->warn(err);

        result_ = Err(std::move(err));
        return;
      }
      var_ = enumerator->key;
      return;
    }

    // var was generated from an enum (is an integer)
    const auto key = var_.getCopyAs<uint32_t>();
    auto* enumerator = type.getEnumFromKey(key);

    // translate the enum using the name in case the origin type is provided
    if (originType_ != nullptr)
    {
      if (const auto* enumType = originType_->asEnumType(); enumType)
      {
        // get the origin enumerator and take the target enumerator that has the same name
        const auto* originEnum = enumType->getEnumFromKey(key);
        enumerator = type.getEnumFromName(originEnum->name);
      }
    }

    if (enumerator == nullptr)
    {
      std::string err;
      err.append("invalid enumerator key '");
      err.append(std::to_string(key));
      err.append("' for enum ");
      err.append(type.getQualifiedName());
      getLogger()->warn(err);

      result_ = Err(std::move(err));
      return;
    }

    if (useStrings_)
    {
      var_ = enumerator->name;
    }
    else
    {
      var_ = enumerator->key;
    }
  }

  void apply(const BoolType& type) override
  {
    std::ignore = type;
    var_ = var_.getCopyAs<bool, /*checkedConversion=*/false>();
  }

  void apply(const NumericType& type) override
  {
    if (auto* integral = type.asIntegralType())
    {
      apply(*integral);
    }
    else if (auto* real = type.asRealType())
    {
      apply(*real);
    }
    else
    {
      SEN_ASSERT(false && "Could not match underlying type.");
    }
  }

  void apply(const IntegralType& type) override
  {
    // Dispatch to correct sized type
    if (auto* b = type.asBoolType())
    {
      apply(*b);
    }
    else if (auto* uint8 = type.asUInt8Type())
    {
      apply(*uint8);
    }
    else if (auto* uint16 = type.asUInt16Type())
    {
      apply(*uint16);
    }
    else if (auto* int16 = type.asInt16Type())
    {
      apply(*int16);
    }
    else if (auto* uint32 = type.asUInt32Type())
    {
      apply(*uint32);
    }
    else if (auto* int32 = type.asInt32Type())
    {
      apply(*int32);
    }
    else if (auto* uint64 = type.asUInt64Type())
    {
      apply(*uint64);
    }
    else if (auto* int64 = type.asInt64Type())
    {
      apply(*int64);
    }
    else
    {
      SEN_ASSERT(false && "Could not match sized floating point type.");
    }
  }

  void apply(const RealType& type) override
  {
    // Dispatch to correct sized type
    if (auto* f32 = type.asFloat32Type())
    {
      apply(*f32);
    }
    else if (auto* f64 = type.asFloat64Type())
    {
      apply(*f64);
    }
    else
    {
      SEN_ASSERT(false && "Could not match sized floating point type.");
    }
  }

  void apply(const UInt8Type& type) override
  {
    std::ignore = type;
    var_ = var_.getCopyAs<uint8_t, /*checkedConversion=*/false>();
  }

  void apply(const Int16Type& type) override
  {
    std::ignore = type;
    var_ = var_.getCopyAs<int16_t, /*checkedConversion=*/false>();
  }

  void apply(const UInt16Type& type) override
  {
    std::ignore = type;
    var_ = var_.getCopyAs<uint16_t, /*checkedConversion=*/false>();
  }

  void apply(const Int32Type& type) override
  {
    std::ignore = type;
    var_ = var_.getCopyAs<int32_t, /*checkedConversion=*/false>();
  }

  void apply(const UInt32Type& type) override
  {
    std::ignore = type;
    var_ = var_.getCopyAs<uint32_t, /*checkedConversion=*/false>();
  }

  void apply(const Int64Type& type) override
  {
    std::ignore = type;
    var_ = var_.getCopyAs<int64_t, /*checkedConversion=*/false>();
  }

  void apply(const UInt64Type& type) override
  {

    std::ignore = type;
    var_ = var_.getCopyAs<uint64_t, /*checkedConversion=*/false>();
  }

  void apply(const Float32Type& type) override
  {
    std::ignore = type;
    var_ = var_.getCopyAs<float32_t, /*checkedConversion=*/false>();
  }

  void apply(const Float64Type& type) override
  {
    std::ignore = type;
    var_ = var_.getCopyAs<float64_t, /*checkedConversion=*/false>();
  }

  void apply(const DurationType& type) override { std::ignore = type; }

  void apply(const TimestampType& type) override { std::ignore = type; }

  void apply(const StringType& type) override
  {
    std::ignore = type;

    // check if the origin type is an enum, in that case adapt to the name of the enumerator
    if (originType_ != nullptr)
    {
      if (const auto* enumType = originType_->asEnumType(); enumType != nullptr)
      {
        const auto* enumerator = enumType->getEnumFromKey(var_.getCopyAs<uint32_t>());
        var_ = enumerator->name;
      }
    }
  }

  // custom types
  void apply(const CustomType& type) override { apply(static_cast<const Type&>(type)); }

  void apply(const StructType& type) override
  {
    // handle possible struct inside an optional
    if (var_.holds<std::monostate>())
    {
      return;
    }

    if (!var_.holds<VarMap>())
    {
      std::string err;
      err.append("Invalid format (not a map) for structure of type ");
      err.append(type.getQualifiedName());
      getLogger()->warn(err);

      result_ = Err(std::move(err));
      return;
    }

    adaptStruct(var_.get<VarMap>(), type);
  }

  void apply(const VariantType& type) override
  {
    // handle possible variant inside an optional
    if (var_.holds<std::monostate>())
    {
      return;
    }

    if (var_.holds<KeyedVar>())
    {
      auto& keyedVar = var_.get<KeyedVar>();
      auto fieldKey = std::get<0>(keyedVar);
      const auto* variantField = type.getFieldFromKey(fieldKey);
      if (variantField == nullptr)
      {
        std::string err;
        err.append("missing type with key ");
        err.append(std::to_string(fieldKey));
        err.append(" while adapting variant to ");
        err.append(type.getQualifiedName());

        getLogger()->warn(err);
        result_ = Err(std::move(err));
        return;
      }

      result_ = adaptVariant(*variantField->type, *std::get<1>(keyedVar), std::nullopt, useStrings_);
      return;
    }

    if (!var_.holds<VarMap>())
    {
      std::string err;
      err.append("Invalid format for variant of type ");
      err.append(type.getQualifiedName());
      err.append(R"(. Use { "type": "<type>", "value": <value> })");
      getLogger()->warn(err);

      result_ = Err(err);
      return;
    }

    auto& map = var_.get<VarMap>();

    auto typeIterator = map.find("type");
    if (typeIterator == map.end())
    {
      std::string err;
      err.append("Missing 'type' field for variant of type ");
      err.append(type.getQualifiedName());
      err.append(R"(. Use { "type": "<type>", "value": <value> })");
      result_ = Err(err);
      return;
    }

    MaybeConstTypeHandle<> fieldType;
    uint32_t fieldKey = 0;

    if (typeIterator->second.holds<std::string>())
    {
      const auto fieldTypeName = typeIterator->second.get<std::string>();
      for (const auto& variantField: type.getFields())
      {
        const auto* custom = variantField.type->asCustomType();
        if (variantField.type->getName() == fieldTypeName ||
            (custom != nullptr && custom->getQualifiedName() == fieldTypeName))
        {
          typeIterator->second = variantField.key;
          fieldType = variantField.type;
          fieldKey = variantField.key;
          break;
        }
      }

      if (!fieldType)
      {
        std::string err;
        err.append("Invalid 'type' field '");
        err.append(fieldTypeName);
        err.append("' for variant of type ");
        err.append(type.getQualifiedName());
        err.append(". Valid types are: ");
        for (const auto& variantField: type.getFields())
        {
          err.append("'");
          err.append(variantField.type->getName());
          err.append("' ");
        }

        getLogger()->warn(err);
        result_ = Err(std::move(err));
        return;
      }
    }
    else
    {
      fieldKey = typeIterator->second.getCopyAs<uint32_t>();
      const auto* variantField = type.getFieldFromKey(fieldKey);
      if (variantField == nullptr)
      {
        std::string err;
        err.append("Invalid 'type' field '");
        err.append(std::to_string(fieldKey));
        err.append("' for variant of type ");
        err.append(type.getQualifiedName());
        getLogger()->warn(err);

        result_ = Err(std::move(err));
        return;
      }
      fieldType = variantField->type;
    }

    // ensure that 'type' holds an int
    map["type"] = fieldKey;

    if (auto valueItr = map.find("value"); valueItr == map.end())
    {
      std::string err;
      err.append("Missing 'value' field for variant of type ");
      err.append(type.getQualifiedName());
      err.append(R"(. Use { "type": "<type>", "value": <value> })");
      getLogger()->warn(err);

      result_ = Err(std::move(err));
      return;
    }

    SEN_ASSERT(fieldType.has_value() && "fieldType is required to adapt the variant.");
    result_ = adaptVariant(*fieldType.value(), map["value"], std::nullopt, useStrings_);
  }

  void apply(const SequenceType& type) override
  {
    if (var_.isEmpty())
    {
      return;
    }

    if (!var_.holds<VarList>())
    {
      std::string err;
      err.append("Invalid format for list of type ");
      err.append(type.getQualifiedName());
      getLogger()->warn(err);

      result_ = Err(std::move(err));
      return;
    }

    auto& list = var_.get<VarList>();

    if (type.isBounded())
    {
      const auto targetSize = type.getMaxSize().value();
      const auto varSize = list.size();

      if (varSize > targetSize)
      {
        getLogger()->warn(
          "elements will be discarded while adapting to target sequence {} of size {}. The source sequence was "
          "longer.",
          type.getQualifiedName(),
          targetSize);
      }

      // resize if we need to truncate
      if (type.hasFixedSize() || targetSize < varSize)
      {
        list.resize(targetSize);
      }

      for (auto& elem: list)
      {
        // adapt the element variant, and hold the first error as our result
        auto elementResult = adaptVariant(*type.getElementType(), elem, std::nullopt, useStrings_);
        if (elementResult.isError() && result_.isOk())
        {
          result_ = elementResult;
        }
      }
    }
    else
    {
      for (auto& elem: list)
      {
        // adapt the element variant, and hold the first error as our result
        auto elementResult = adaptVariant(*type.getElementType(), elem, std::nullopt, useStrings_);
        if (elementResult.isError() && result_.isOk())
        {
          result_ = elementResult;
        }
      }
    }
  }

  void apply(const ClassType& type) override { apply(static_cast<const CustomType&>(type)); }

  void apply(const QuantityType& type) override
  {
    // durations are fine
    if (var_.holds<Duration>())
    {
      return;
    }

    // timestamps are fine
    if (var_.holds<TimeStamp>())
    {
      return;
    }

    // check if the origin type is a quantity (even if it was in an optional) and convert the units
    if (originType_ != nullptr)
    {
      auto originType = originType_;
      if (const auto* optionalType = originType_->asOptionalType(); optionalType != nullptr)
      {
        originType = optionalType->getType()->asQuantityType();
      }

      if (const auto* originQuantity = originType->asQuantityType(); originQuantity != nullptr)
      {
        var_ = Unit::convert(
          originQuantity->getUnit(Unit::ensurePresent), type.getUnit(Unit::ensurePresent), var_.getCopyAs<f64>());
      }
    }

    if (var_.isEmpty())
    {
      return;
    }

    // check that we are in a valid range
    const auto currentValue = var_.getCopyAs<f64>();

    if (const auto maxVal = type.getMaxValue(); maxVal.has_value() && maxVal.value() < currentValue)
    {
      std::string err;
      err.append(type.getName());
      err.append(" quantity value ");
      err.append(std::to_string(currentValue));
      err.append(" exceeded the maximum value of ");
      err.append(std::to_string(maxVal.value()));
      err.append(". Saturating to the maximum value.");

      getLogger()->warn(err);
      result_ = Err(std::move(err));
      var_ = maxVal.value();
    }

    if (const auto minVal = type.getMinValue(); minVal.has_value() && currentValue < minVal.value())
    {
      std::string err;
      err.append(type.getName());
      err.append(" quantity value ");
      err.append(std::to_string(currentValue));
      err.append(" smaller than the minimum value of ");
      err.append(std::to_string(minVal.value()));
      err.append(". Saturating to the minimum value.");

      getLogger()->warn(err);
      result_ = Err(std::move(err));
      var_ = minVal.value();
    }
  }

  void apply(const AliasType& type) override
  {
    result_ = adaptVariant(*type.getAliasedType(), var_, std::nullopt, useStrings_);
  }

  void apply(const OptionalType& type) override
  {
    if (var_.isEmpty())
    {
      return;
    }

    result_ = adaptVariant(*type.getType(), var_, std::nullopt, useStrings_);
  }

private:
  void adaptStruct(VarMap& map, const StructType& type)
  {
    if (auto parent = type.getParent())
    {
      adaptStruct(map, *parent.value());
    }

    for (const auto& field: type.getFields())
    {
      if (auto fieldItr = map.find(field.name); fieldItr == map.end())
      {
        std::string err;
        err.append("missing field ");
        err.append(field.name);
        err.append(" of type ");
        err.append(field.type->getName());
        err.append(" while adapting struct to type ");
        err.append(type.getQualifiedName());

        getLogger()->warn(err);
        result_ = Err(std::move(err));
        return;
      }

      auto fieldResult = adaptVariant(*field.type, map[field.name], std::nullopt, useStrings_);
      if (fieldResult.isError() && result_.isOk())
      {
        result_ = fieldResult;
      }
    }
  }

private:
  Var& var_;
  const Type* originType_ = nullptr;  // type from which the Var was created (optional)
  bool useStrings_ = false;
  Result<void, std::string> result_;
};

//--------------------------------------------------------------------------------------------------------------
// StreamReader
//--------------------------------------------------------------------------------------------------------------

class StreamReader: public FullTypeVisitor
{
  SEN_NOCOPY_NOMOVE(StreamReader)

public:
  StreamReader(Var& var, InputStream& in): var_(var), in_(in) {}

  ~StreamReader() override = default;

public:  // abstractions
  void apply(const Type& type) override { unhandled(type); }

  // native types
  void apply(const NativeType& type) override { unhandled(type); }

  void apply(const NumericType& type) override { unhandled(type); }

  void apply(const IntegralType& type) override { unhandled(type); }

  void apply(const RealType& type) override { unhandled(type); }

  void apply(const ClassType& type) override { unhandled(type); }

public:  // specific types
  void apply(const EnumType& type) override { type.getStorageType().accept(*this); }

  void apply(const VoidType& type) override { std::ignore = type; }

  void apply(const BoolType& type) override
  {
    std::ignore = type;
    readNative<bool>();
  }

  void apply(const UInt8Type& type) override
  {
    std::ignore = type;
    readNative<uint8_t>();
  }

  void apply(const Int16Type& type) override
  {
    std::ignore = type;
    readNative<int16_t>();
  }

  void apply(const UInt16Type& type) override
  {
    std::ignore = type;
    readNative<uint16_t>();
  }

  void apply(const Int32Type& type) override
  {
    std::ignore = type;
    readNative<int32_t>();
  }

  void apply(const UInt32Type& type) override
  {
    std::ignore = type;
    readNative<uint32_t>();
  }

  void apply(const Int64Type& type) override
  {
    std::ignore = type;
    readNative<int64_t>();
  }

  void apply(const UInt64Type& type) override
  {
    std::ignore = type;
    readNative<uint64_t>();
  }

  void apply(const Float32Type& type) override
  {
    std::ignore = type;
    readNative<float32_t>();
  }

  void apply(const Float64Type& type) override
  {
    std::ignore = type;
    readNative<float64_t>();
  }

  void apply(const DurationType& type) override
  {
    std::ignore = type;
    readNative<Duration>();
  }

  void apply(const TimestampType& type) override
  {
    std::ignore = type;
    TimeStamp timeStamp;
    in_.readTimeStamp(timeStamp);
    var_ = timeStamp;
  }

  void apply(const StringType& type) override
  {
    std::ignore = type;
    readNative<std::string>();
  }

  // custom types
  void apply(const CustomType& type) override { apply(static_cast<const Type&>(type)); }

  void apply(const StructType& type) override
  {
    VarMap map {};
    readStruct(in_, type, map);
    var_ = map;
  }

  void apply(const VariantType& type) override
  {
    uint32_t fieldKey = 0U;
    in_.readUInt32(fieldKey);

    auto* field = type.getFieldFromKey(fieldKey);
    if (field == nullptr)
    {
      std::string err;
      err.append("could not find field '");
      err.append(std::to_string(fieldKey));
      err.append("' in variant '");
      err.append(type.getQualifiedName());
      err.append("'");
      throwRuntimeError(err);
    }

    // read the value
    auto fieldValue = std::make_shared<Var>();
    {
      StreamReader reader(*fieldValue, in_);
      field->type->accept(reader);
    }

    var_ = KeyedVar(fieldKey, std::move(fieldValue));
  }

  void apply(const SequenceType& type) override
  {
    var_ = VarList {};
    auto& list = var_.get<VarList>();

    uint32_t size = 0U;

    if (type.hasFixedSize())
    {
      size = static_cast<uint32_t>(type.getMaxSize().value());
    }
    else
    {
      in_.readUInt32(size);
    }

    if (size == 0U)
    {
      list.resize(0U);
      return;
    }

    // allocate space for the data
    list.resize(size);

    // read all the elements
    for (auto& elem: list)
    {
      StreamReader reader(elem, in_);
      type.getElementType()->accept(reader);
    }
  }

  void apply(const QuantityType& type) override { type.getElementType()->accept(*this); }

  void apply(const AliasType& type) override { type.getAliasedType()->accept(*this); }

  void apply(const OptionalType& type) override
  {
    bool present = false;
    in_.readBool(present);

    if (present)
    {
      type.getType()->accept(*this);
    }
    else
    {
      var_ = std::monostate();
    }
  }

private:
  template <typename T>
  inline void readNative()
  {
    T val {};
    SerializationTraits<T>::read(in_, val);
    var_ = val;
  }

  [[noreturn]] static void unhandled(const Type& type)
  {
    std::string err;
    err.append("unhandled type '");
    err.append(type.getName());
    err.append("' in StreamReader of GenericRemoteObject");
    throwRuntimeError(err);
  }

  void readStruct(InputStream& in, const StructType& type, VarMap& map)
  {
    if (auto parent = type.getParent())
    {
      readStruct(in, *parent.value(), map);
    }

    for (const auto& field: type.getFields())
    {
      Var fieldVar;
      StreamReader visitor(fieldVar, in_);
      field.type->accept(visitor);
      map.try_emplace(field.name, std::move(fieldVar));
    }
  }

private:
  Var& var_;
  InputStream& in_;
};

//--------------------------------------------------------------------------------------------------------------
// StreamWriter
//--------------------------------------------------------------------------------------------------------------

class StreamWriter: public FullTypeVisitor
{
  SEN_NOCOPY_NOMOVE(StreamWriter)

public:
  StreamWriter(const Var& var, OutputStream& out): var_(var), out_(out) {}

  ~StreamWriter() override = default;

public:  // abstractions
  void apply(const Type& type) override { unhandled(type); }

  // native types
  void apply(const NativeType& type) override { unhandled(type); }

  void apply(const NumericType& type) override { unhandled(type); }

  void apply(const IntegralType& type) override { unhandled(type); }

  void apply(const RealType& type) override { unhandled(type); }

  void apply(const ClassType& type) override { unhandled(type); }

public:  // specific types
  void apply(const EnumType& type) override { type.getStorageType().accept(*this); }

  void apply(const VoidType& type) override { std::ignore = type; }

  void apply(const BoolType& type) override
  {
    std::ignore = type;
    writeNative<bool>();
  }

  void apply(const UInt8Type& type) override
  {
    std::ignore = type;
    writeNative<uint8_t>();
  }

  void apply(const Int16Type& type) override
  {
    std::ignore = type;
    writeNative<int16_t>();
  }

  void apply(const UInt16Type& type) override
  {
    std::ignore = type;
    writeNative<uint16_t>();
  }

  void apply(const Int32Type& type) override
  {
    std::ignore = type;
    writeNative<int32_t>();
  }

  void apply(const UInt32Type& type) override
  {
    std::ignore = type;
    writeNative<uint32_t>();
  }

  void apply(const Int64Type& type) override
  {
    std::ignore = type;
    writeNative<int64_t>();
  }

  void apply(const UInt64Type& type) override
  {
    std::ignore = type;
    writeNative<uint64_t>();
  }

  void apply(const Float32Type& type) override
  {
    std::ignore = type;
    writeNative<float32_t>();
  }

  void apply(const Float64Type& type) override
  {
    std::ignore = type;
    writeNative<float64_t>();
  }

  void apply(const DurationType& type) override
  {
    std::ignore = type;
    writeNative<Duration>();
  }

  void apply(const TimestampType& type) override
  {
    std::ignore = type;
    writeNative<TimeStamp>();
  }

  void apply(const StringType& type) override
  {
    std::ignore = type;
    writeNative<std::string>();
  }

  // custom types
  void apply(const CustomType& type) override { apply(static_cast<const Type&>(type)); }

  void apply(const StructType& type) override
  {
    auto map = var_.get<VarMap>();
    writeStruct(map, type);
  }

  void apply(const VariantType& type) override
  {
    if (var_.holds<KeyedVar>())
    {
      const auto& [fieldKey, fieldValue] = var_.get<KeyedVar>();
      out_.writeUInt32(fieldKey);

      StreamWriter writer(*fieldValue, out_);
      type.getFieldFromKey(fieldKey)->type->accept(writer);
    }
    else
    {
      const auto& map = var_.get<VarMap>();
      auto fieldKey = map.find("type")->second.getCopyAs<uint32_t>();
      auto fieldType = type.getFieldFromKey(fieldKey)->type;
      auto fieldValue = map.find("value")->second;

      out_.writeUInt32(fieldKey);
      StreamWriter writer(fieldValue, out_);
      fieldType->accept(writer);
    }
  }

  void apply(const SequenceType& type) override
  {
    if (var_.holds<VarList>())
    {
      const auto& list = var_.get<VarList>();

      std::size_t size = 0;
      if (type.hasFixedSize())
      {
        size = type.getMaxSize().value();
      }
      else
      {
        size = static_cast<uint32_t>(list.size());
        out_.writeUInt32(static_cast<uint32_t>(size));
      }

      if (size == 0U)
      {
        return;
      }

      // write all the elements
      for (auto& elem: list)
      {
        StreamWriter writer(elem, out_);
        type.getElementType()->accept(writer);
      }
    }
    else
    {
      out_.writeUInt32(0);
    }
  }

  void apply(const QuantityType& type) override { type.getElementType()->accept(*this); }

  void apply(const AliasType& type) override { type.getAliasedType()->accept(*this); }

  void apply(const OptionalType& type) override
  {
    const auto isPresent = !var_.isEmpty();
    out_.writeBool(isPresent);
    if (isPresent)
    {
      type.getType()->accept(*this);
    }
  }

private:
  template <typename T>
  void writeNative()
  {
    if (var_.holds<std::monostate>())
    {
      SerializationTraits<T>::write(out_, T {});
    }
    else
    {
      SerializationTraits<T>::write(out_, var_.getCopyAs<T>());
    }
  }

  void writeStruct(VarMap& map, const StructType& type)
  {
    if (auto parent = type.getParent())
    {
      writeStruct(map, *parent.value());
    }

    for (const auto& field: type.getFields())
    {
      StreamWriter writer(map[field.name], out_);
      field.type->accept(writer);
    }
  }

  [[noreturn]] static void unhandled(const Type& type)
  {
    std::string err;
    err.append("unhandled type '");
    err.append(type.getName());
    err.append("' in StreamReader of GenericRemoteObject");
    throwRuntimeError(err);
  }

private:
  const Var& var_;
  OutputStream& out_;
};

//--------------------------------------------------------------------------------------------------------------
// SerializedSizeGetter
//--------------------------------------------------------------------------------------------------------------

class SerializedSizeGetter: public FullTypeVisitor
{
  SEN_NOCOPY_NOMOVE(SerializedSizeGetter)

public:
  explicit SerializedSizeGetter(const Var& var): var_(var) {}

  ~SerializedSizeGetter() override = default;

  [[nodiscard]] uint32_t getResult() const noexcept { return result_; }

public:  // abstractions
  void apply(const Type& type) override { unhandled(type); }

  // native types
  void apply(const NativeType& type) override { unhandled(type); }

  void apply(const NumericType& type) override { unhandled(type); }

  void apply(const IntegralType& type) override { unhandled(type); }

  void apply(const RealType& type) override { unhandled(type); }

  void apply(const ClassType& type) override { unhandled(type); }

public:  // specific types
  void apply(const EnumType& type) override { type.getStorageType().accept(*this); }

  void apply(const VoidType& type) override { std::ignore = type; }

  void apply(const BoolType& type) override
  {
    std::ignore = type;
    result_ += computeNative<bool>();
  }

  void apply(const UInt8Type& type) override
  {
    std::ignore = type;
    result_ += computeNative<uint8_t>();
  }

  void apply(const Int16Type& type) override
  {
    std::ignore = type;
    result_ += computeNative<int16_t>();
  }

  void apply(const UInt16Type& type) override
  {
    std::ignore = type;
    result_ += computeNative<uint16_t>();
  }

  void apply(const Int32Type& type) override
  {
    std::ignore = type;
    result_ += computeNative<int32_t>();
  }

  void apply(const UInt32Type& type) override
  {
    std::ignore = type;
    result_ += computeNative<uint32_t>();
  }

  void apply(const Int64Type& type) override
  {
    std::ignore = type;
    result_ += computeNative<int64_t>();
  }

  void apply(const UInt64Type& type) override
  {
    std::ignore = type;
    result_ += computeNative<uint64_t>();
  }

  void apply(const Float32Type& type) override
  {
    std::ignore = type;
    result_ += computeNative<float32_t>();
  }

  void apply(const Float64Type& type) override
  {
    std::ignore = type;
    result_ += computeNative<float64_t>();
  }

  void apply(const DurationType& type) override
  {
    std::ignore = type;
    result_ += computeNative<Duration>();
  }

  void apply(const TimestampType& type) override
  {
    std::ignore = type;
    result_ += computeNative<TimeStamp>();
  }

  void apply(const StringType& type) override
  {
    std::ignore = type;
    result_ += computeNative<std::string>();
  }

  // custom types
  void apply(const CustomType& type) override { apply(static_cast<const Type&>(type)); }

  void apply(const StructType& type) override
  {
    const auto& map = var_.get<VarMap>();

    for (const auto& field: type.getFields())
    {
      SerializedSizeGetter getter(map.find(field.name)->second);
      field.type->accept(getter);

      result_ += getter.getResult();
    }
  }

  void apply(const VariantType& type) override
  {
    const auto& [fieldKey, fieldValue] = var_.get<KeyedVar>();
    result_ += SerializationTraits<uint32_t>::serializedSize(fieldKey);

    SerializedSizeGetter getter(*fieldValue);
    type.getFieldFromKey(fieldKey)->type->accept(getter);

    result_ += getter.getResult();
  }

  void apply(const SequenceType& type) override
  {
    const auto& list = var_.get<VarList>();

    auto size = static_cast<uint32_t>(list.size());
    result_ += SerializationTraits<uint32_t>::serializedSize(size);

    if (size == 0U)
    {
      return;
    }

    // write all the elements
    for (auto& elem: list)
    {
      SerializedSizeGetter getter(elem);
      type.getElementType()->accept(getter);
      result_ += getter.getResult();
    }
  }

  void apply(const QuantityType& type) override { type.getElementType()->accept(*this); }

  void apply(const AliasType& type) override { type.getAliasedType()->accept(*this); }

  void apply(const OptionalType& type) override
  {
    const bool isPresent = !var_.isEmpty();
    result_ += SerializationTraits<bool>::serializedSize(isPresent);
    if (isPresent)
    {
      type.getType()->accept(*this);
    }
  }

private:
  template <typename T>
  uint32_t computeNative()
  {
    return SerializationTraits<T>::serializedSize(var_.getCopyAs<T>());
  }

  [[noreturn]] static void unhandled(const Type& type)
  {
    std::string err;
    err.append("unhandled type '");
    err.append(type.getName());
    err.append("' in StreamReader of GenericRemoteObject");
    throwRuntimeError(err);
  }

private:
  const Var& var_;
  uint32_t result_ = 0U;
};

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Implementation
//--------------------------------------------------------------------------------------------------------------

void readFromStream(Var& var, InputStream& in, const Type& type)
{
  StreamReader reader(var, in);
  type.accept(reader);
}

void writeToStream(const Var& var, OutputStream& out, const Type& type, MaybeConstTypeHandle<> originType)
{
  auto varCopy = var;
  std::ignore = adaptVariant(type, varCopy, originType);

  StreamWriter writer(varCopy, out);
  type.accept(writer);
}

[[nodiscard]] uint32_t getSerializedSize(const Var& var, const Type* type)
{
  SerializedSizeGetter getter(var);
  type->accept(getter);
  return getter.getResult();
}

Result<void, std::string> adaptVariant(const Type& targetType,
                                       Var& variant,
                                       MaybeConstTypeHandle<> originType,
                                       bool useStrings)
{
  VariantAdapter adapter(variant, originType.has_value() ? originType.value().type() : nullptr, useStrings);
  targetType.accept(adapter);
  return adapter.getResult();
}

}  // namespace sen::impl
