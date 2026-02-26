// === type_specs_utils.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/type_specs_utils.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/span.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/unit.h"
#include "sen/core/meta/unit_registry.h"
#include "sen/core/meta/variant_type.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/sen/kernel/type_specs.stl.h"

// std
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace sen::kernel
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

constexpr auto allParents = ClassType::SearchMode::includeParents;

std::string logIncompatible(const std::string& name)
{
  std::string result = "Type with name ";
  result.append(name);
  result.append(" has an incompatible remote implementation. ");
  return result;
}

std::string_view getQualifiedTypeName(const ::sen::Type& type)
{
  if (auto* custom = type.asCustomType(); custom != nullptr)
  {
    return custom->getQualifiedName();
  }

  return type.getName();
}

UnitCat getUnitCat(UnitCategory category)
{
  switch (category)
  {
    case UnitCategory::length:
      return UnitCat::length;
    case UnitCategory::mass:
      return UnitCat::mass;
    case UnitCategory::time:
      return UnitCat::time;
    case UnitCategory::angle:
      return UnitCat::angle;
    case UnitCategory::temperature:
      return UnitCat::temperature;
    case UnitCategory::frequency:
      return UnitCat::frequency;
    case UnitCategory::velocity:
      return UnitCat::velocity;
    case UnitCategory::angularVelocity:
      return UnitCat::angularVelocity;
    case UnitCategory::acceleration:
      return UnitCat::acceleration;
    case UnitCategory::angularAcceleration:
      return UnitCat::angularAcceleration;
    case UnitCategory::density:
      return UnitCat::density;
    case UnitCategory::pressure:
      return UnitCat::pressure;
    case UnitCategory::area:
      return UnitCat::area;
    case UnitCategory::force:
      return UnitCat::force;
    default:
      break;
  }

  throw std::logic_error("unhandled unit category");
}

UnitInfo getUnitInfo(std::optional<const ::sen::Unit*> unit)
{
  UnitInfo info;
  if (unit)
  {
    info.name = unit.value()->getName();
    info.abbreviation = unit.value()->getAbbreviation();
    info.category = getUnitCat(unit.value()->getCategory());
  }
  else
  {
    info.name = "";
    info.abbreviation = "none";
    info.category = getUnitCat(UnitCategory::length);
  }

  return info;
}

PropertyCategorySpec getPropertyCategory(::sen::PropertyCategory category)
{
  switch (category)
  {
    case PropertyCategory::staticRO:
      return PropertyCategorySpec::staticRO;
    case PropertyCategory::staticRW:
      return PropertyCategorySpec::staticRW;
    case PropertyCategory::dynamicRW:
      return PropertyCategorySpec::dynamicRW;
    case PropertyCategory::dynamicRO:
      return PropertyCategorySpec::dynamicRO;
    default:
      break;
  }

  throw std::logic_error("unhandled property category");
}

::sen::PropertyCategory getPropertyCategory(PropertyCategorySpec category)
{
  switch (category)
  {
    case PropertyCategorySpec::staticRW:
      return ::sen::PropertyCategory::staticRW;
    case PropertyCategorySpec::staticRO:
      return ::sen::PropertyCategory::staticRO;
    case PropertyCategorySpec::dynamicRW:
      return ::sen::PropertyCategory::dynamicRW;
    case PropertyCategorySpec::dynamicRO:
      return ::sen::PropertyCategory::dynamicRO;
    default:
      break;
  }

  throw std::logic_error("unhandled property category");
}

MethodConstnessSpec getConstness(::sen::Constness constness)
{
  switch (constness)
  {
    case ::sen::Constness::constant:
      return MethodConstnessSpec::constant;
    case ::sen::Constness::nonConstant:
      return MethodConstnessSpec::nonConstant;
    default:
      break;
  }

  throw std::logic_error("unhandled method constness");
}

::sen::Constness getConstness(MethodConstnessSpec constness)
{
  switch (constness)
  {
    case MethodConstnessSpec::constant:
      return ::sen::Constness::constant;
    case MethodConstnessSpec::nonConstant:
      return ::sen::Constness::nonConstant;
    default:
      break;
  }

  throw std::logic_error("unhandled method constness");
}

TransportModeSpec getTransportMode(::sen::TransportMode mode)
{
  switch (mode)
  {
    case TransportMode::confirmed:
      return TransportModeSpec::confirmed;
    case TransportMode::multicast:
      return TransportModeSpec::multicast;
    case TransportMode::unicast:
      return TransportModeSpec::unicast;
    default:
      break;
  }

  throw std::logic_error("unhandled transport mode");
}

PropertyRelationSpec toPropertyRelationSpec(::sen::PropertyRelation relation)
{
  return std::visit(
    ::sen::Overloaded {[](const NonPropertyRelated& /**/) { return PropertyRelationSpec::nonPropertyRelated; },
                       [](const PropertyGetter& /**/) { return PropertyRelationSpec::propertyGetter; },
                       [](const PropertySetter& /**/) { return PropertyRelationSpec::propertySetter; }},
    relation);
}

::sen::PropertyRelation toPropertyRelation(PropertyRelationSpec spec)
{
  switch (spec)
  {
    case PropertyRelationSpec::nonPropertyRelated:
      return ::sen::PropertyRelation {::sen::NonPropertyRelated {}};
    case PropertyRelationSpec::propertyGetter:
      return ::sen::PropertyRelation {::sen::PropertyGetter {}};
    case PropertyRelationSpec::propertySetter:
      return ::sen::PropertyRelation {::sen::PropertySetter {}};
    default:
      break;
  }

  throw std::logic_error("unhandled property relation");
}

::sen::TransportMode getTransportMode(TransportModeSpec mode)
{
  switch (mode)
  {
    case TransportModeSpec::confirmed:
      return ::sen::TransportMode::confirmed;
    case TransportModeSpec::multicast:
      return ::sen::TransportMode::multicast;
    case TransportModeSpec::unicast:
      return ::sen::TransportMode::unicast;
    default:
      break;
  }

  throw std::logic_error("unhandled transport mode");
}

IntegralType getIntegralType(const ::sen::IntegralType& type)
{
  if (type.isUInt8Type())
  {
    return IntegralType::uint8Type;
  }
  if (type.isInt16Type())
  {
    return IntegralType::int16Type;
  }
  if (type.isUInt16Type())
  {
    return IntegralType::uint16Type;
  }
  if (type.isInt32Type())
  {
    return IntegralType::int32Type;
  }
  if (type.isUInt32Type())
  {
    return IntegralType::uint32Type;
  }
  if (type.isInt64Type())
  {
    return IntegralType::int64Type;
  }
  if (type.isUInt64Type())
  {
    return IntegralType::uint64Type;
  }

  {
    std::string err;
    err.append("unhandled integral type ");
    err.append(type.getName());
    throw std::logic_error(err);
  }
}

::sen::ConstTypeHandle<::sen::IntegralType> getIntegralType(IntegralType type)
{
  switch (type)
  {
    case IntegralType::uint8Type:
      return ::sen::UInt8Type::get();
    case IntegralType::int16Type:
      return ::sen::Int16Type::get();
    case IntegralType::uint16Type:
      return ::sen::UInt16Type::get();
    case IntegralType::int32Type:
      return ::sen::Int32Type::get();
    case IntegralType::uint32Type:
      return ::sen::UInt32Type::get();
    case IntegralType::int64Type:
      return ::sen::Int64Type::get();
    case IntegralType::uint64Type:
      return ::sen::UInt64Type::get();
    default:
      break;
  }

  throw std::logic_error("unhandled integral type");
}

[[nodiscard]] ::sen::ConstTypeHandle<::sen::RealType> getRealType(RealType type)
{
  switch (type)
  {
    case RealType::float32Type:
      return ::sen::Float32Type::get();
    case RealType::float64Type:
      return ::sen::Float64Type::get();
    default:
      break;
  }

  throw std::logic_error("unhandled real type");
}

NumericType getNumericType(const ::sen::NumericType& type)
{
  if (auto* integral = type.asIntegralType(); integral != nullptr)
  {
    return getIntegralType(*integral);
  }

  if (auto* float32Type = type.asFloat32Type(); float32Type != nullptr)
  {
    return RealType::float32Type;
  }

  if (auto* float64Type = type.asFloat64Type(); float64Type != nullptr)
  {
    return RealType::float64Type;
  }

  std::string err;
  err.append("unhandled numeric type ");
  err.append(type.getName());
  throw std::logic_error(err);
}

::sen::ConstTypeHandle<::sen::NumericType> getNumericType(const NumericType& type)
{
  return std::visit(Overloaded {[](const IntegralType& integralType) -> ::sen::ConstTypeHandle<::sen::NumericType>
                                { return getIntegralType(integralType); },
                                [](const RealType& realType) -> ::sen::ConstTypeHandle<::sen::NumericType>
                                { return getRealType(realType); }},
                    type);
}

ArgSpecList getArgs(Span<const Arg> args)
{
  ArgSpecList result;
  result.reserve(args.size());

  for (const auto& elem: args)
  {
    ArgSpec spec;
    {
      spec.name = elem.name;
      spec.description = elem.description;
      spec.type = getQualifiedTypeName(*elem.type);
    }

    result.push_back(std::move(spec));
  }

  return result;
}

MethodSpec getMethodSpec(const ::sen::Method* method)
{
  MethodSpec methodSpec;
  {
    methodSpec.name = method->getName();
    methodSpec.description = method->getDescription();
    methodSpec.args = getArgs(method->getArgs());
    methodSpec.transportMode = getTransportMode(method->getTransportMode());
    methodSpec.constness = getConstness(method->getConstness());
    methodSpec.deferred = method->getDeferred();
    methodSpec.returnType = getQualifiedTypeName(*method->getReturnType());
    methodSpec.propertyRelation = toPropertyRelationSpec(method->getPropertyRelation());
    methodSpec.localOnly = method->getLocalOnly();
  }
  SEN_ASSERT(!methodSpec.returnType.empty());

  return methodSpec;
}

ConstTypeHandle<CustomType> makeNonNative(const CustomTypeSpec& typeData,
                                          const EnumTypeSpec& spec,
                                          const CustomTypeRegistry& nativeTypes,
                                          const CustomTypeRegistry& nonNativeTypes)
{
  std::ignore = nativeTypes;
  std::ignore = nonNativeTypes;

  std::vector<Enumerator> enums;
  enums.reserve(spec.enums.size());
  for (const auto& [name, key, description]: spec.enums)
  {
    Enumerator enumerator;
    enumerator.name = name;
    enumerator.key = key;
    enumerator.description = description;
    enums.push_back(std::move(enumerator));
  }

  ::sen::EnumSpec enumSpec(
    typeData.name, typeData.qualifiedName, typeData.description, std::move(enums), getIntegralType(spec.storageType));

  return EnumType::make(enumSpec);
}

ConstTypeHandle<CustomType> makeNonNative(const CustomTypeSpec& typeData,
                                          const QuantityTypeSpec& spec,
                                          const CustomTypeRegistry& nativeTypes,
                                          const CustomTypeRegistry& nonNativeTypes)
{
  std::ignore = nativeTypes;
  std::ignore = nonNativeTypes;

  QuantitySpec quantitySpec(typeData.name,
                            typeData.qualifiedName,
                            typeData.description,
                            getNumericType(spec.elementType),
                            UnitRegistry::get().searchUnitByName(spec.unit.name),
                            spec.minValue,
                            spec.maxValue);

  return QuantityType::make(quantitySpec);
}

[[nodiscard]] ConstTypeHandle<> findType(const std::string& qualifiedName,
                                         const CustomTypeRegistry& localRegistry,
                                         const CustomTypeRegistry& remoteRegistry)
{
  auto result = remoteRegistry.get(qualifiedName);
  if (result)
  {
    return std::move(result).value();
  }

  // search in the native types
  auto nativeTypes = getNativeTypes();
  for (const auto& elem: nativeTypes)
  {
    std::string nativeQualified = "sen.";
    nativeQualified.append(elem->getName());

    if (nativeQualified == qualifiedName)
    {
      return elem;
    }
  }

  result = localRegistry.get(qualifiedName);
  if (!result)
  {
    std::string err;
    err.append("could not find type '");
    err.append(qualifiedName);
    err.append("'");
    throwRuntimeError(err);
  }

  return std::move(result).value();
}

[[nodiscard]] std::vector<Arg> getArgs(const ArgSpecList& args,
                                       const CustomTypeRegistry& nativeTypes,
                                       const CustomTypeRegistry& nonNativeTypes)
{
  std::vector<Arg> result;
  result.reserve(args.size());

  for (const auto& [name, description, type]: args)
  {
    result.emplace_back(name, description, findType(type, nativeTypes, nonNativeTypes));
  }

  return result;
}

ConstTypeHandle<CustomType> makeNonNative(const CustomTypeSpec& typeData,
                                          const SequenceTypeSpec& spec,
                                          const CustomTypeRegistry& nativeTypes,
                                          const CustomTypeRegistry& nonNativeTypes)
{

  SequenceSpec sequenceSpec(typeData.name,
                            typeData.qualifiedName,
                            typeData.description,
                            findType(spec.elementType, nativeTypes, nonNativeTypes),
                            spec.maxSize,
                            spec.fixedSize);

  return SequenceType::make(sequenceSpec);
}

ConstTypeHandle<CustomType> makeNonNative(const CustomTypeSpec& typeData,
                                          const StructTypeSpec& spec,
                                          const CustomTypeRegistry& nativeTypes,
                                          const CustomTypeRegistry& nonNativeTypes)
{
  MaybeConstTypeHandle<StructType> parent;

  if (!spec.parent.empty())
  {
    auto parentType = findType(spec.parent, nativeTypes, nonNativeTypes);
    if (!parentType->isStructType())
    {
      std::string err;
      err.append("type '");
      err.append(spec.parent);
      err.append("' is not a valid parent for struct ");
      err.append(typeData.qualifiedName);
      throwRuntimeError(err);
    }
    parent = dynamicTypeHandleCast<const StructType>(parentType);
  }

  std::vector<StructField> fields;
  fields.reserve(spec.fields.size());
  for (const auto& [name, description, type]: spec.fields)
  {
    fields.emplace_back(name, description, findType(type, nativeTypes, nonNativeTypes));
  }

  StructSpec structSpec(
    typeData.name, typeData.qualifiedName, typeData.description, std::move(fields), std::move(parent));

  return StructType::make(structSpec);
}

ConstTypeHandle<CustomType> makeNonNative(const CustomTypeSpec& typeData,
                                          const VariantTypeSpec& spec,
                                          const CustomTypeRegistry& nativeTypes,
                                          const CustomTypeRegistry& nonNativeTypes)
{
  VariantSpec variantSpec;
  variantSpec.name = typeData.name;
  variantSpec.qualifiedName = typeData.qualifiedName;
  variantSpec.description = typeData.description;

  variantSpec.fields.reserve(spec.fields.size());
  for (const auto& [key, description, type]: spec.fields)
  {
    variantSpec.fields.emplace_back(key, description, findType(type, nativeTypes, nonNativeTypes));
  }

  return VariantType::make(variantSpec);
}

ConstTypeHandle<CustomType> makeNonNative(const CustomTypeSpec& typeData,
                                          const AliasTypeSpec& spec,
                                          const CustomTypeRegistry& nativeTypes,
                                          const CustomTypeRegistry& nonNativeTypes)
{
  AliasSpec aliasSpec(typeData.name,
                      typeData.qualifiedName,
                      typeData.description,
                      findType(spec.aliasedType, nativeTypes, nonNativeTypes));

  return AliasType::make(aliasSpec);
}

ConstTypeHandle<CustomType> makeNonNative(const CustomTypeSpec& typeData,
                                          const OptionalTypeSpec& spec,
                                          const CustomTypeRegistry& nativeTypes,
                                          const CustomTypeRegistry& nonNativeTypes)
{
  OptionalSpec optionalSpec(
    typeData.name, typeData.qualifiedName, typeData.description, findType(spec.type, nativeTypes, nonNativeTypes));

  return OptionalType::make(optionalSpec);
}

[[nodiscard]] ::sen::MethodSpec makeMethodSpec(const MethodSpec& spec,
                                               const CustomTypeRegistry& nativeTypes,
                                               const CustomTypeRegistry& nonNativeTypes)
{

  CallableSpec callableSpec;
  callableSpec.name = spec.name;
  callableSpec.description = spec.description;
  callableSpec.args = getArgs(spec.args, nativeTypes, nonNativeTypes);
  callableSpec.transportMode = getTransportMode(spec.transportMode);

  SEN_ASSERT(!spec.returnType.empty());

  ::sen::MethodSpec result(callableSpec,
                           findType(spec.returnType, nativeTypes, nonNativeTypes),
                           getConstness(spec.constness),
                           toPropertyRelation(spec.propertyRelation),
                           spec.deferred,
                           spec.localOnly);

  return result;
}

[[nodiscard]] ::sen::PropertySpec makePropertySpec(const PropertySpec& spec,
                                                   const CustomTypeRegistry& nativeTypes,
                                                   const CustomTypeRegistry& nonNativeTypes)
{
  return {spec.name,
          spec.description,
          findType(spec.type, nativeTypes, nonNativeTypes),
          getPropertyCategory(spec.category),
          getTransportMode(spec.transportMode),
          spec.checkedSet,
          spec.tags};
}

[[nodiscard]] ::sen::EventSpec makeEventSpec(const EventSpec& spec,
                                             const CustomTypeRegistry& nativeTypes,
                                             const CustomTypeRegistry& nonNativeTypes)
{
  ::sen::EventSpec result {};

  result.callableSpec.name = spec.name;
  result.callableSpec.description = spec.description;
  result.callableSpec.args = getArgs(spec.args, nativeTypes, nonNativeTypes);
  result.callableSpec.transportMode = getTransportMode(spec.transportMode);

  return result;
}

ConstTypeHandle<CustomType> makeNonNative(const CustomTypeSpec& typeData,
                                          const ClassTypeSpec& spec,
                                          const CustomTypeRegistry& nativeTypes,
                                          const CustomTypeRegistry& nonNativeTypes)
{
  ClassSpec classSpec {};

  classSpec.name = typeData.name;
  classSpec.qualifiedName = typeData.qualifiedName;
  classSpec.description = typeData.description;
  classSpec.isInterface = spec.isInterface;
  classSpec.constructor = makeMethodSpec(spec.constructor, nativeTypes, nonNativeTypes);

  // parents
  classSpec.parents.reserve(spec.parents.size());
  for (const auto& elem: spec.parents)
  {
    const auto type = findType(elem, nativeTypes, nonNativeTypes);
    if (!type->isClassType())
    {
      std::string err;
      err.append("type ");
      err.append(elem);
      err.append(" is not a valid parent for class ");
      err.append(typeData.qualifiedName);
      throwRuntimeError(err);
    }

    classSpec.parents.push_back(dynamicTypeHandleCast<const ClassType>(type).value());
  }

  // properties
  classSpec.properties.reserve(spec.properties.size());
  for (const auto& elem: spec.properties)
  {
    classSpec.properties.push_back(makePropertySpec(elem, nativeTypes, nonNativeTypes));
  }

  // methods
  classSpec.methods.reserve(spec.methods.size());
  for (const auto& elem: spec.methods)
  {
    classSpec.methods.push_back(makeMethodSpec(elem, nativeTypes, nonNativeTypes));
  }

  // events
  classSpec.events.reserve(spec.events.size());
  for (const auto& elem: spec.events)
  {
    classSpec.events.push_back(makeEventSpec(elem, nativeTypes, nonNativeTypes));
  }

  return ClassType::make(classSpec);
}

CustomTypeData makeCustomTypeData(const CustomType* type)
{
  class CustomTypeVisitor: public TypeVisitor
  {
  public:
    SEN_NOCOPY_NOMOVE(CustomTypeVisitor)

  public:
    explicit CustomTypeVisitor(CustomTypeData& result): result_(result) {}

    ~CustomTypeVisitor() override = default;

  public:
    using TypeVisitor::apply;

    // root type
    void apply(const Type& type) override
    {
      std::string err;
      err.append("unhandled custom type ");
      err.append(type.getName());
      throw std::logic_error(err);
    }

    void apply(const EnumType& type) override
    {
      EnumTypeSpec spec;
      spec.storageType = getIntegralType(type.getStorageType());

      for (const auto& [name, key, description]: type.getEnums())
      {
        EnumeratorSpec enumSpec;
        {
          enumSpec.name = name;
          enumSpec.key = key;
          enumSpec.description = description;
        }

        spec.enums.push_back(std::move(enumSpec));
      }

      result_.emplace<EnumTypeSpec>(std::move(spec));
    }

    // custom types
    void apply(const StructType& type) override
    {
      StructTypeSpec spec;

      auto parent = type.getParent();
      spec.parent = parent ? parent.value()->getQualifiedName() : "";

      for (const auto& [name, description, type]: type.getFields())
      {
        StructTypeFieldSpec fieldSpec;
        {
          fieldSpec.name = name;
          fieldSpec.description = description;
          fieldSpec.type = getQualifiedTypeName(*type);
        }

        spec.fields.push_back(std::move(fieldSpec));
      }

      result_.emplace<StructTypeSpec>(std::move(spec));
    }

    void apply(const VariantType& type) override
    {
      VariantTypeSpec spec;

      for (const auto& [key, description, type]: type.getFields())
      {
        VariantTypeFieldSpec fieldSpec;
        {
          fieldSpec.key = key;
          fieldSpec.description = description;
          fieldSpec.type = getQualifiedTypeName(*type);
        }

        spec.fields.push_back(std::move(fieldSpec));
      }

      result_.emplace<VariantTypeSpec>(std::move(spec));
    }

    void apply(const SequenceType& type) override
    {
      SequenceTypeSpec spec;
      {
        spec.elementType = getQualifiedTypeName(*type.getElementType());
        spec.maxSize = type.getMaxSize();
        spec.fixedSize = type.hasFixedSize();
      }

      result_.emplace<SequenceTypeSpec>(std::move(spec));
    }

    void apply(const ClassType& type) override
    {
      constexpr auto doNotSearchParents = ClassType::SearchMode::doNotIncludeParents;
      ClassTypeSpec spec;

      // basics
      spec.isInterface = type.isInterface();

      // parents
      for (const auto& elem: type.getParents())
      {
        spec.parents.emplace_back(elem->getQualifiedName());
      }

      // properties
      for (const auto& elem: type.getProperties(doNotSearchParents))
      {
        PropertySpec propertySpec;
        {
          propertySpec.name = elem->getName();
          propertySpec.description = elem->getDescription();
          propertySpec.category = getPropertyCategory(elem->getCategory());
          propertySpec.type = getQualifiedTypeName(*elem->getType());
          propertySpec.transportMode = getTransportMode(elem->getTransportMode());
          propertySpec.tags = elem->getTags();
          propertySpec.checkedSet = elem->getCheckedSet();
        }

        spec.properties.push_back(std::move(propertySpec));
      }

      // events
      for (const auto& elem: type.getEvents(doNotSearchParents))
      {
        EventSpec eventSpec;
        {
          eventSpec.name = elem->getName();
          eventSpec.description = elem->getDescription();
          eventSpec.args = getArgs(elem->getArgs());
          eventSpec.transportMode = getTransportMode(elem->getTransportMode());
        }

        spec.events.push_back(std::move(eventSpec));
      }

      // methods
      for (const auto& elem: type.getMethods(doNotSearchParents))
      {
        spec.methods.push_back(getMethodSpec(elem.get()));
      }

      // constructor
      spec.constructor = type.getConstructor() != nullptr ? getMethodSpec(type.getConstructor()) : MethodSpec {};

      result_.emplace<ClassTypeSpec>(std::move(spec));
    }

    void apply(const QuantityType& type) override
    {
      QuantityTypeSpec spec;
      {
        spec.elementType = getNumericType(*type.getElementType());
        spec.unit = getUnitInfo(type.getUnit());
        spec.minValue = type.getMinValue();
        spec.maxValue = type.getMaxValue();
      }

      result_.emplace<QuantityTypeSpec>(std::move(spec));
    }

    void apply(const AliasType& type) override
    {
      AliasTypeSpec spec;
      spec.aliasedType = getQualifiedTypeName(*type.getAliasedType());
      result_.emplace<AliasTypeSpec>(std::move(spec));
    }

    void apply(const OptionalType& type) override
    {
      OptionalTypeSpec spec;
      spec.type = getQualifiedTypeName(*type.getType());
      result_.emplace<OptionalTypeSpec>(std::move(spec));
    }

  private:
    CustomTypeData& result_;
  };

  CustomTypeData result;
  CustomTypeVisitor visitor(result);
  type->accept(visitor);
  return result;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Implementation
//--------------------------------------------------------------------------------------------------------------

CustomTypeSpec makeCustomTypeSpec(const CustomType* type)
{
  return {std::string(type->getName()),
          std::string(type->getQualifiedName()),
          std::string(type->getDescription()),
          makeCustomTypeData(type)};
}

bool equivalent(const Type* localType, const Type* remoteType)
{
  if (localType == nullptr || remoteType == nullptr)
  {
    return false;
  }

  return localType->getHash() == remoteType->getHash();
}

ConstTypeHandle<CustomType> buildNonNativeType(const CustomTypeSpec& remoteType,
                                               const CustomTypeRegistry& nativeTypes,
                                               const CustomTypeRegistry& nonNativeTypes)
{
  return std::visit(Overloaded {[&remoteType, &nativeTypes, &nonNativeTypes](const auto& spec)
                                { return makeNonNative(remoteType, spec, nativeTypes, nonNativeTypes); }},
                    remoteType.data);
}

/// Visitor class that notes the runtime differences between 2 meta types that are runtime-compatible but not equal
class RuntimeDifferencesVisitor final: public TypeVisitor
{
public:
  SEN_NOCOPY_NOMOVE(RuntimeDifferencesVisitor)

public:
  RuntimeDifferencesVisitor(const Type* remoteType, std::vector<std::string>& differences)
    : remoteType_(remoteType), differences_(differences)
  {
  }

  ~RuntimeDifferencesVisitor() override = default;

public:
  using TypeVisitor::apply;

  void apply(const EnumType& type) override
  {
    const auto& qualifiedName = type.getQualifiedName();

    if (!remoteType_->isEnumType())
    {
      std::string diff;
      {
        diff.append("The remote version of '");
        diff.append(qualifiedName);
        diff.append("', called '");
        diff.append(remoteType_->getName());
        diff.append("', is not an EnumType.");
      }

      differences_.emplace_back(diff);
      return;
    }

    const auto* remoteEnum = remoteType_->asEnumType();
    if (type.getEnums() != remoteEnum->getEnums())
    {
      std::string diff;
      {
        diff.append("The enumerators of the local and remote versions of ");
        diff.append(qualifiedName);
        diff.append(" are different.");
      }

      differences_.emplace_back(diff);
    }

    if (type.getStorageType() != remoteEnum->getStorageType())
    {
      std::string diff;
      {
        diff.append("The storage type of the local and remote versions of ");
        diff.append(qualifiedName);
        diff.append(" are different.");
      }

      differences_.emplace_back(diff);
    }
  }

  void apply(const StructType& type) override
  {
    auto qualifiedName = type.getQualifiedName();

    if (!remoteType_->isStructType())
    {
      std::string diff;
      {
        diff.append("The remote version of '");
        diff.append(qualifiedName);
        diff.append("', called '");
        diff.append(remoteType_->getName());
        diff.append("' is not a StructType.");
      }

      differences_.emplace_back(diff);
      return;
    }

    const auto* remoteStruct = remoteType_->asStructType();
    const auto& localFields = type.getFields();
    const auto& remoteFields = remoteStruct->getFields();

    const auto numLocalFields = localFields.size();
    const auto numRemoteFields = remoteFields.size();

    const auto& fields = numLocalFields >= numRemoteFields ? localFields : remoteFields;
    const auto* otherStruct = numLocalFields < numRemoteFields ? &type : remoteStruct;

    for (const auto& field: fields)
    {
      const auto* otherField = otherStruct->getFieldFromName(field.name);

      if (otherField == nullptr)
      {
        std::string diff;
        {
          diff.append("The '");
          diff.append(qualifiedName);
          diff.append(".");
          diff.append(field.name);
          diff.append("' struct field is not present in both versions of '");
          diff.append(qualifiedName);
          diff.append("'.");
        }

        differences_.emplace_back(diff);
        return;
      }

      if (field.type->getHash() != otherField->type->getHash())
      {
        std::string diff;
        {
          diff.append("The local and remote types of the '");
          diff.append(field.name);
          diff.append("' field of ");
          diff.append(qualifiedName);
          diff.append(" are different: ");
          diff.append(field.type->getName());
          diff.append(" : ");
          diff.append(otherField->type->getName());
        }

        differences_.emplace_back(diff);

        concatDifferences(getRuntimeDifferences(field.type.type(), otherField->type.type()));
      }
    }

    const auto& localParent = type.getParent();
    const auto& remoteParent = remoteStruct->getParent();
    if (localParent ? !remoteParent.has_value() ||
                        (remoteParent && localParent.value()->getHash() != remoteParent.value()->getHash())
                    : remoteParent.has_value())
    {
      if (localParent && remoteParent)
      {
        concatDifferences(getRuntimeDifferences(localParent.value().type(), remoteParent.value().type()));
      }
    }
  }

  void apply(const VariantType& type) override
  {
    auto qualifiedName = type.getQualifiedName();

    if (!remoteType_->isVariantType())
    {
      std::string diff;
      {
        diff.append("The remote version of '");
        diff.append(qualifiedName);
        diff.append("', called '");
        diff.append(remoteType_->getName());
        diff.append(" is not a VariantType.");
      }

      differences_.emplace_back(diff);
      return;
    }

    const auto* remoteVariant = remoteType_->asVariantType();
    const auto& localFields = type.getFields();
    const auto& remoteFields = remoteVariant->getFields();

    const auto numLocalFields = localFields.size();
    const auto numRemoteFields = remoteFields.size();

    const auto& fields = numLocalFields >= numRemoteFields ? localFields : remoteFields;
    const auto* otherVariant = numLocalFields < numRemoteFields ? &type : remoteVariant;

    for (const auto& field: fields)
    {
      const auto* otherField = otherVariant->getFieldFromKey(field.key);

      if (otherField == nullptr)
      {
        std::string diff;
        {
          diff.append("The '");
          diff.append(qualifiedName);
          diff.append(".");
          diff.append(std::to_string(field.key));
          diff.append("' variant field is not present in both versions of '");
          diff.append(qualifiedName);
          diff.append("'.");
        }

        differences_.emplace_back(diff);
        return;
      }

      if (field.type->getHash() != otherField->type->getHash())
      {
        std::string diff;
        {
          diff.append("The local and remote types of the '");
          diff.append(std::to_string(field.key));
          diff.append("' field of ");
          diff.append(qualifiedName);
          diff.append(" are different:");
        }

        differences_.emplace_back(diff);
      }

      concatDifferences(getRuntimeDifferences(field.type.type(), otherField->type.type()));
    }
  }

  void apply(const SequenceType& type) override
  {
    const auto& qualifiedName = type.getQualifiedName();

    if (!remoteType_->isSequenceType())
    {
      std::string diff;
      {
        diff.append("The remote version of '");
        diff.append(qualifiedName);
        diff.append("', called '");
        diff.append(remoteType_->getName());
        diff.append("' is not a SequenceType.");
      }

      differences_.emplace_back(diff);
      return;
    }

    const auto* remoteSequence = remoteType_->asSequenceType();
    if (type.getElementType()->getHash() != remoteSequence->getElementType()->getHash())
    {
      std::string diff;
      {
        diff.append("The local and remote versions of '");
        diff.append(qualifiedName);
        diff.append("' have a different element type:");
      }

      differences_.emplace_back(diff);
    }

    concatDifferences(getRuntimeDifferences(type.getElementType().type(), remoteSequence->getElementType().type()));

    if (type.getMaxSize() != remoteSequence->getMaxSize())
    {
      std::string diff;
      {
        diff.append("The max sizes of the local and remote versions of ");
        diff.append(qualifiedName);
        diff.append(" are different.");
      }

      differences_.emplace_back(diff);
    }

    if (type.hasFixedSize() != remoteSequence->hasFixedSize())
    {
      std::string diff;
      {
        diff.append("The fixed size flags of the local and remote versions of ");
        diff.append(qualifiedName);
        diff.append(" are different.");
      }

      differences_.emplace_back(diff);
    }
  }

  void apply(const ClassType& type) override
  {
    const auto& qualifiedName = type.getQualifiedName();

    if (!remoteType_->isClassType())
    {
      std::string diff;
      {
        diff.append("The remote version of '");
        diff.append(qualifiedName);
        diff.append("' called '");
        diff.append(remoteType_->getName());
        diff.append(" is not a ClassType.");
      }

      differences_.emplace_back(diff);
      return;
    }
    const auto* remoteClass = remoteType_->asClassType();

    // properties
    checkClassProperties(type, *remoteClass, qualifiedName);

    // methods
    checkClassMethods(type, *remoteClass, qualifiedName);

    // events
    checkClassEvents(type, *remoteClass, qualifiedName);

    // constructor
    {
      if (type.getConstructor()->getHash() != remoteClass->getConstructor()->getHash())
      {
        std::string diff;
        {
          diff.append("The local and remote versions of the class constructor '");
          diff.append(qualifiedName);
          diff.append(".");
          diff.append(type.getConstructor()->getName());
          diff.append("' are different.");
        }

        differences_.emplace_back(diff);
      }
    }

    // parents
    {
      for (const auto& parent: type.getParents())
      {

        if (const auto itr = std::find_if(remoteClass->getParents().begin(),
                                          remoteClass->getParents().end(),
                                          [&parent](const auto& remoteParent)
                                          { return remoteParent->getQualifiedName() == parent->getQualifiedName(); });
            itr != remoteClass->getParents().end())
        {
          const auto& remoteParent = *itr;
          if (parent->getHash() != remoteParent->getHash())
          {
            std::string diff;
            {
              diff.append("The local and remote versions of class '");
              diff.append(parent->getQualifiedName());
              diff.append("' are different. The differences are the following:");
            }

            differences_.emplace_back(diff);

            concatDifferences(getRuntimeDifferences(parent.type(), remoteParent.type()));
          }
        }
      }
    }
  }

  void apply(const QuantityType& type) override
  {
    const auto& qualifiedName = type.getQualifiedName();

    if (!remoteType_->isQuantityType())
    {
      std::string diff;
      {
        diff.append("The remote version of '");
        diff.append(qualifiedName);
        diff.append("', called '");
        diff.append(remoteType_->getName());
        diff.append("' is not a QuantityType.");
      }

      differences_.emplace_back(diff);
      return;
    }

    const auto* remoteQuantity = remoteType_->asQuantityType();
    if (type.getElementType()->getHash() != remoteQuantity->getElementType()->getHash())
    {
      std::string diff;
      {
        diff.append(" The local and remote versions of ");
        diff.append(qualifiedName);
        diff.append(" have a different element type:");
      }

      differences_.emplace_back(diff);
    }

    concatDifferences(getRuntimeDifferences(type.getElementType().type(), remoteQuantity->getElementType().type()));

    const auto localUnit = type.getUnit();
    const auto remoteUnit = remoteQuantity->getUnit();

    if (localUnit ? !remoteUnit || (remoteUnit && localUnit.value()->getHash() != remoteUnit.value()->getHash())
                  : remoteUnit.has_value())
    {
      std::string diff;
      {
        diff.append("The local and remote versions of ");
        diff.append(qualifiedName);
        diff.append(" have different units.");
      }

      differences_.emplace_back(diff);
    }

    if (type.getMinValue() != remoteQuantity->getMinValue())
    {
      std::string diff;
      {
        diff.append("The local and remote versions of ");
        diff.append(qualifiedName);
        diff.append(" have different minimum values.");
      }

      differences_.emplace_back(diff);
    }

    if (type.getMaxValue() != remoteQuantity->getMaxValue())
    {
      std::string diff;
      {
        diff.append("The local and remote versions of ");
        diff.append(qualifiedName);
        diff.append(" have different maximum values.");
      }

      differences_.emplace_back(diff);
    }
  }

  void apply(const AliasType& type) override
  {
    if (const auto* remoteAlias = remoteType_->asAliasType(); remoteAlias != nullptr)
    {
      concatDifferences(getRuntimeDifferences(type.getAliasedType().type(), remoteAlias));
      return;
    }

    concatDifferences(getRuntimeDifferences(type.getAliasedType().type(), remoteType_));
  }

  void apply(const OptionalType& type) override
  {
    const auto& qualifiedName = type.getQualifiedName();

    if (!remoteType_->isOptionalType())
    {
      std::string diff;
      {
        diff.append("The remote version of '");
        diff.append(qualifiedName);
        diff.append("', called '");
        diff.append(remoteType_->getName());
        diff.append(" is not a OptionalType.");
      }

      differences_.emplace_back(diff);
      return;
    }

    const auto* remoteOptional = remoteType_->asOptionalType();
    if (type.getType()->getHash() != remoteOptional->getType()->getHash())
    {
      std::string diff;
      {
        diff.append("The local and remote versions of ");
        diff.append(qualifiedName);
        diff.append(" have different types:");
      }

      differences_.emplace_back(diff);
    }

    concatDifferences(getRuntimeDifferences(type.getType().type(), remoteType_));
  }

private:
  // NOLINTNEXTLINE
  void concatDifferences(const std::vector<std::string>& differences)
  {
    differences_.insert(differences_.end(), differences.begin(), differences.end());
  }

  void checkClassProperties(const ClassType& localClass,
                            const ClassType& remoteClass,
                            const std::string_view& qualifiedName)
  {
    const auto& localProps = localClass.getProperties(ClassType::SearchMode::doNotIncludeParents);
    const auto& remoteProps = remoteClass.getProperties(ClassType::SearchMode::doNotIncludeParents);
    const auto numOfLocalProps = localProps.size();
    const auto numOfRemoteProps = remoteProps.size();
    const auto& props = numOfLocalProps >= numOfRemoteProps ? localProps : remoteProps;
    const auto& otherClass = numOfLocalProps < numOfRemoteProps ? localClass : remoteClass;

    for (const auto& prop: props)
    {
      const auto& propName = prop->getName();
      const auto* otherProp = otherClass.searchPropertyByName(propName, ClassType::SearchMode::doNotIncludeParents);

      if (otherProp == nullptr)
      {
        std::string diff;
        {
          diff.append("The remote or local version of ");
          diff.append(qualifiedName);
          diff.append(" does not have the '");
          diff.append(propName);
          diff.append("' property.");
        }

        differences_.emplace_back(diff);
        continue;
      }

      if (prop->getHash() != otherProp->getHash())
      {
        if (prop->getCategory() != otherProp->getCategory())
        {
          std::string diff;
          {
            diff.append("The local and remote versions of the property '");
            diff.append(qualifiedName);
            diff.append(".");
            diff.append(propName);
            diff.append("' have different property category.");
          }

          differences_.emplace_back(diff);
        }

        if (prop->getType()->getHash() != otherProp->getType()->getHash())
        {
          std::string diff;
          {
            diff.append("The local and remote versions of the property '");
            diff.append(qualifiedName);
            diff.append(".");
            diff.append(propName);
            diff.append("' have different types: ");
            diff.append(prop->getType()->getName());
            diff.append(" : ");
            diff.append(otherProp->getType()->getName());
          }

          differences_.emplace_back(diff);

          concatDifferences(getRuntimeDifferences(prop->getType().type(), otherProp->getType().type()));
        }

        if (prop->getTransportMode() != otherProp->getTransportMode())
        {
          std::string diff;
          {
            diff.append("The local and remote versions of the property '");
            diff.append(qualifiedName);
            diff.append(".");
            diff.append(propName);
            diff.append("' have different transport modes");
          }

          differences_.emplace_back(diff);
        }

        if (prop->getTags() != otherProp->getTags())
        {
          std::string diff;
          {
            diff.append("The local and remote versions of the property '");
            diff.append(qualifiedName);
            diff.append(".");
            diff.append(propName);
            diff.append("' have different tags");
          }

          differences_.emplace_back(diff);
        }

        if (prop->getCheckedSet() != otherProp->getCheckedSet())
        {
          std::string diff;
          {
            diff.append("The local and remote versions of the property '");
            diff.append(qualifiedName);
            diff.append(".");
            diff.append(propName);
            diff.append("' have different checked set flags");
          }

          differences_.emplace_back(diff);
        }
      }
    }
  }

  void checkCallableArgs(const Callable& localCallable,
                         const Callable& remoteCallable,
                         const std::string_view& qualifiedName)
  {
    const auto numOfLocalArgs = localCallable.getArgs().size();
    const auto numOfRemoteArgs = remoteCallable.getArgs().size();

    const auto& args = numOfLocalArgs >= numOfRemoteArgs ? localCallable.getArgs() : remoteCallable.getArgs();
    const auto& otherCallable = numOfLocalArgs < numOfRemoteArgs ? localCallable : remoteCallable;

    for (const auto& arg: args)
    {
      const auto* otherArg = otherCallable.getArgFromName(arg.name);
      if (otherArg == nullptr)
      {
        std::string diff;
        {
          diff.append("The argument '");
          diff.append(arg.name);
          diff.append("' of callable '");
          diff.append(qualifiedName);
          diff.append(".");
          diff.append(localCallable.getName());
          diff.append("' could not be found in both the local and remote versions of the callable.");
        }

        differences_.emplace_back(diff);
        continue;
      }

      if (arg.type->getHash() != otherArg->type->getHash())
      {
        std::string diff;
        {
          diff.append("The local and remote types of callable argument '");
          diff.append(qualifiedName);
          diff.append(".");
          diff.append(localCallable.getName());
          diff.append(".");
          diff.append(arg.name);
          diff.append("' are different:");
        }

        differences_.emplace_back(diff);

        concatDifferences(getRuntimeDifferences(arg.type.type(), otherArg->type.type()));
      }
    }
  }

  void checkClassMethods(const ClassType& localClass,
                         const ClassType& remoteClass,
                         const std::string_view& qualifiedName)
  {
    const auto& localMethods = localClass.getMethods(ClassType::SearchMode::doNotIncludeParents);
    const auto& remoteMethods = remoteClass.getMethods(ClassType::SearchMode::doNotIncludeParents);
    const auto numOfLocalMethods = localMethods.size();
    const auto numOfRemoteMethods = remoteMethods.size();
    const auto& methods = numOfLocalMethods >= numOfRemoteMethods ? localMethods : remoteMethods;
    const auto& otherClass = numOfLocalMethods < numOfRemoteMethods ? localClass : remoteClass;

    for (const auto& method: methods)
    {
      const auto& methodName = method->getName();
      const auto* otherMethod = otherClass.searchMethodByName(methodName, ClassType::SearchMode::doNotIncludeParents);

      if (otherMethod == nullptr)
      {
        std::string diff;
        {
          diff.append("The remote or local version of ");
          diff.append(qualifiedName);
          diff.append(" does not have the '");
          diff.append(methodName);
          diff.append("' method.");
        }

        differences_.emplace_back(diff);
        continue;
      }

      if (method->getHash() != otherMethod->getHash())
      {
        checkCallableArgs(*method, *otherMethod, qualifiedName);

        if (method->getTransportMode() != otherMethod->getTransportMode())
        {
          std::string diff;
          {
            diff.append("The remote or local versions of method '");
            diff.append(qualifiedName);
            diff.append(".");
            diff.append(methodName);
            diff.append("' have different transport modes.");
          }

          differences_.emplace_back(diff);
        }

        if (method->getConstness() != otherMethod->getConstness())
        {
          std::string diff;
          {
            diff.append("The remote or local versions of method '");
            diff.append(qualifiedName);
            diff.append(".");
            diff.append(methodName);
            diff.append("' have different constness.");
          }

          differences_.emplace_back(diff);
        }

        if (method->getDeferred() != otherMethod->getDeferred())
        {
          std::string diff;
          {
            diff.append("The remote or local versions of method '");
            diff.append(qualifiedName);
            diff.append(".");
            diff.append(methodName);
            diff.append("' have different deferred flags.");
          }

          differences_.emplace_back(diff);
        }

        const auto& returnType = method->getReturnType();
        const auto& otherReturnType = otherMethod->getReturnType();

        if (returnType->getHash() != otherReturnType->getHash())
        {
          std::string diff;
          {
            diff.append("The remote or local versions of method '");
            diff.append(qualifiedName);
            diff.append(".");
            diff.append(methodName);
            diff.append("' have different return types:");
          }

          differences_.emplace_back(diff);

          concatDifferences(getRuntimeDifferences(method->getReturnType().type(), otherMethod->getReturnType().type()));
        }

        if (method->getPropertyRelation().index() != otherMethod->getPropertyRelation().index())
        {
          std::string diff;
          {
            diff.append("The remote or local versions of method '");
            diff.append(qualifiedName);
            diff.append(".");
            diff.append(methodName);
            diff.append("' have different property relations.");
          }

          differences_.emplace_back(diff);
        }

        if (method->getLocalOnly() != otherMethod->getLocalOnly())
        {
          std::string diff;
          {
            diff.append("The remote or local versions of method '");
            diff.append(qualifiedName);
            diff.append(".");
            diff.append(methodName);
            diff.append("' have different local only flags.");
          }

          differences_.emplace_back(diff);
        }
      }
    }
  }

  void checkClassEvents(const ClassType& localClass,
                        const ClassType& remoteClass,
                        const std::string_view& qualifiedName)
  {
    const auto& localEvents = localClass.getEvents(ClassType::SearchMode::doNotIncludeParents);
    const auto& remoteEvents = remoteClass.getEvents(ClassType::SearchMode::doNotIncludeParents);
    const auto numOfLocalEvents = localEvents.size();
    const auto numOfRemoteEvents = remoteEvents.size();
    const auto& events = numOfLocalEvents >= numOfRemoteEvents ? localEvents : remoteEvents;
    const auto& otherClass = numOfLocalEvents < numOfRemoteEvents ? localClass : remoteClass;

    for (const auto& event: events)
    {
      const auto& eventName = event->getName();
      const auto* otherEvent = otherClass.searchEventByName(eventName, ClassType::SearchMode::doNotIncludeParents);

      if (otherEvent == nullptr)
      {
        std::string diff;
        {
          diff.append("The remote or local version of ");
          diff.append(qualifiedName);
          diff.append(" does not have the '");
          diff.append(eventName);
          diff.append("' event.");
        }

        differences_.emplace_back(diff);
        continue;
      }

      if (event->getHash() != otherEvent->getHash())
      {
        checkCallableArgs(*event, *otherEvent, qualifiedName);

        if (event->getTransportMode() != otherEvent->getTransportMode())
        {
          std::string diff;
          {
            diff.append("The remote or local versions of event '");
            diff.append(qualifiedName);
            diff.append(".");
            diff.append(eventName);
            diff.append("' have different transport modes.");
          }

          differences_.emplace_back(diff);
        }
      }
    }
  }

private:
  const Type* remoteType_;
  std::vector<std::string>& differences_;
};

std::vector<std::string> getRuntimeDifferences(const Type* localType, const Type* remoteType)
{
  std::vector<std::string> differences;
  RuntimeDifferencesVisitor visitor {remoteType, differences};
  localType->accept(visitor);
  return differences;
}

// NOLINTNEXTLINE
std::vector<std::string> runtimeCompatible(const Type* localType, const Type* remoteType)
{
  class RuntimeCompatVisitor: public FullTypeVisitor
  {
  public:
    SEN_NOCOPY_NOMOVE(RuntimeCompatVisitor)

  public:
    RuntimeCompatVisitor(const Type* remoteType, std::vector<std::string>& problems)
      : remoteType_(remoteType), problems_(problems)
    {
    }

    ~RuntimeCompatVisitor() override = default;

  public:
    using FullTypeVisitor::apply;

    // root type
    void apply(const Type& type) override { std::ignore = type; }

    void apply(const VoidType& type) override
    {
      std::ignore = type;

      if (!remoteType_->isVoidType())
      {
        problems_.emplace_back(logIncompatible(type.getName().data()));
      }
    }

    void apply(const NativeType& type) override
    {
      auto isCompatible = remoteType_->isNativeType() || remoteType_->isEnumType() || remoteType_->isQuantityType();

      // remote type is not native, not enum and not quantity
      if (!isCompatible)
      {
        problems_.emplace_back(logIncompatible(type.getName().data()));
      }
    }

    void apply(const ::sen::IntegralType& type) override { apply(static_cast<const NativeType&>(type)); }

    void apply(const ::sen::RealType& type) override { apply(static_cast<const NativeType&>(type)); }

    void apply(const ::sen::NumericType& type) override { apply(static_cast<const NativeType&>(type)); }

    void apply(const BoolType& type) override { apply(static_cast<const NativeType&>(type)); }

    void apply(const UInt8Type& type) override { apply(static_cast<const NativeType&>(type)); }

    void apply(const Int16Type& type) override { apply(static_cast<const ::sen::IntegralType&>(type)); }

    void apply(const UInt16Type& type) override { apply(static_cast<const ::sen::IntegralType&>(type)); }

    void apply(const Int32Type& type) override { apply(static_cast<const ::sen::IntegralType&>(type)); }

    void apply(const UInt32Type& type) override { apply(static_cast<const ::sen::IntegralType&>(type)); }

    void apply(const Int64Type& type) override { apply(static_cast<const ::sen::IntegralType&>(type)); }

    void apply(const UInt64Type& type) override { apply(static_cast<const ::sen::IntegralType&>(type)); }

    void apply(const Float32Type& type) override { apply(static_cast<const ::sen::RealType&>(type)); }

    void apply(const Float64Type& type) override { apply(static_cast<const ::sen::RealType&>(type)); }

    void apply(const DurationType& type) override
    {
      if (!remoteType_->isDurationType())
      {
        problems_.emplace_back(logIncompatible(type.getName().data()));
      }
    }

    void apply(const TimestampType& type) override
    {
      if (!remoteType_->isTimestampType())
      {
        problems_.emplace_back(logIncompatible(type.getName().data()));
      }
    }

    void apply(const StringType& type) override { apply(static_cast<const NativeType&>(type)); }

    // custom types
    void apply(const CustomType& type) override { apply(static_cast<const Type&>(type)); }

    void apply(const EnumType& type) override
    {
      // remote type is not native and not enum
      if (!remoteType_->isNativeType() && !remoteType_->isEnumType())
      {
        problems_.emplace_back(logIncompatible(type.getName().data()));
      }
    }

    void apply(const StructType& type) override
    {
      // remote type is not a struct
      if (!remoteType_->isStructType())
      {
        problems_.emplace_back(logIncompatible(type.getName().data()));
        return;
      }

      const auto* remoteStruct = remoteType_->asStructType();
      for (const auto& remoteField: remoteStruct->getAllFields())
      {
        if (const auto* localField = type.getFieldFromName(remoteField.name); localField != nullptr)
        {
          if (const auto fieldProblems = runtimeCompatible(localField->type.type(), remoteField.type.type());
              !fieldProblems.empty())
          {
            std::string error;
            {
              error.append("Found incompatible matching fields with name ");
              error.append(localField->name);
              error.append(" in struct type ");
              error.append(type.getName());
            }
            problems_.emplace_back(error);

            concatProblems(fieldProblems);
          }
        }
      }
    }

    void apply(const VariantType& type) override
    {
      // remote type is not a variant
      if (!remoteType_->isVariantType())
      {
        problems_.emplace_back(logIncompatible(type.getName().data()));
        return;
      }

      // TODO: review if we can accept types contained in the variant
      const auto* remoteVariant = remoteType_->asVariantType();

      for (std::size_t i = 0; i < std::min(type.getFields().size(), remoteVariant->getFields().size()); ++i)
      {
        const auto key = std_util::checkedConversion<uint32_t, std_util::ReportPolicyIgnore>(i);

        if (const auto fieldProblems = runtimeCompatible(type.getFieldFromKey(key)->type.type(),
                                                         remoteVariant->getFieldFromKey(key)->type.type());
            !fieldProblems.empty())
        {
          std::string error;
          {
            error.append("Found incompatible matching variant fields with key ");
            error.append(std::to_string(i));
            error.append(" in variant type ");
            error.append(type.getName());
          }
          problems_.emplace_back(error);

          concatProblems(fieldProblems);
        }
      }
    }

    void apply(const SequenceType& type) override
    {

      // remote type is not a sequence
      if (!remoteType_->isSequenceType())
      {
        problems_.emplace_back(logIncompatible(type.getName().data()));
        return;
      }

      const auto* remoteSequence = remoteType_->asSequenceType();
      // check the element type
      concatProblems(runtimeCompatible(type.getElementType().type(), remoteSequence->getElementType().type()));
    }

    // NOLINTNEXTLINE
    void apply(const ClassType& type) override
    {

      // remote is not a class
      if (!remoteType_->isClassType())
      {
        problems_.emplace_back(logIncompatible(type.getName().data()));
        return;
      }

      const auto* remoteClass = remoteType_->asClassType();
      // check properties
      for (const auto& remoteProp: remoteClass->getProperties(allParents))
      {
        if (const auto* localProp = type.searchPropertyByName(remoteProp->getName()); localProp != nullptr)
        {
          if (const auto propProblems = runtimeCompatible(localProp->getType().type(), remoteProp->getType().type());
              !propProblems.empty())
          {
            std::string error;
            {
              error.append("Found incompatible property with name ");
              error.append(localProp->getName());
              error.append(" in class ");
              error.append(type.getName());
              error.append(". Local type is ");
              error.append(localProp->getType()->getName());
              error.append(" and remote type is ");
              error.append(remoteProp->getType()->getName());
            }
            problems_.emplace_back(error);

            concatProblems(propProblems);
          }
        }
      }

      // check events
      for (const auto& remoteEvent: remoteClass->getEvents(allParents))
      {
        if (const auto* localEvent = type.searchEventByName(remoteEvent->getName()); localEvent != nullptr)
        {
          for (const auto& remoteArg: remoteEvent->getArgs())
          {
            if (const auto& localArg = localEvent->getArgFromName(remoteArg.name); localArg != nullptr)
            {
              if (const auto eventProblems = runtimeCompatible(localArg->type.type(), remoteArg.type.type());
                  !eventProblems.empty())
              {
                std::string error;
                {
                  error.append("Found incompatible argument with name ");
                  error.append(localArg->name);
                  error.append(" in event ");
                  error.append(localEvent->getName());
                  error.append(" in class ");
                  error.append(type.getName());
                  error.append(". Local type is ");
                  error.append(localArg->type->getName());
                  error.append(" and remote type is ");
                  error.append(remoteArg.type->getName());
                }
                problems_.emplace_back(error);

                concatProblems(eventProblems);
              }
            }
          }
        }
      }

      // check methods
      for (const auto& remoteMethod: remoteClass->getMethods(allParents))
      {
        if (const auto* localMethod = type.searchMethodByName(remoteMethod->getName()); localMethod != nullptr)
        {
          // check return type
          auto localReturnType = localMethod->getReturnType();
          auto remoteReturnType = remoteMethod->getReturnType();

          concatProblems(runtimeCompatible(localReturnType.type(), remoteReturnType.type()));

          // check args
          for (const auto& localArg: localMethod->getArgs())
          {
            if (const auto* remoteArg = remoteMethod->getArgFromName(localArg.name); remoteArg != nullptr)
            {
              if (const auto argProblems = runtimeCompatible(localArg.type.type(), remoteArg->type.type());
                  !argProblems.empty())
              {
                std::string error;
                {
                  error.append("Found incompatible argument with name ");
                  error.append(localArg.name);
                  error.append(" in method ");
                  error.append(localMethod->getName());
                  error.append(" in class ");
                  error.append(type.getName());
                  error.append(". Local type is ");
                  error.append(localArg.type->getName());
                  error.append(" and remote type is ");
                  error.append(remoteArg->type->getName());
                }
                problems_.emplace_back(error);

                concatProblems(argProblems);
              }
            }
          }
        }
      }
    }

    void apply(const QuantityType& type) override
    {
      // remote is another quantity
      if (const auto* remoteQuantity = remoteType_->asQuantityType(); remoteQuantity != nullptr)
      {
        const auto localUnit = type.getUnit();
        const auto remoteUnit = remoteQuantity->getUnit();

        if (!localUnit && remoteUnit)
        {
          std::string error;
          {
            error.append("Found incompatible local unit (non-dimensional) and remote unit ");
            error.append(remoteUnit.value()->getName());
            error.append(" in type ");
            error.append(type.getName());
          }

          problems_.emplace_back(error);
          return;
        }

        if (localUnit && !remoteUnit)
        {
          std::string error;
          {
            error.append("Found incompatible local unit ");
            error.append(type.getName());
            error.append(" and remote unit (non-dimensional) in type ");
            error.append(type.getName());
          }

          problems_.emplace_back(error);
          return;
        }

        if (localUnit)
        {
          const auto localCategory = localUnit.value()->getCategory();
          const auto remoteCategory = remoteUnit.value()->getCategory();

          if (localCategory != remoteCategory)
          {
            std::string error;
            {
              error.append("Found incompatible local unit category ");
              error.append(Unit::getCategoryString(localCategory));
              error.append(" and remote unit category ");
              error.append(Unit::getCategoryString(remoteCategory));
              error.append(" in type ");
              error.append(type.getName());
            }

            problems_.emplace_back(error);
          }
        }

        return;
      }

      // remote is not numeric or string
      if (!remoteType_->isNumericType() && !remoteType_->isStringType())
      {
        problems_.emplace_back(logIncompatible(type.getName().data()));
      }
    }

    void apply(const AliasType& type) override { apply(static_cast<const CustomType&>(type)); }

    void apply(const OptionalType& type) override
    {
      concatProblems(runtimeCompatible(type.getType().type(), remoteType_));
    }

  private:
    void concatProblems(const std::vector<std::string>& problems)
    {
      problems_.insert(problems_.end(), problems.begin(), problems.end());
    }

  private:
    const Type* remoteType_;
    std::vector<std::string>& problems_;
  };

  // always runtime compatible if equivalent
  if (equivalent(localType, remoteType))
  {
    return {};
  }

  // handle the case where the remote type is optional first
  if (const auto* optionalRemote = remoteType->asOptionalType(); optionalRemote != nullptr)
  {
    return runtimeCompatible(localType, optionalRemote->getType().type());
  }

  // remote type is a timestamp
  if (remoteType->isTimestampType() && !localType->isTimestampType())
  {
    return {logIncompatible(localType->getName().data())};
  }

  // remote type is a duration
  if (remoteType->isDurationType() && localType->isDurationType())
  {
    return {logIncompatible(localType->getName().data())};
  }

  std::vector<std::string> problems;
  RuntimeCompatVisitor visitor {remoteType, problems};
  localType->accept(visitor);
  return problems;
}

CustomTypeSpec toCurrentVersion(const CustomTypeSpecV4& v4)
{
  CustomTypeSpec result {};

  result.name = v4.name;
  result.qualifiedName = v4.qualifiedName;
  result.description = v4.description;

  std::visit(
    Overloaded {
      [&result](const EnumTypeSpecV4& v4Data)
      {
        EnumeratorSpecList enums {};
        {
          enums.reserve(v4Data.enums.size());
          for (const auto& [name, key]: v4Data.enums)
          {
            enums.emplace_back();
            enums.back() = {name, static_cast<u32>(key), ""};
          }
        }
        result.data = EnumTypeSpec {enums, v4Data.storageType};
      },
      [&result](const QuantityTypeSpecV4& v4Data)
      { result.data = QuantityTypeSpec {v4Data.numericType, v4Data.unit, v4Data.minValue, v4Data.maxValue}; },
      [&result](const SequenceTypeSpecV4& v4Data)
      { result.data = SequenceTypeSpec {v4Data.elementType, v4Data.maxSize, false}; },
      [&result](const StructTypeSpecV4& v4Data)
      {
        StructTypeFieldSpecList fields {};
        {
          fields.reserve(v4Data.fields.size());
          for (const auto& [name, description, type]: v4Data.fields)
          {
            fields.emplace_back();
            fields.back() = {name, description, type};
          }
        }
        result.data = StructTypeSpec {fields, v4Data.parent};
      },
      [&result](const VariantTypeSpecV4& v4Data)
      {
        VariantTypeFieldSpecList fields {};
        {
          fields.reserve(v4Data.fields.size());
          for (const auto& [key, description, type]: v4Data.fields)
          {
            fields.emplace_back();
            fields.back() = {key, description, type};
          }
        }
        result.data = VariantTypeSpec {fields};
      },
      [&result](const AliasTypeSpecV4& v4Data) { result.data = AliasTypeSpec {v4Data.aliasedType}; },
      [&result](const OptionalTypeSpecV4& v4Data) { result.data = OptionalTypeSpec {v4Data.type}; },
      [&result](const ClassTypeSpecV4& v4Data)
      {
        // properties
        PropertySpecList properties {};
        {
          properties.reserve(v4Data.properties.size());
          for (const auto& [memberHash, name, description, category, type, transportMode, tags]: v4Data.properties)
          {
            properties.emplace_back();
            properties.back() = {name, description, category, type, transportMode, tags, false};
          }
        }

        // methods
        MethodSpecList methods {};
        {
          methods.reserve(v4Data.methods.size());
          for (const auto& [memberHash, name, description, v4Args, transportMode, constness, deferred, returnTypeId]:
               v4Data.methods)
          {
            methods.emplace_back();

            ArgSpecList args {};
            {
              args.reserve(args.size());
              for (const auto& [name, description, type]: v4Args)
              {
                args.emplace_back();
                args.back() = {name, description, type};
              }
            }

            methods.back() = {name,
                              description,
                              args,
                              transportMode,
                              constness,
                              deferred,
                              returnTypeId,
                              PropertyRelationSpec::nonPropertyRelated,
                              false};
          }
        }

        // events
        EventSpecList events {};
        {
          events.reserve(v4Data.events.size());
          for (const auto& [memberHash, name, description, v4Args, transportMode]: v4Data.events)
          {
            events.emplace_back();

            ArgSpecList args {};
            {
              args.reserve(args.size());
              for (const auto& [name, description, type]: v4Args)
              {
                args.emplace_back();
                args.back() = {name, description, type};
              }
            }

            events.back() = {name, description, args, transportMode};
          }
        }

        MethodSpec constructor {};
        {
          ArgSpecList args {};
          {
            args.reserve(v4Data.constructor.args.size());
            for (const auto& [name, description, type]: v4Data.constructor.args)
            {
              args.emplace_back();
              args.back() = {name, description, type};
            }
          }

          constructor = {v4Data.constructor.name,
                         v4Data.constructor.description,
                         args,
                         v4Data.constructor.transportMode,
                         v4Data.constructor.constness,
                         v4Data.constructor.deferred,
                         v4Data.constructor.returnTypeId,
                         PropertyRelationSpec::nonPropertyRelated,
                         false};
        }

        result.data = ClassTypeSpec {properties, methods, events, constructor, v4Data.parents, v4Data.isInterface};
      },
    },
    v4.data);

  return result;
}

CustomTypeSpec toCurrentVersion(const CustomTypeSpecV5& v5)
{
  CustomTypeSpec result {};

  result.name = v5.name;
  result.qualifiedName = v5.qualifiedName;
  result.description = v5.description;

  std::visit(
    Overloaded {
      [&result](const EnumTypeSpecV5& v5Data)
      {
        EnumeratorSpecList enums {};
        {
          enums.reserve(v5Data.enums.size());
          for (const auto& [name, key, hash]: v5Data.enums)
          {
            enums.emplace_back();
            enums.back() = {name, static_cast<u32>(key), ""};
          }
        }
        result.data = EnumTypeSpec {enums, v5Data.storageType};
      },
      [&result](const QuantityTypeSpecV5& v5Data)
      { result.data = QuantityTypeSpec {v5Data.numericType, v5Data.unit, v5Data.minValue, v5Data.maxValue}; },
      [&result](const SequenceTypeSpecV5& v5Data)
      { result.data = SequenceTypeSpec {v5Data.elementType, v5Data.maxSize, v5Data.fixedSize}; },
      [&result](const StructTypeSpecV5& v5Data)
      {
        StructTypeFieldSpecList fields {};
        {
          fields.reserve(v5Data.fields.size());
          for (const auto& [name, description, type, hash]: v5Data.fields)
          {
            fields.emplace_back();
            fields.back() = {name, description, type};
          }
        }
        result.data = StructTypeSpec {fields, v5Data.parent};
      },
      [&result](const VariantTypeSpecV5& v5Data)
      {
        VariantTypeFieldSpecList fields {};
        {
          fields.reserve(v5Data.fields.size());
          for (const auto& [key, description, type, hash]: v5Data.fields)
          {
            fields.emplace_back();
            fields.back() = {key, description, type};
          }
        }
        result.data = VariantTypeSpec {fields};
      },
      [&result](const AliasTypeSpecV5& v5Data) { result.data = AliasTypeSpec {v5Data.aliasedType}; },
      [&result](const OptionalTypeSpecV5& v5Data) { result.data = OptionalTypeSpec {v5Data.type}; },
      [&result](const ClassTypeSpecV5& v5Data)
      {
        // properties
        PropertySpecList properties {};
        {
          properties.reserve(v5Data.properties.size());
          for (const auto& [memberHash, name, description, category, type, transportMode, tags, hash]:
               v5Data.properties)
          {
            properties.emplace_back();
            properties.back() = {name, description, category, type, transportMode, tags, false};
          }
        }

        // methods
        MethodSpecList methods {};
        {
          methods.reserve(v5Data.methods.size());
          for (const auto& [memberHash,
                            name,
                            description,
                            v5Args,
                            transportMode,
                            constness,
                            deferred,
                            returnTypeId,
                            hash]: v5Data.methods)
          {
            methods.emplace_back();

            ArgSpecList args {};
            {
              args.reserve(args.size());
              for (const auto& [name, nameHash, description, type, hash]: v5Args)
              {
                args.emplace_back();
                args.back() = {name, description, type};
              }
            }

            methods.back() = {name,
                              description,
                              args,
                              transportMode,
                              constness,
                              deferred,
                              returnTypeId,
                              PropertyRelationSpec::nonPropertyRelated,
                              false};
          }
        }

        // events
        EventSpecList events {};
        {
          events.reserve(v5Data.events.size());
          for (const auto& [memberHash, name, description, v5Args, transportMode, hash]: v5Data.events)
          {
            events.emplace_back();

            ArgSpecList args {};
            {
              args.reserve(args.size());
              for (const auto& [name, nameHash, description, type, hash]: v5Args)
              {
                args.emplace_back();
                args.back() = {name, description, type};
              }
            }

            events.back() = {name, description, args, transportMode};
          }
        }

        MethodSpec constructor {};
        {
          ArgSpecList args {};
          {
            args.reserve(v5Data.constructor.args.size());
            for (const auto& [name, nameHash, description, type, hash]: v5Data.constructor.args)
            {
              args.emplace_back();
              args.back() = {name, description, type};
            }
          }

          constructor = {v5Data.constructor.name,
                         v5Data.constructor.description,
                         args,
                         v5Data.constructor.transportMode,
                         v5Data.constructor.constness,
                         false,
                         v5Data.constructor.returnTypeId,
                         PropertyRelationSpec::nonPropertyRelated,
                         false};
        }

        result.data = ClassTypeSpec {properties, methods, events, constructor, v5Data.parents, v5Data.isInterface};
      },
    },
    v5.data);

  return result;
}

}  // namespace sen::kernel
