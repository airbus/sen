// === json_type_storage.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "json_type_storage.h"

// app
#include "util.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/span.h"
#include "sen/core/lang/stl_resolver.h"
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
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/variant_type.h"

// inja
#include <inja/json.hpp>

// std
#include <algorithm>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace
{

using TypeNames = std::tuple<std::string, std::string, std::string>;

//--------------------------------------------------------------------------------------------------------------
// CppTypeName
//--------------------------------------------------------------------------------------------------------------

class CppTypeName final: protected sen::TypeVisitor
{
  SEN_NOCOPY_NOMOVE(CppTypeName)

public:
  static TypeNames get(const sen::Type& type, const std::string& currentNamespace)
  {
    CppTypeName visitor(currentNamespace);
    type.accept(visitor);
    return {visitor.absolute_, visitor.relative_, visitor.stlRelative_};
  }

protected:
  void apply(const sen::Type& type) final
  {
    std::ignore = type;
    throw std::logic_error("unhandled type");
  }

  void apply(const sen::VoidType& type) final
  {
    absolute_ = type.getName();
    relative_ = absolute_;
    stlRelative_ = absolute_;
  }

  void apply(const sen::NativeType& type) final
  {
    absolute_ = type.getName();
    relative_ = absolute_;
    stlRelative_ = absolute_;
  }

  void apply(const sen::CustomType& type) final
  {
    std::string typeNamespace {type.getQualifiedName()};

    auto lastDot = typeNamespace.find_last_of('.');
    if (lastDot != std::string::npos)
    {
      typeNamespace = typeNamespace.substr(0U, lastDot);

      // replace '.' by '::'
      const std::string from = ".";
      const std::string to = "::";
      size_t pos = 0U;
      while ((pos = typeNamespace.find(from, pos)) != std::string::npos)
      {
        typeNamespace.replace(pos, from.length(), to);
        pos += to.length();
      }

      absolute_ = std::string("::") + typeNamespace + "::" + std::string(type.getName());
      relative_ = typeNamespace == currentNamespace_ ? type.getName() : absolute_;
      stlRelative_ = typeNamespace == currentNamespace_ ? type.getName() : type.getQualifiedName();
    }
    else
    {
      absolute_ = std::string("::") + std::string(type.getQualifiedName());
      relative_ = absolute_;
      stlRelative_ = absolute_;
    }
  }

  void apply(const sen::StringType& type) final
  {
    std::ignore = type;
    absolute_ = "std::string";
    relative_ = absolute_;
    stlRelative_ = type.getName();
  }

  void apply(const sen::TimestampType& type) final
  {
    absolute_ = "::sen::TimeStamp";
    relative_ = absolute_;
    stlRelative_ = type.getName();
  }

  void apply(const sen::DurationType& type) final
  {
    absolute_ = "::sen::Duration";
    relative_ = absolute_;
    stlRelative_ = type.getName();
  }

private:
  explicit CppTypeName(const std::string& currentNamespace): currentNamespace_(currentNamespace) {}

  ~CppTypeName() final = default;

private:
  const std::string& currentNamespace_;
  std::string absolute_;
  std::string relative_;
  std::string stlRelative_;
};

//--------------------------------------------------------------------------------------------------------------
// PythonAnnotationName
//--------------------------------------------------------------------------------------------------------------

class PythonAnnotationName final: protected sen::TypeVisitor
{
  SEN_NOCOPY_NOMOVE(PythonAnnotationName)

public:
  static std::string get(const sen::Type& type)
  {
    PythonAnnotationName visitor;
    type.accept(visitor);
    return visitor.result_;
  }

protected:
  void apply(const sen::Type& type) final
  {
    std::ignore = type;
    throw std::logic_error("unhandled type");
  }

  void apply(const sen::VoidType& type) final
  {
    std::ignore = type;
    result_ = "NoneType";
  }

  void apply(const sen::BoolType& type) final
  {
    std::ignore = type;
    result_ = "bool";
  }

  void apply(const sen::IntegralType& type) final
  {
    std::ignore = type;
    result_ = "int";
  }

  void apply(const sen::RealType& type) final
  {
    std::ignore = type;
    result_ = "float";
  }

  void apply(const sen::DurationType& type) final
  {
    std::ignore = type;
    result_ = "timedelta";
  }

  void apply(const sen::TimestampType& type) final
  {
    std::ignore = type;
    result_ = "datetime";
  }

  void apply(const sen::StringType& type) final
  {
    std::ignore = type;
    result_ = "str";
  }

  void apply(const sen::CustomType& type) final { result_ = type.getName(); }

private:
  explicit PythonAnnotationName() = default;

  ~PythonAnnotationName() final = default;

private:
  std::string result_;
};

//--------------------------------------------------------------------------------------------------------------
// JSONSchemaRef
//--------------------------------------------------------------------------------------------------------------

class JSONSchemaRef final: protected sen::TypeVisitor
{
  SEN_NOCOPY_NOMOVE(JSONSchemaRef)

public:
  static std::string get(const sen::Type& type)
  {
    JSONSchemaRef visitor;
    type.accept(visitor);
    return visitor.result_;
  }

protected:
  void apply(const sen::Type& type) final
  {
    std::ignore = type;
    throw std::logic_error("unhandled type");
  }

  void apply(const sen::VoidType& type) final
  {
    std::ignore = type;
    result_ = R"("type": "void")";
  }

  void apply(const sen::BoolType& type) final
  {
    std::ignore = type;
    result_ = R"("type": "boolean")";
  }

  void apply(const sen::IntegralType& type) final
  {
    std::ignore = type;
    result_ = R"("type": "integer")";
  }

  void apply(const sen::RealType& type) final
  {
    std::ignore = type;
    result_ = R"("type": "number")";
  }

  void apply(const sen::DurationType& type) final
  {
    std::ignore = type;
    result_ = R"("type": "string")";
  }

  void apply(const sen::TimestampType& type) final
  {
    std::ignore = type;
    result_ = R"("type": "string")";
  }

  void apply(const sen::StringType& type) final
  {
    std::ignore = type;
    result_ = R"("type": "string")";
  }

  void apply(const sen::CustomType& type) final
  {
    result_ = R"("$ref": "#/$defs/)";
    result_.append(type.getQualifiedName());
    result_.append("\"");
  }

private:
  explicit JSONSchemaRef() = default;

  ~JSONSchemaRef() final = default;

private:
  std::string result_;
};

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

std::string getSenQual(const sen::Type& type)
{
  if (type.isCustomType())
  {
    return std::string(type.asCustomType()->getQualifiedName());
  }
  return std::string(type.getName());
}

std::string qualLocalName(const sen::ClassType& type, const std::string& currentNamespace)
{
  auto [absolute, relative, senRel] = CppTypeName::get(type, currentNamespace);
  std::ignore = relative;
  std::ignore = senRel;
  return absolute + "Base";
}

bool passByRef(const sen::Type& type) noexcept
{
  return (type.isCustomType() && !type.isQuantityType()) || type.isStringType();
}

bool allParentsAreInterfaces(const sen::ClassType& type)
{
  const auto& parents = type.getParents();
  for (const auto& parent: parents)
  {
    if (!parent->isInterface())
    {
      return false;
    }

    if (!allParentsAreInterfaces(*parent))
    {
      return false;
    }
  }

  return true;
}

bool hasLocalProperties(const sen::ClassType& type)
{
  constexpr auto mode = sen::ClassType::SearchMode::doNotIncludeParents;
  return !type.getProperties(mode).empty();
}

bool hasLocalDynamicProperties(const sen::ClassType& type)
{
  constexpr auto mode = sen::ClassType::SearchMode::doNotIncludeParents;
  auto list = type.getProperties(mode);
  for (const auto& prop: list)
  {
    if (prop->getCategory() == sen::PropertyCategory::dynamicRO ||
        prop->getCategory() == sen::PropertyCategory::dynamicRW)
    {
      return true;
    }
  }
  return false;
}

bool hasLocalEvents(const sen::ClassType& type)
{
  constexpr auto mode = sen::ClassType::SearchMode::doNotIncludeParents;
  return !type.getEvents(mode).empty();
}

bool hasLocalPropertiesOrEvents(const sen::ClassType& type) { return hasLocalProperties(type) || hasLocalEvents(type); }

std::string interfaceName(const sen::ClassType& type) { return std::string(type.getName()) + "Interface"; }

std::string qualInterfaceName(const sen::ClassType& type, const std::string& currentNamespace)
{
  auto [absolute, relative, senRel] = CppTypeName::get(type, currentNamespace);
  std::ignore = absolute;
  return relative + "Interface";
}

std::string localName(const sen::ClassType& type) { return std::string(type.getName()) + "Base"; }

std::string remoteName(const sen::ClassType& type) { return std::string(type.getName()) + "RemoteProxy"; }

std::string localProxyName(const sen::ClassType& type) { return std::string(type.getName()) + "LocalProxy"; }

std::string qualRemoteName(const sen::ClassType& type, const std::string& currentNamespace)
{
  auto [absolute, relative, senRel] = CppTypeName::get(type, currentNamespace);
  std::ignore = absolute;
  return relative + "RemoteProxy";
}

std::string qualLocalProxyName(const sen::ClassType& type, const std::string& currentNamespace)
{
  auto [absolute, relative, senRel] = CppTypeName::get(type, currentNamespace);
  std::ignore = absolute;
  return relative + "LocalProxy";
}

std::string getTransportEnumString(const sen::TransportMode mode)
{
  switch (mode)
  {
    case sen::TransportMode::unicast:
      return "::sen::TransportMode::unicast";

    case sen::TransportMode::multicast:
      return "::sen::TransportMode::multicast";

    case sen::TransportMode::confirmed:
      return "::sen::TransportMode::confirmed";

    default:
      SEN_UNREACHABLE();
  }
}

void addIsTypeInfo(inja::json& data, const sen::Type* type)
{
  data["isStructType"] = type->isStructType();
  data["isNativeType"] = type->isNativeType();
  data["isVariantType"] = type->isVariantType();
  data["isDurationType"] = type->isDurationType();
  data["isQuantityType"] = type->isQuantityType() && !type->isTimestampType();
  data["isEnum"] = type->isEnumType();
}

void replaceAll(std::string& str, const std::string& from, const std::string& to)
{
  if (from.empty())
  {
    return;
  }

  size_t startPos = 0;
  while ((startPos = str.find(from, startPos)) != std::string::npos)
  {
    str.replace(startPos, from.length(), to);
    startPos += to.length();  // In case 'to' contains 'from', like replacing 'x' with 'yx'
  }
}

[[nodiscard]] std::string escapeInvalidStringCharacters(std::string_view view)
{
  std::string string(view);
  replaceAll(string, "\"", "\\\"");
  return string;
}

[[nodiscard]] bool hasCheckedProperties(const sen::ClassType& type, sen::ClassType::SearchMode searchMode)
{
  for (const auto& prop: type.getProperties(searchMode))
  {
    if (prop->getCheckedSet())
    {
      return true;
    }
  }
  return false;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// JsonTypeStorage
//--------------------------------------------------------------------------------------------------------------

JsonTypeStorage::JsonTypeStorage(const sen::lang::TypeSet& typeSet, bool publicSymbols) noexcept
  : typeSet_(typeSet), nameSpace_(computeCppNamespace(typeSet)), publicSymbols_(publicSymbols)
{
}

std::tuple<std::string, std::string, std::string> JsonTypeStorage::cppTypeName(const sen::Type& type) const
{
  return CppTypeName::get(type, nameSpace_);
}

template <typename T>
inja::json JsonTypeStorage::toJsonList(const sen::Span<T>& list) const
{
  std::vector<inja::json> jsonList;
  jsonList.reserve(list.size());

  for (const auto& element: list)
  {
    jsonList.push_back(toJson(element));
  }
  return jsonList;
}

inja::json JsonTypeStorage::toJson(const sen::StructField& field) const
{
  auto [absolute, relative, senRel] = cppTypeName(*field.type);

  auto fieldType = field.type->isAliasType() ? field.type->asAliasType()->getAliasedType() : field.type;

  inja::json data;
  data["type"] = relative;
  data["qualType"] = absolute;
  data["senType"] = senRel;
  data["senQual"] = getSenQual(*field.type);
  data["pyAnnotationType"] = PythonAnnotationName::get(*field.type);
  data["jsonSchemaType"] = JSONSchemaRef::get(*field.type);
  data["name"] = field.name;
  data["description"] = escapeInvalidStringCharacters(field.description);
  data["requiresBlockComment"] = field.description.size() > impl::maxCommentLineLength;
  addIsTypeInfo(data, fieldType.type());
  data["isEnum"] = fieldType->isEnumType();
  data["ostreamNeedsNewline"] =
    (fieldType->isStructType() || fieldType->isVariantType() || fieldType->isSequenceType());

  return data;
}

inja::json JsonTypeStorage::toJson(const sen::Enumerator& enumerator) const
{
  inja::json data;
  data["key"] = enumerator.key;
  data["name"] = enumerator.name;
  data["description"] = escapeInvalidStringCharacters(enumerator.description);
  data["requiresBlockComment"] = (enumerator.description.size() > impl::maxCommentLineLength);

  return data;
}

inja::json JsonTypeStorage::toJson(const sen::VariantField& field) const
{
  auto [absolute, relative, senRel] = cppTypeName(*field.type);

  auto fieldType = field.type->isAliasType() ? field.type->asAliasType()->getAliasedType() : field.type;

  inja::json data;
  data["type"] = relative;
  data["key"] = field.key;
  data["qualType"] = absolute;
  data["senType"] = field.type->getName();
  data["senQual"] = getSenQual(*field.type);
  data["isTypeAlias"] = field.type->isAliasType();
  data["senAliasedType"] = field.type->isAliasType() ? field.type->asAliasType()->getAliasedType()->getName() : "";
  data["pyAnnotationType"] = PythonAnnotationName::get(*field.type);
  data["jsonSchemaType"] = JSONSchemaRef::get(*field.type);
  data["description"] = escapeInvalidStringCharacters(field.description);
  addIsTypeInfo(data, fieldType.type());
  data["requiresBlockComment"] = (field.description.size() > impl::maxCommentLineLength);
  data["ostreamNeedsNewline"] =
    (fieldType->isStructType() || fieldType->isVariantType() || fieldType->isSequenceType());

  return data;
}

inja::json JsonTypeStorage::toJson(const sen::StructType& type) const
{
  auto [absolute, relative, senRel] = cppTypeName(type);

  inja::json data;
  data["name"] = type.getName();
  data["qualType"] = absolute;
  data["senType"] = senRel;
  data["senQual"] = getSenQual(type);
  data["pyAnnotationType"] = PythonAnnotationName::get(type);
  data["jsonSchemaType"] = JSONSchemaRef::get(type);
  data["description"] = escapeInvalidStringCharacters(type.getDescription());
  data["fields"] = toJsonList(type.getFields());
  data["fieldCount"] = type.getFields().size();
  data["namespace"] = computeCppNamespace(type);
  data["isClassType"] = false;
  data["publicSymbols"] = publicSymbols_;

  // compute the max field name length
  std::size_t maxFieldNameLength = 0U;
  for (const auto& field: type.getFields())
  {
    maxFieldNameLength = std::max(field.name.length() + 2U, maxFieldNameLength);
  }
  data["maxFieldNameLength"] = maxFieldNameLength;

  if (type.getParent())
  {
    auto [absParent, relParent, senRel2] = cppTypeName(*type.getParent().value());
    data["parent"] = relParent;
    data["parentToInheritFrom"] = relParent;
    data["qualParentType"] = absParent;
    data["pyParentAnnotationType"] = PythonAnnotationName::get(*type.getParent().value());
    data["jsonParentSchemaType"] = JSONSchemaRef::get(*type.getParent().value());
  }

  return data;
}

inja::json JsonTypeStorage::toJson(const sen::EnumType& type) const
{
  auto [absolute, relative, senRel] = cppTypeName(type);
  auto [storageAbs, storageRel, storageSenRel] = cppTypeName(type.getStorageType());

  inja::json data;
  data["name"] = type.getName();
  data["qualType"] = absolute;
  data["senType"] = senRel;
  data["senQual"] = getSenQual(type);
  data["pyAnnotationType"] = PythonAnnotationName::get(type);
  data["jsonSchemaType"] = JSONSchemaRef::get(type);
  data["description"] = escapeInvalidStringCharacters(type.getDescription());
  data["storageType"] = storageRel;
  data["storageQualType"] = storageAbs;
  data["storageSenRel"] = storageSenRel;
  data["storageSenQual"] = getSenQual(type.getStorageType());
  data["enums"] = toJsonList(type.getEnums());
  data["namespace"] = computeCppNamespace(type);
  data["isClassType"] = false;
  data["publicSymbols"] = publicSymbols_;

  bool anyEnumsHasDescription = false;
  for (const auto& item: type.getEnums())
  {
    if (!item.description.empty())
    {
      anyEnumsHasDescription = true;
      break;
    }
  }

  data["anyEnumsHasDescription"] = anyEnumsHasDescription;

  return data;
}

inja::json JsonTypeStorage::toJson(const sen::VariantType& type) const
{
  auto [absolute, relative, senRel] = cppTypeName(type);

  inja::json data;
  data["name"] = type.getName();
  data["qualType"] = absolute;
  data["senType"] = senRel;
  data["senQual"] = getSenQual(type);
  data["pyAnnotationType"] = PythonAnnotationName::get(type);
  data["jsonSchemaType"] = JSONSchemaRef::get(type);
  data["description"] = escapeInvalidStringCharacters(type.getDescription());
  data["fields"] = toJsonList(type.getFields());
  data["namespace"] = computeCppNamespace(type);
  data["isClassType"] = false;
  data["publicSymbols"] = publicSymbols_;

  return data;
}

inja::json JsonTypeStorage::toJson(const sen::SequenceType& type) const
{
  auto [absolute, relative, senRel] = cppTypeName(type);
  auto [absElem, relElem, senRelElem] = cppTypeName(*type.getElementType());

  std::string parent;
  if (type.isBounded())
  {
    parent = "::sen::StaticVector<";
    parent.append(absElem);
    parent.append(", ");
    parent.append(std::to_string(type.getMaxSize().value()));
    parent.append("U>");
  }
  else
  {
    parent = "std::vector<";
    parent.append(absElem);
    parent.append(">");
  }

  inja::json data;
  data["name"] = type.getName();
  data["qualTypeAlias"] = std::string("::") + nameSpace_ + "::" + std::string(type.getName());
  data["qualType"] = absolute;
  data["type"] = relative;
  data["senType"] = senRel;
  data["senQual"] = getSenQual(type);
  data["pyAnnotationType"] = PythonAnnotationName::get(type);
  data["jsonSchemaType"] = JSONSchemaRef::get(type);
  data["description"] = escapeInvalidStringCharacters(type.getDescription());
  data["qualElementType"] = absElem;
  data["elementType"] = relElem;
  data["elementSenQual"] = getSenQual(*type.getElementType());
  data["pyElemAnnotationType"] = PythonAnnotationName::get(*type.getElementType());
  data["jsonElemSchemaType"] = JSONSchemaRef::get(*type.getElementType());
  data["namespace"] = computeCppNamespace(type);
  data["isBounded"] = type.isBounded();
  data["fixedSize"] = type.hasFixedSize();
  data["isClassType"] = false;
  data["publicSymbols"] = publicSymbols_;

  if (type.isBounded())
  {
    data["maxSize"] = type.getMaxSize().value();
  }

  return data;
}

inja::json JsonTypeStorage::toJson(const sen::AliasType& type) const
{
  auto [absolute, relative, senRel] = cppTypeName(type);
  auto [absAlias, relAlias, senRelAlias] = cppTypeName(*type.getAliasedType());

  inja::json data;
  data["name"] = type.getName();
  data["qualType"] = absolute;
  data["senType"] = senRel;
  data["senQual"] = getSenQual(type);
  data["pyAnnotationType"] = PythonAnnotationName::get(type);
  data["pyAliasedAnnotationType"] = PythonAnnotationName::get(*type.getAliasedType());
  data["jsonSchemaType"] = JSONSchemaRef::get(type);
  data["jsonAliasedSchemaType"] = JSONSchemaRef::get(*type.getAliasedType());
  data["description"] = escapeInvalidStringCharacters(type.getDescription());
  data["alias"] = relAlias;
  data["qualAlias"] = absAlias;
  data["senQualAlias"] = getSenQual(*type.getAliasedType());
  data["aliasRepType"] = std::string(type.getName()) + "Type";
  data["qualAliasRepType"] = absolute + "Type";
  data["isNotNative"] = !type.getAliasedType()->isNativeType();
  data["isNotSequence"] = !type.getAliasedType()->isSequenceType();
  data["isClassType"] = false;
  data["namespace"] = computeCppNamespace(type);

  return data;
}

inja::json JsonTypeStorage::toJson(const sen::OptionalType& type) const
{
  auto [absolute, relative, senRel] = cppTypeName(type);
  auto [absElem, relElem, senRelElem] = cppTypeName(*type.getType());

  inja::json data;
  data["name"] = type.getName();
  data["qualType"] = absolute;
  data["senType"] = senRel;
  data["senQual"] = getSenQual(type);
  data["pyAnnotationType"] = PythonAnnotationName::get(type);
  data["pyElementAnnotationType"] = PythonAnnotationName::get(*type.getType());
  data["jsonSchemaType"] = JSONSchemaRef::get(type);
  data["jsonElementSchemaType"] = JSONSchemaRef::get(*type.getType());
  data["description"] = escapeInvalidStringCharacters(type.getDescription());
  data["elementType"] = relElem;
  data["qualElementType"] = absElem;
  data["senQualElementType"] = getSenQual(*type.getType());
  data["isNotNative"] = !type.getType()->isNativeType();
  data["isClassType"] = false;
  data["namespace"] = computeCppNamespace(type);
  data["publicSymbols"] = publicSymbols_;

  return data;
}

inja::json JsonTypeStorage::toJson(const sen::QuantityType& type) const
{
  auto [absolute, relative, senRel] = cppTypeName(type);
  auto [absElem, relElem, senRelElem] = cppTypeName(*type.getElementType());

  inja::json data;
  data["name"] = type.getName();
  data["qualType"] = absolute;
  data["senType"] = senRel;
  data["senQual"] = getSenQual(type);
  data["pyElemAnnotationType"] = PythonAnnotationName::get(*type.getElementType());
  data["description"] = escapeInvalidStringCharacters(type.getDescription());
  data["unitAbbrev"] = type.getUnit() ? type.getUnit(sen::Unit::ensurePresent).getAbbreviation() : "none";
  data["elementType"] = relElem;
  data["qualElementType"] = absElem;
  data["namespace"] = computeCppNamespace(type);
  data["isClassType"] = false;
  data["publicSymbols"] = publicSymbols_;

  if (type.getMaxValue().has_value())
  {
    data["max"] = *type.getMaxValue();
  }

  if (type.getMinValue().has_value())
  {
    data["min"] = *type.getMinValue();
  }

  return data;
}

inja::json JsonTypeStorage::toJson(const sen::ClassType& type) const
{
  const auto& parents = type.getParents();

  inja::json data = interfaceData(type);
  data["isInterface"] = type.isInterface();
  data["isAbstract"] = type.isAbstract();
  data["typeGetterFunc"] = sen::ClassType::computeTypeGetterFuncName(std::string(type.getQualifiedName()));
  data["instanceMakerFunc"] = sen::ClassType::computeInstanceMakerFuncName(std::string(type.getQualifiedName()));
  data["isClassType"] = true;
  data["publicSymbols"] = publicSymbols_;

  // constructor
  auto constructor = type.getConstructor();
  if (constructor)
  {
    data["constructor"] = toJson(*constructor);
  }

  if (type.isInterface())
  {
    return data;
  }

  bool inheritsFromLocalObject = type.getParents().empty() || allParentsAreInterfaces(type);

  // inherits from local object when it has
  // no parents or when all parents are interfaces
  data["inheritsFromLocalObject"] = inheritsFromLocalObject;
  data["inlineImplementation"] = inheritsFromLocalObject ? "" : "inline";

  // parents
  std::vector<inja::json> interfaceParentsJson;
  std::vector<inja::json> parentsJson;

  if (!parents.empty())
  {
    for (const auto& parent: parents)
    {
      auto [parentAbs, parentRel, senRelParent] = cppTypeName(*parent);
      inja::json parentData;
      parentData["name"] = parent->getName();
      parentData["type"] = parentRel;
      parentData["qualType"] = parentAbs;
      parentData["senType"] = senRelParent;
      parentData["senQual"] = getSenQual(*parent);
      parentData["pyAnnotationType"] = PythonAnnotationName::get(*parent);
      parentData["jsonSchemaType"] = JSONSchemaRef::get(*parent);
      parentData["isInterface"] = parent->isInterface();
      parentData["isAbstract"] = parent->isAbstract();
      parentData["interfaceName"] = interfaceName(*parent);
      parentData["qualInterfaceName"] = qualInterfaceName(*parent, nameSpace_);
      parentData["localName"] = localName(*parent);
      parentData["qualLocalName"] = qualLocalName(*parent, nameSpace_);
      parentData["remoteName"] = remoteName(*parent);
      parentData["localProxyName"] = localProxyName(*parent);
      parentData["qualRemoteName"] = qualRemoteName(*parent, nameSpace_);
      parentData["qualLocalProxyName"] = qualLocalProxyName(*parent, nameSpace_);
      parentData["namespace"] = computeCppNamespace(*parent);
      parentData["inheritsFromLocalObject"] = parent->getParents().empty() || allParentsAreInterfaces(*parent);

      auto parentConstructor = parent->getConstructor();
      if (parentConstructor)
      {
        parentData["constructor"] = toJson(*parentConstructor);
      }

      parentsJson.push_back(parentData);

      if (parent->isInterface())
      {
        interfaceParentsJson.push_back(parentData);
      }
      else
      {
        data["nonInterfaceParent"] = parentData;
      }
    }
  }
  data["parents"] = parentsJson;
  data["interfaceParents"] = interfaceParentsJson;

  auto dynamicPropertyCount = 0U;
  auto methodCount = 0U;
  auto eventCount = 0U;
  for (const auto& prop: type.getProperties(sen::ClassType::SearchMode::doNotIncludeParents))
  {
    if (prop->getCategory() == sen::PropertyCategory::dynamicRO ||
        prop->getCategory() == sen::PropertyCategory::dynamicRW)
    {
      dynamicPropertyCount++;
    }
  }

  methodCount += static_cast<u32>(type.getMethods(sen::ClassType::SearchMode::doNotIncludeParents).size());
  eventCount += static_cast<u32>(type.getEvents(sen::ClassType::SearchMode::doNotIncludeParents).size());

  // interfacesToImplement
  {
    // we have to implement our own interface
    std::vector<inja::json> interfacesToImplement;
    interfacesToImplement.push_back(interfaceData(type));

    for (const auto& parent: parents)
    {
      if (parent->isInterface())
      {
        interfacesToImplement.push_back(interfaceData(*parent));

        for (const auto& prop: parent->getProperties(sen::ClassType::SearchMode::doNotIncludeParents))
        {
          if (prop->getCategory() == sen::PropertyCategory::dynamicRO ||
              prop->getCategory() == sen::PropertyCategory::dynamicRW)
          {
            dynamicPropertyCount++;
          }
        }

        methodCount += static_cast<u32>(parent->getMethods(sen::ClassType::SearchMode::doNotIncludeParents).size());
        eventCount += static_cast<u32>(parent->getEvents(sen::ClassType::SearchMode::doNotIncludeParents).size());
      }
    }
    data["interfacesToImplement"] = interfacesToImplement;
  }

  data["dynamicPropertyCount"] = dynamicPropertyCount;
  data["methodCount"] = methodCount;
  data["eventCount"] = eventCount;

  // needsPrivateMembers
  {
    bool needsPrivateMembers = hasLocalPropertiesOrEvents(type);
    for (auto itr = parents.begin(); itr != parents.end() && !needsPrivateMembers; ++itr)
    {
      needsPrivateMembers = (*itr)->isInterface() && hasLocalPropertiesOrEvents(*(*itr));
    }
    data["needsPrivateMembers"] = needsPrivateMembers;
  }

  // implementsProperties
  {
    bool implementsProperties = hasLocalProperties(type);
    for (auto itr = parents.begin(); itr != parents.end() && !implementsProperties; ++itr)
    {
      implementsProperties = (*itr)->isInterface() && hasLocalProperties(*(*itr));
    }
    data["implementsProperties"] = implementsProperties;
  }

  // implementsDynamicProperties
  {
    bool implementsDynamicProperties = hasLocalDynamicProperties(type);
    for (auto itr = parents.begin(); itr != parents.end() && !implementsDynamicProperties; ++itr)
    {
      implementsDynamicProperties = (*itr)->isInterface() && hasLocalDynamicProperties(*(*itr));
    }
    data["implementsDynamicProperties"] = implementsDynamicProperties;
  }

  // implementsEvents
  {
    bool implementsEvents = hasLocalEvents(type);
    for (auto itr = parents.begin(); itr != parents.end() && !implementsEvents; ++itr)
    {
      implementsEvents = (*itr)->isInterface() && hasLocalEvents(*(*itr));
    }
    data["implementsEvents"] = implementsEvents;
  }

  // implementsMethods
  {
    auto doesIt = [](const sen::ClassType& meta) -> bool
    {
      constexpr auto noParents = sen::ClassType::SearchMode::doNotIncludeParents;
      return !meta.getMethods(noParents).empty() || !meta.getProperties(noParents).empty();
    };

    bool implementsMethods = doesIt(type);
    for (auto itr = parents.begin(); itr != parents.end() && !implementsMethods; ++itr)
    {
      implementsMethods = implementsMethods || doesIt(*(*itr));  // NOLINT
    }
    data["implementsMethods"] = implementsMethods;
  }

  data["implementsInstanceMaker"] = type.getMethods(sen::ClassType::SearchMode::includeParents).empty() &&
                                    !hasCheckedProperties(type, sen::ClassType::SearchMode::includeParents);

  return data;
}

inja::json JsonTypeStorage::interfaceData(const sen::ClassType& type) const
{
  auto [absolute, relative, senRel] = cppTypeName(type);

  // collect members
  constexpr auto searchMode = sen::ClassType::SearchMode::doNotIncludeParents;
  const auto props = type.getProperties(searchMode);
  const auto events = type.getEvents(searchMode);
  const auto methods = type.getMethods(searchMode);

  bool hasProtectedPropertySetters = false;
  for (const auto& prop: props)
  {
    const auto cat = prop->getCategory();
    if (cat == sen::PropertyCategory::dynamicRO || cat == sen::PropertyCategory::staticRO)
    {
      hasProtectedPropertySetters = true;
      break;
    }
  }

  inja::json data;
  data["name"] = type.getName();
  data["interfaceName"] = interfaceName(type);
  data["localName"] = localName(type);
  data["remoteName"] = remoteName(type);
  data["localProxyName"] = localProxyName(type);
  data["qualType"] = absolute;
  data["senType"] = senRel;
  data["senQual"] = getSenQual(type);
  data["pyAnnotationType"] = PythonAnnotationName::get(type);
  data["jsonSchemaType"] = JSONSchemaRef::get(type);
  data["namespace"] = computeCppNamespace(typeSet_);
  data["description"] = escapeInvalidStringCharacters(type.getDescription());
  data["props"] = toJsonList(sen::makeConstSpan(props));
  data["events"] = toJsonList(sen::makeConstSpan(events));
  data["methods"] = toJsonList(sen::makeConstSpan(methods));
  data["hasMembers"] = !methods.empty() || !events.empty() || !props.empty();
  data["hasProtectedPropertySetters"] = hasProtectedPropertySetters;
  data["hasCheckedProperties"] = hasCheckedProperties(type, sen::ClassType::SearchMode::doNotIncludeParents);
  data["hasProperties"] = !type.getProperties(sen::ClassType::SearchMode::includeParents).empty();
  data["hasOwnProperties"] = !type.getProperties(sen::ClassType::SearchMode::doNotIncludeParents).empty();
  data["hasOwnMethods"] = !type.getMethods(sen::ClassType::SearchMode::doNotIncludeParents).empty();
  data["hasOwnEvents"] = !type.getEvents(sen::ClassType::SearchMode::doNotIncludeParents).empty();

  return data;
}

inja::json JsonTypeStorage::toJson(std::shared_ptr<const sen::Property> prop) const
{
  auto [absolute, relative, senRel] = cppTypeName(*prop->getType());

  inja::json data;
  data["id"] = intToHex(prop->getId().get());
  data["name"] = prop->getName();
  data["description"] = escapeInvalidStringCharacters(prop->getDescription());
  data["type"] = relative;
  data["qualType"] = absolute;
  data["senType"] = senRel;
  data["senQual"] = getSenQual(*prop->getType());
  data["pyAnnotationType"] = PythonAnnotationName::get(*prop->getType());
  data["jsonSchemaType"] = JSONSchemaRef::get(*prop->getType());
  data["passByRef"] = passByRef(*prop->getType());
  data["getter"] = toJson(prop->getGetterMethod());
  data["setter"] = toJson(prop->getSetterMethod());
  data["nextGetter"] = prepend("getNext", capitalize(prop->getName()));
  data["eventName"] = prop->getChangeEvent().getName();
  data["callbackName"] = prepend("on", capitalize(prop->getChangeEvent().getName()));
  data["transportEnum"] = getTransportEnumString(prop->getTransportMode());
  data["confirmed"] = prop->getTransportMode() == sen::TransportMode::confirmed;
  data["checkedSet"] = prop->getCheckedSet();

  if (prop->getCheckedSet())
  {
    std::string checkerMethodName(prop->getName());
    checkerMethodName.append("AcceptsSet");
    data["checkerMethodName"] = checkerMethodName;

    std::string validatorFunc = "[this](const auto& val) { if (!";
    validatorFunc.append(checkerMethodName);
    validatorFunc.append("(val)) { throw std::runtime_error(\"invalid value for property ");
    validatorFunc.append(prop->getName());
    validatorFunc.append("\"); } }");

    data["validatorFunc"] = validatorFunc;
  }

  std::vector<inja::json> tagList;

  for (const auto& tag: prop->getTags())
  {
    inja::json tagElem;
    tagElem["value"] = tag;
    tagList.push_back(std::move(tagElem));
  }

  data["tags"] = tagList;

  {
    auto propType = prop->getType()->isAliasType() ? prop->getType()->asAliasType()->getAliasedType() : prop->getType();
    addIsTypeInfo(data, propType.type());
  }

  switch (prop->getCategory())
  {
    case sen::PropertyCategory::dynamicRW:
      data["writeAllowed"] = true;
      data["needsProtectedSetter"] = false;
      data["dynamic"] = true;
      data["static"] = false;
      data["staticRW"] = false;
      data["staticRO"] = false;
      data["categoryEnum"] = "PropertyCategory::dynamicRW";
      break;

    case sen::PropertyCategory::dynamicRO:
      data["writeAllowed"] = false;
      data["needsProtectedSetter"] = true;
      data["dynamic"] = true;
      data["static"] = false;
      data["staticRW"] = false;
      data["staticRO"] = false;
      data["categoryEnum"] = "PropertyCategory::dynamicRO";
      break;

    case sen::PropertyCategory::staticRO:
      data["writeAllowed"] = false;
      data["needsProtectedSetter"] = true;
      data["dynamic"] = false;
      data["static"] = true;
      data["staticRW"] = false;
      data["staticRO"] = true;
      data["categoryEnum"] = "PropertyCategory::staticRO";
      break;

    case sen::PropertyCategory::staticRW:
      data["writeAllowed"] = false;
      data["needsProtectedSetter"] = false;
      data["dynamic"] = false;
      data["static"] = true;
      data["staticRW"] = true;
      data["staticRO"] = false;
      data["categoryEnum"] = "PropertyCategory::staticRW";
      break;
  }

  return data;
}

inja::json JsonTypeStorage::toJson(const sen::Method& method) const
{
  inja::json data;
  data["name"] = method.getName();
  data["nameImpl"] = append(std::string(method.getName()), "Impl");
  data["description"] = escapeInvalidStringCharacters(method.getDescription());
  data["constant"] = (method.getConstness() == sen::Constness::constant);
  data["hasArgs"] = !method.getArgs().empty();
  data["args"] = toJsonList(method.getArgs());
  data["id"] = intToHex(method.getId().get());
  data["transportEnum"] = getTransportEnumString(method.getTransportMode());
  data["deferred"] = method.getDeferred();
  data["localOnly"] = method.getLocalOnly();
  data["notLocal"] = !method.getLocalOnly();
  data["confirmed"] = method.getTransportMode() == sen::TransportMode::confirmed;

  auto retType = method.getReturnType();
  if (retType->isVoidType())
  {
    data["type"] = "void";
    data["qualType"] = "void";
    data["senType"] = "void";
    data["senQual"] = "void";
    data["pyAnnotationType"] = "NoneType";
    data["jsonSchemaType"] = R"("type" : "null")";
    data["passByRef"] = false;
  }
  else
  {
    auto [absolute, relative, senRel] = cppTypeName(*retType);
    data["type"] = relative;
    data["qualType"] = absolute;
    data["senType"] = senRel;
    data["senQual"] = getSenQual(*retType);
    data["pyAnnotationType"] = PythonAnnotationName::get(*retType);
    data["jsonSchemaType"] = JSONSchemaRef::get(*retType);
    data["passByRef"] = passByRef(*retType);
  }

  return data;
}

inja::json JsonTypeStorage::toJson(std::shared_ptr<const sen::Method> method) const { return toJson(*method); }

inja::json JsonTypeStorage::toJson(std::shared_ptr<const sen::Event> ev) const
{
  inja::json data;
  data["id"] = intToHex(ev->getId().get());
  data["name"] = ev->getName();
  data["description"] = escapeInvalidStringCharacters(ev->getDescription());
  data["args"] = toJsonList(ev->getArgs());
  data["callback"] = prepend("on", capitalize(ev->getName()));
  data["transportEnum"] = getTransportEnumString(ev->getTransportMode());

  return data;
}

inja::json JsonTypeStorage::toJson(const sen::Arg& arg) const
{
  const auto& argType = *arg.type;
  auto [absolute, relative, senRel] = cppTypeName(argType);

  inja::json data;
  data["name"] = arg.name;
  data["description"] = escapeInvalidStringCharacters(arg.description);
  data["type"] = relative;
  data["passByRef"] = passByRef(argType);
  data["qualType"] = absolute;
  data["senType"] = senRel;
  data["senQual"] = getSenQual(*arg.type);
  data["pyAnnotationType"] = PythonAnnotationName::get(*arg.type);
  data["jsonSchemaType"] = JSONSchemaRef::get(*arg.type);

  return data;
}
