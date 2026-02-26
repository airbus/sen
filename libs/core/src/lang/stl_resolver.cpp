// === stl_resolver.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/lang/stl_resolver.h"

// implementation
#include "fom_document_set.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/lang/error_reporter.h"
#include "sen/core/lang/stl_parser.h"
#include "sen/core/lang/stl_scanner.h"
#include "sen/core/lang/stl_statement.h"
#include "sen/core/lang/stl_token.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/unit_registry.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"

// std
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace sen::lang
{

namespace
{

constexpr auto* packagePathSeparator = ".";

// transport
constexpr auto defaultMethodTransport = TransportMode::confirmed;
constexpr auto defaultEventTransport = TransportMode::multicast;
constexpr auto defaultPropertyTransport = TransportMode::multicast;

enum class TypeKind
{
  valueType,
  classType,
  anyType
};

void readFile(const std::filesystem::path& fileName, std::string& contents)
{
  std::ifstream in(fileName);

  // reserve required memory in one go
  in.seekg(0U, std::ios::end);
  contents.reserve(static_cast<std::size_t>(in.tellg()));
  in.seekg(0U, std::ios::beg);

  // read contents
  contents.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

[[nodiscard]] std::string computeQualifiedName(std::string_view packagePrefix, std::string_view name)
{
  std::string result;
  if (!packagePrefix.empty())
  {
    result.append(packagePrefix);
    result.append(packagePathSeparator);
  }

  result.append(name);
  return result;
}

[[nodiscard]] bool isTypeNameQualified(std::string_view name)
{
  // if it has a package path separator then it must be qualified
  return (name.find_first_of(packagePathSeparator) != std::string::npos);
}

[[nodiscard]] std::string computePackagePrefix(const std::vector<std::string>& path)
{
  std::string result;

  for (const auto& item: path)
  {
    if (!result.empty())
    {
      result.append(packagePathSeparator);
    }

    result.append(item);
  }

  return result;
}

[[nodiscard]] std::optional<ConstTypeHandle<>> findQualifiedType(const std::string& name,
                                                                 const TypeSetContext& globalTypeSetContext) noexcept
{
  for (const auto& typeSet: globalTypeSetContext)
  {
    for (const auto& type: typeSet.types)
    {
      if (type->getQualifiedName() == name)
      {
        return type;  // TODO check
      }
    }
  }

  return std::nullopt;
}

[[nodiscard]] std::optional<Var> tryFindAttribute(std::string_view name, const StlAttributeList& attributes)
{
  for (const auto& element: attributes)
  {
    if (element.name.lexeme == name)
    {
      return element.value.lexeme;
    }
  }
  return std::nullopt;
}

[[nodiscard]] bool isSingleWordAttribute(const StlAttribute& elem) { return elem.value.lexeme.empty(); }

[[nodiscard]] std::string generateDescription(const std::vector<StlToken>& tokens)
{
  // early exit
  if (tokens.empty())
  {
    return {};
  }

  std::string result;
  for (const auto& token: tokens)
  {
    if (token.lexeme.empty())
    {
      continue;
    }

    if (!result.empty() && result.back() != ' ' && token.lexeme.front() != ' ')
    {
      result.append(" ");
    }

    result.append(token.lexeme);
  }
  return result;
}

//--------------------------------------------------------------------------------------------------------------
// StatementVisitor
//--------------------------------------------------------------------------------------------------------------

class StatementVisitor
{
  SEN_NOCOPY_NOMOVE(StatementVisitor)

public:
  [[nodiscard]] static const TypeSet* resolve(const std::vector<StlStatement>& statements,
                                              const ResolverContext& context,
                                              TypeSetContext& globalContext,
                                              const TypeSettings& settings)
  {
    // create a new typeset for this file
    auto newTypeSet = globalContext.createNewTypeSet();

    // set the filename here, the rest of fields are set during
    // the visit to each of the statements below
    newTypeSet->fileName = context.originalFileName.string();

    StatementVisitor visitor(*newTypeSet, context, globalContext, settings);
    for (const auto& statement: statements)
    {
      std::visit(visitor, statement);
    }

    return newTypeSet;
  }

public:
  void operator()(std::monostate statement) const { std::ignore = statement; }

  void operator()(const StlImportStatement& statement)
  {
    set_.importedSets.push_back(
      readTypesFile(statement.fileName.lexeme, context_.includePaths, globalContext_, settings_, set_.fileName));
  }

  void operator()(const StlPackageStatement& statement)
  {
    if (!packagePrefix_.empty())
    {
      std::string err = "package name ";
      err.append("is already defined as '");
      err.append(packagePrefix_);
      err.append("' in file '");
      err.append(set_.fileName);
      err.append("'");
      ErrorReporter::report(statement.path.at(0).loc, err);
      throwRuntimeError(err);
    }

    for (const auto& item: statement.path)
    {
      set_.package.push_back(item.lexeme);
    }
    packagePrefix_ = computePackagePrefix(set_.package);
  }

  void operator()(const StlStructStatement& statement)
  {
    checkPackageIsDefined(statement.identifier);

    auto specName = statement.identifier.lexeme;
    auto specQualifiedName = computeQualifiedName(packagePrefix_, statement.identifier.lexeme);
    auto specDescription = generateDescription(statement.description);

    std::vector<StructField> fields;
    fields.reserve(statement.fields.size());
    for (const auto& item: statement.fields)
    {
      reportIfNotLowercaseName(item.identifier);

      StructField fieldSpec(item.identifier.lexeme,
                            generateDescription(item.description),
                            ensureFindType(item.typeName, "type not found", TypeKind::valueType));

      fields.push_back(std::move(fieldSpec));
    }

    MaybeConstTypeHandle<StructType> parent;
    if (statement.parentStructName.has_value())
    {
      auto parentType = findType(*statement.parentStructName, TypeKind::valueType);
      if (!parentType)
      {
        std::string err = "type '";
        err.append(statement.parentStructName.value().qualifiedName);
        err.append("' not found");
        ErrorReporter::report(statement.parentStructName->path.front().loc, err);
        throwRuntimeError(err);
      }

      parent = dynamicTypeHandleCast<const StructType>(parentType.value());

      if (!parent)
      {
        std::string err = "parent of struct ";
        err.append(specName);
        err.append(" of type '");
        err.append(statement.parentStructName->qualifiedName);
        err.append("'");
        err.append(" is not a struct");
        ErrorReporter::report(statement.parentStructName->path.front().loc, err);
        throwRuntimeError(err);
      }
    }

    StructSpec spec(specName, specQualifiedName, specDescription, fields, parent);

    addType([&spec]() { return StructType::make(spec); }, spec.name, statement.identifier);
  }

  void operator()(const StlEnumStatement& statement)
  {
    checkPackageIsDefined(statement.identifier);

    std::vector<Enumerator> enums;
    enums.reserve(statement.enumerators.size());
    for (uint32_t i = 0U; i < statement.enumerators.size(); ++i)
    {
      const auto& item = statement.enumerators[i];

      reportIfNotLowercaseName(item.identifier);

      Enumerator enumerator;
      enumerator.name = item.identifier.lexeme;
      enumerator.key = i;
      enumerator.description = generateDescription(item.description);

      enums.push_back(std::move(enumerator));
    }

    auto storageType = dynamicTypeHandleCast<const IntegralType>(
      ensureFindType(statement.storageTypeName, "type not found", TypeKind::valueType));

    errorIfNotFound(storageType, statement.storageTypeName.path.back(), "type is not numeric");

    EnumSpec spec(statement.identifier.lexeme,
                  computeQualifiedName(packagePrefix_, statement.identifier.lexeme),
                  generateDescription(statement.description),
                  std::move(enums),
                  std::move(storageType).value());

    addType([&spec]() { return EnumType::make(spec); }, spec.name, statement.identifier);
  }

  void operator()(const StlVariantStatement& statement)
  {
    checkPackageIsDefined(statement.identifier);

    VariantSpec spec;
    setBasicSpecInfo(spec, statement.identifier);
    spec.description = generateDescription(statement.description);

    for (uint32_t i = 0U; i < statement.elements.size(); ++i)
    {
      const auto& item = statement.elements[i];

      VariantField fieldSpec(
        i, generateDescription(item.description), ensureFindType(item.typeName, "type not found", TypeKind::valueType));

      spec.fields.push_back(std::move(fieldSpec));
    }

    addType([&spec]() { return VariantType::make(spec); }, spec.name, statement.identifier);
  }

  void operator()(const StlSequenceStatement& statement)
  {
    std::optional<size_t> maxSize;
    if (statement.maxSize)
    {
      maxSize = getCopyAs<uint64_t>(statement.maxSize->value);
    }

    SequenceSpec spec(statement.identifier.lexeme,
                      computeQualifiedName(packagePrefix_, statement.identifier.lexeme),
                      generateDescription(statement.description),
                      ensureFindType(statement.elementTypeName, "type not found", TypeKind::valueType),
                      maxSize,
                      false);

    addType([&spec]() { return SequenceType::make(spec); }, spec.name, statement.identifier);
  }

  void operator()(const StlArrayStatement& statement)
  {
    SequenceSpec spec(statement.identifier.lexeme,
                      computeQualifiedName(packagePrefix_, statement.identifier.lexeme),
                      generateDescription(statement.description),
                      ensureFindType(statement.elementTypeName, "type not found", TypeKind::valueType),
                      getCopyAs<uint64_t>(statement.size.value),
                      true);

    addType([&spec]() { return SequenceType::make(spec); }, spec.name, statement.identifier);
  }

  void operator()(const StlQuantityStatement& statement)
  {
    auto elementType = ensureFindType(statement.valueTypeName, "type not found", TypeKind::valueType);
    if (!elementType->isNumericType())
    {
      errorIfNotFound(std::optional {elementType}, statement.valueTypeName.path.front(), "type is not numeric");
      return;
    }

    std::optional<float64_t> maxValue;
    std::optional<float64_t> minValue;

    if (statement.attributes)
    {
      if (auto max = tryFindAttribute("max", *statement.attributes))
      {
        maxValue = getCopyAs<float64_t>(*max);
      }

      if (auto min = tryFindAttribute("min", *statement.attributes))
      {
        minValue = getCopyAs<float64_t>(*min);
      }
    }

    SEN_ASSERT(elementType->isNumericType() && "ElementType needs to be numeric");
    QuantitySpec spec(statement.identifier.lexeme,
                      computeQualifiedName(packagePrefix_, statement.identifier.lexeme),
                      generateDescription(statement.description),
                      dynamicTypeHandleCast<const NumericType>(elementType).value(),
                      UnitRegistry::get().searchUnitByAbbreviation(statement.unitName.lexeme),
                      minValue,
                      maxValue);

    addType([&spec]() { return QuantityType::make(spec); }, "quantity", statement.identifier);
  }

  void operator()(const StlTypeAliasStatement& statement)
  {
    AliasSpec spec(statement.identifier.lexeme,
                   computeQualifiedName(packagePrefix_, statement.identifier.lexeme),
                   generateDescription(statement.description),
                   ensureFindType(statement.typeName, "type not found", TypeKind::valueType));

    addType([&spec]() { return AliasType::make(spec); }, spec.name, statement.identifier);
  }

  void operator()(const StlOptionalTypeStatement& statement)
  {
    OptionalSpec spec(statement.identifier.lexeme,
                      computeQualifiedName(packagePrefix_, statement.identifier.lexeme),
                      generateDescription(statement.description),
                      ensureFindType(statement.typeName, "type not found", TypeKind::valueType));

    addType([&spec]() { return OptionalType::make(spec); }, spec.name, statement.identifier);
  }

  void operator()(const StlClassStatement& statement)
  {
    checkPackageIsDefined(statement.members.identifier);

    ClassSpec classSpec;
    classSpec.isInterface = false;
    classSpec.isAbstract = statement.isAbstract;
    populateClassMembers(classSpec, statement.members);

    const auto& parentToken = statement.extends;

    // parent class, if any
    if (const auto& parentName = parentToken.qualifiedName; !parentName.empty())
    {
      const auto parentType =
        ensureFindType(parentToken, "expecting a valid parent type for class", TypeKind::classType);

      if (!parentType->isClassType())
      {
        std::string msg;
        msg.append("invalid type '");
        msg.append(parentName);
        msg.append("' as parent for class '");
        msg.append(classSpec.name);
        msg.append("'");
        ErrorReporter::report(parentToken.path.front().loc, msg, parentName.size());
        throwRuntimeError(msg);
      }

      const auto parentClass = parentType->asClassType();
      if (parentClass->isInterface())
      {
        std::string msg;
        msg.append("cannot extend interface '");
        msg.append(parentName);
        msg.append("' as parent for class '");
        msg.append(classSpec.name);
        msg.append("'. Use 'implements' instead.");

        ErrorReporter::report(parentToken.path.front().loc, msg, parentName.size());
        throwRuntimeError(msg);
      }

      classSpec.parents.push_back(dynamicTypeHandleCast<const ClassType>(parentType).value());
    }

    // interfaces, if any
    for (const auto& iface: statement.implements)
    {
      const auto ifaceType = ensureFindType(iface, "expecting a valid parent type for interface", TypeKind::classType);

      if (ifaceType->isClassType())
      {
        std::string msg;
        msg.append("invalid type '");
        msg.append(iface.qualifiedName);
        msg.append("' as parent interface for class '");
        msg.append(classSpec.name);
        msg.append("'");

        ErrorReporter::report(iface.path.front().loc, msg, iface.qualifiedName.size());
        throwRuntimeError(msg);
      }

      const auto ifaceClass = ifaceType->asClassType();
      if (!ifaceClass->isInterface())
      {
        std::string msg;
        msg.append("type '");
        msg.append(iface.qualifiedName);
        msg.append("' is not an interface (as parent for class '");
        msg.append(classSpec.name);
        msg.append("')");

        ErrorReporter::report(iface.path.front().loc, msg, iface.qualifiedName.size());
        throwRuntimeError(msg);
      }

      classSpec.parents.push_back(dynamicTypeHandleCast<const ClassType>(ifaceType).value());
    }

    classSpec.constructor = makeConstructor(classSpec);

    addType([&classSpec]() { return ClassType::make(classSpec); }, classSpec.name, statement.members.identifier);
  }

  void operator()(const StlInterfaceStatement& statement)
  {
    checkPackageIsDefined(statement.identifier);

    ClassSpec classSpec;
    classSpec.isInterface = true;
    populateClassMembers(classSpec, statement);
    classSpec.constructor = makeConstructor(classSpec);

    addType([&classSpec]() { return ClassType::make(classSpec); }, classSpec.name, statement.identifier);
  }

private:
  StatementVisitor(TypeSet& set,
                   const ResolverContext& context,
                   TypeSetContext& globalContext,
                   const TypeSettings& settings) noexcept
    : context_(context), set_(set), globalContext_(globalContext), settings_(settings)
  {
  }

  ~StatementVisitor() noexcept = default;

  static void computeTransportMode(const StlAttribute& attr, TransportMode& val, bool& set)
  {
    if (attr.name.lexeme == "bestEffort")
    {
      if (set)
      {
        reportRepeatedAttribute(attr);
      }
      set = true;
      val = TransportMode::unicast;
    }

    if (attr.name.lexeme == "confirmed")
    {
      if (set)
      {
        reportRepeatedAttribute(attr);
      }
      set = true;
      val = TransportMode::confirmed;
    }
  }

  // NOLINTNEXTLINE(readability-function-size)
  void populateClassMembers(ClassSpec& classSpec, const StlInterfaceStatement& statement)
  {
    setBasicSpecInfo(classSpec, statement.identifier);
    classSpec.description = generateDescription(statement.description);

    auto annotationsItr = settings_.classAnnotations.find(classSpec.qualifiedName);

    for (const auto& method: statement.functions)
    {
      reportIfNotLowercaseName(method.identifier);

      CallableSpec methodCallableSpec;
      methodCallableSpec.name = method.identifier.lexeme;
      methodCallableSpec.description = generateDescription(method.description);
      methodCallableSpec.transportMode = defaultMethodTransport;

      auto methodSpecConstness = Constness::nonConstant;
      auto methodSpecDeferred = false;
      auto methodSpecLocalOnly = false;

      if (annotationsItr != settings_.classAnnotations.end())
      {
        methodSpecDeferred = annotationsItr->second.deferredMethods.count(methodCallableSpec.name) != 0;
      }

      bool transportModeSet = false;
      for (const auto& attr: method.attributes)
      {
        if (isSingleWordAttribute(attr))
        {
          if (attr.name.lexeme == "const")
          {
            methodSpecConstness = Constness::constant;
            continue;
          }
          if (attr.name.lexeme == "deferred")
          {
            ErrorReporter::report(attr.name.loc,
                                  "using 'deferred' in STL is now deprecated. Please use code generation settings.",
                                  attr.name.lexeme.size(),
                                  true);

            methodSpecDeferred = true;
            continue;
          }
          if (attr.name.lexeme == "local")
          {
            methodSpecLocalOnly = true;
            continue;
          }

          computeTransportMode(attr, methodCallableSpec.transportMode, transportModeSet);
        }
        else
        {
          reportInvalidAttribute(attr);
        }
      }

      // args
      for (const auto& methodArg: method.arguments)
      {
        methodCallableSpec.args.push_back(getArgSpec(methodArg));
      }

      // return type
      ConstTypeHandle<> methodSpecReturnType = sen::VoidType::get();
      if (!method.returnTypeName.qualifiedName.empty() && method.returnTypeName.qualifiedName != "void")
      {
        methodSpecReturnType = ensureFindType(
          method.returnTypeName, "invalid return type for method " + methodCallableSpec.name, TypeKind::valueType);
      }

      MethodSpec methodSpec(methodCallableSpec,
                            methodSpecReturnType,
                            methodSpecConstness,
                            NonPropertyRelated {},
                            methodSpecDeferred,
                            methodSpecLocalOnly);

      classSpec.methods.push_back(std::move(methodSpec));
    }

    for (const auto& eventMember: statement.events)
    {
      reportIfNotLowercaseName(eventMember.identifier);

      EventSpec eventSpec;
      eventSpec.callableSpec.name = eventMember.identifier.lexeme;
      eventSpec.callableSpec.description = generateDescription(eventMember.description);
      eventSpec.callableSpec.transportMode = defaultEventTransport;

      bool transportModeSet = false;
      for (const auto& attr: eventMember.attributes)
      {
        if (isSingleWordAttribute(attr))
        {
          computeTransportMode(attr, eventSpec.callableSpec.transportMode, transportModeSet);
        }
        else
        {
          reportInvalidAttribute(attr);
        }
      }

      // args
      for (const auto& eventArg: eventMember.arguments)
      {
        eventSpec.callableSpec.args.push_back(getArgSpec(eventArg));
      }

      classSpec.events.push_back(std::move(eventSpec));
    }

    for (const auto& prop: statement.properties)
    {
      reportIfNotLowercaseName(prop.identifier);

      // the ID of the property depends on is name and the class id
      auto propertySpecName = prop.identifier.lexeme;
      auto propertySpecType = ensureFindType(prop.typeName, "expecting property type", TypeKind::valueType);
      auto propertySpecDescription = generateDescription(prop.description);
      auto propertySpecCategory = PropertyCategory::dynamicRO;
      auto propertySpecTransportMode = defaultPropertyTransport;
      std::vector<std::string> propertySpecTags;
      bool propertySpecCheckedSet = false;

      bool transportModeSet = false;
      for (const auto& attr: prop.attributes)
      {
        if (isSingleWordAttribute(attr))
        {
          computeTransportMode(attr, propertySpecTransportMode, transportModeSet);

          if (attr.name.lexeme == "static")
          {
            propertySpecCategory = PropertyCategory::staticRW;
            continue;
          }

          if (attr.name.lexeme == "static_no_config")
          {
            propertySpecCategory = PropertyCategory::staticRO;
            continue;
          }

          if (attr.name.lexeme == "writable")
          {
            propertySpecCategory = PropertyCategory::dynamicRW;
            continue;
          }

          if (attr.name.lexeme == "checked")
          {
            ErrorReporter::report(attr.name.loc,
                                  "using 'checked' in STL is now deprecated. Please use code generation settings.",
                                  attr.name.lexeme.size(),
                                  true);

            propertySpecCheckedSet = true;
            continue;
          }
        }
        else
        {
          if (attr.name.lexeme == "tag")
          {
            propertySpecTags.push_back(attr.value.lexeme);
          }
          else
          {
            reportInvalidAttribute(attr);
          }
        }
      }

      if ((propertySpecCategory == PropertyCategory::dynamicRW ||
           propertySpecCategory == PropertyCategory::dynamicRO) &&
          propertySpecTransportMode != TransportMode::confirmed && !propertySpecType->isBounded())
      {
        std::string err;
        err.append("unbounded dynamic properties with non-confirmed transport mode may cause data loss");
        ErrorReporter::report(prop.identifier.loc, err, prop.identifier.lexeme.size());
        throwRuntimeError(err);
      }

      if (annotationsItr != settings_.classAnnotations.end())
      {
        propertySpecCheckedSet = annotationsItr->second.checkedProperties.count(propertySpecName) != 0;
      }

      classSpec.properties.emplace_back(std::move(propertySpecName),
                                        std::move(propertySpecDescription),
                                        std::move(propertySpecType),
                                        propertySpecCategory,
                                        propertySpecTransportMode,
                                        propertySpecCheckedSet,
                                        std::move(propertySpecTags));
    }
  }

  static void collectConstructorArgs(std::vector<Arg>& list, const ClassType& type)
  {
    auto allProps = type.getProperties(ClassType::SearchMode::includeParents);
    for (const auto& prop: allProps)
    {
      if (prop->getCategory() == PropertyCategory::staticRW)
      {
        const auto argName = std::string(prop->getName());
        auto arg = Arg {std::string(prop->getName()), std::string(prop->getDescription()), prop->getType()};
        list.push_back(arg);
      }
    }
  }

  static void collectConstructorArgs(std::vector<Arg>& list, const ClassSpec& spec)
  {
    // collect parents args recursively
    for (const auto& parent: spec.parents)
    {
      collectConstructorArgs(list, *parent);
    }

    // collect class args
    for (const auto& prop: spec.properties)
    {
      if (prop.category == PropertyCategory::staticRW)
      {
        list.emplace_back(prop.name, prop.description, prop.type);
      }
    }
  }

  [[nodiscard]] static MethodSpec makeConstructor(const ClassSpec& spec)
  {

    CallableSpec constructorCallableSpec;
    constructorCallableSpec.name = std::string("constructor") + spec.name;
    constructorCallableSpec.description = "constructor";
    collectConstructorArgs(constructorCallableSpec.args, spec);

    auto constructorSpecReturnType = sen::VoidType::get();
    auto constructorSpecConstness = Constness::nonConstant;

    return {constructorCallableSpec, constructorSpecReturnType, constructorSpecConstness};
  }

  [[nodiscard]] Arg getArgSpec(const StlArgStatement& statement) const
  {
    reportIfNotLowercaseName(statement.identifier);

    auto name = statement.identifier.lexeme;

    return {name,
            generateDescription(statement.description),
            ensureFindType(statement.typeName, std::string("invalid type for argument ") + name, TypeKind::valueType)};
  }

  static void reportIfNotLowercaseName(const StlToken& token)
  {
    if (auto valid = Type::validateLowerCaseName(token.lexeme); valid.isError())
    {
      ErrorReporter::report(token.loc, valid.getError(), token.lexeme.size());
      throwRuntimeError(valid.getError());
    }
  }

  [[noreturn]] static void reportInvalidAttribute(const StlAttribute& attr)
  {
    std::string errorMessage = "invalid attribute ";
    ErrorReporter::report(attr.name.loc, errorMessage, attr.name.lexeme.size());
    throwRuntimeError(errorMessage);
  }

  [[noreturn]] static void reportRepeatedAttribute(const StlAttribute& attr)
  {
    std::string errorMessage = "repeated value for attribute ";
    ErrorReporter::report(attr.name.loc, errorMessage, attr.name.lexeme.size());
    throwRuntimeError(errorMessage);
  }

  template <typename T>
  static void errorIfNotFound(const std::optional<T>& type, const StlToken& id, const std::string& errorMessage)
  {
    if (!type)
    {
      ErrorReporter::report(id.loc, errorMessage, id.lexeme.size());
      throwRuntimeError(errorMessage);
    }
  }

  [[noreturn]] static void reportErrorAndThrow(std::optional<StlToken> id, const std::string& message)
  {
    if (id.has_value())
    {
      ErrorReporter::report(id.value().loc, message, id.value().lexeme.size());
    }
    throwRuntimeError(message);
  }

  static void errorIfWrongKind(const Type* type, std::optional<StlToken> id, TypeKind kind)
  {
    if (kind != TypeKind::anyType && type == nullptr)
    {
      reportErrorAndThrow(id, "type not found");
    }

    const bool isClassType = type->isClassType();
    switch (kind)
    {
      case TypeKind::classType:
        if (!isClassType)
        {
          reportErrorAndThrow(std::move(id), std::string(type->getName()).append(" is not a class type"));
        }
        break;

      case TypeKind::valueType:
        if (isClassType)
        {
          reportErrorAndThrow(std::move(id), std::string(type->getName()).append(" is not a value type"));
        }
        break;

      case TypeKind::anyType:
        break;
    }
  }

  [[nodiscard]] ConstTypeHandle<> ensureFindType(const StlTypeNameStatement& typeName,
                                                 const std::string& errorMessage,
                                                 TypeKind kind) const
  {
    auto type = findType(typeName, kind);
    errorIfNotFound(type, typeName.path.back(), errorMessage);
    return std::move(type).value();
  }

  [[nodiscard]] std::optional<ConstTypeHandle<>> findType(const StlTypeNameStatement& typeName, TypeKind kind) const
  {
    if (typeName.qualifiedName == "void")
    {
      return VoidType::get();
    }

    // if qualified, try to look in the global context
    if (isTypeNameQualified(typeName.qualifiedName))
    {
      if (auto type = findQualifiedType(typeName.qualifiedName, globalContext_); type)
      {
        errorIfWrongKind(type.value().type(), std::nullopt, kind);
        return type;
      }
    }

    // otherwise look up in the built-in types
    const auto& nativeTypes = getNativeTypes();
    for (const auto& type: nativeTypes)
    {
      if (type->getName() == typeName.qualifiedName)
      {
        errorIfWrongKind(type.type(), std::nullopt, kind);
        return type;
      }
    }

    // maybe is in our current package
    for (const auto& type: set_.types)
    {
      if (type->getName() == typeName.qualifiedName)
      {
        errorIfWrongKind(type.type(), std::nullopt, kind);
        return type;
      }
    }

    // maybe is in the imported packages that have the same package name
    for (const auto& otherSet: set_.importedSets)
    {
      if (otherSet->package == set_.package)
      {
        for (const auto& type: otherSet->types)
        {
          if (type->getName() == typeName.qualifiedName || type->getQualifiedName() == typeName.qualifiedName)
          {
            errorIfWrongKind(type.type(), std::nullopt, kind);
            return type;
          }
        }
      }

      // look for a common prefix
      std::string commonPrefix;
      for (std::size_t i = 0; i < otherSet->package.size() && i < set_.package.size(); ++i)
      {
        if (otherSet->package.at(i) != set_.package.at(i))
        {
          break;
        }

        commonPrefix.append(set_.package.at(i)).append(".");
      }

      if (!commonPrefix.empty())
      {
        std::string fullTypeName = commonPrefix + typeName.qualifiedName;

        for (const auto& type: otherSet->types)
        {
          if (type->getQualifiedName() == fullTypeName)
          {
            errorIfWrongKind(type.type(), std::nullopt, kind);
            return type;
          }
        }
      }
    }

    // not found
    return std::nullopt;
  }

  template <typename F>
  void addType(const F& func, const std::string& typeName, const StlToken& token)
  {
    if (auto valid = Type::validateTypeName(token.lexeme); valid.isError())
    {
      ErrorReporter::report(token.loc, valid.getError(), token.lexeme.size());
      throwRuntimeError(valid.getError());
    }

    // check that the type is not repeated
    for (const auto& other: set_.types)
    {
      if (const auto* custom = other->asCustomType(); custom && custom->getQualifiedName() == typeName)
      {
        std::string msg;
        msg.append("There is already a type named '");
        msg.append(typeName);
        msg.append("'");

        ErrorReporter::report(token.loc, msg, token.lexeme.size());
        throwRuntimeError(msg);
      }
    }

    // create the type instance
    try
    {
      set_.types.push_back(func());
    }
    catch (const std::runtime_error& err)
    {
      std::string msg;
      msg.append("Type '");
      msg.append(typeName);
      msg.append("': ");
      msg.append(err.what());

      ErrorReporter::report(token.loc, msg, token.lexeme.size());
      throwRuntimeError(msg);
    }
  }

  void checkPackageIsDefined(const StlToken& token) const
  {
    if (packagePrefix_.empty())
    {
      std::string msg = "please specify the package before defining a type";
      ErrorReporter::report(token.loc, msg, token.lexeme.size());
      throwRuntimeError(msg);
    }
  }

  template <typename Spec>
  void setBasicSpecInfo(Spec& spec, const StlToken& typeName)
  {
    spec.name = typeName.lexeme;
    spec.qualifiedName = computeQualifiedName(packagePrefix_, spec.name);
  }

private:
  const ResolverContext& context_;
  TypeSet& set_;
  TypeSetContext& globalContext_;
  std::string packagePrefix_;
  const TypeSettings& settings_;
};

std::filesystem::path searchFile(const std::filesystem::path& fileName,
                                 const std::vector<std::filesystem::path>& includePaths,
                                 const std::filesystem::path& from)
{
  // directly look for it
  if (std::filesystem::exists(fileName))
  {
    return fileName;
  }

  // check relative to the current file
  if (!from.empty())
  {
    auto fileNameToUse = from.parent_path() / fileName;

    if (std::filesystem::exists(fileNameToUse))
    {
      return fileNameToUse;
    }
  }

  // check in the include paths
  for (const auto& path: includePaths)
  {
    auto fileNameToUse = path / fileName;
    if (std::filesystem::exists(fileNameToUse))
    {
      return fileNameToUse;
    }
  }

  return {};
}

const TypeSet* readStlFile(const std::filesystem::path& originalFileName,
                           const std::filesystem::path& absoluteFileName,
                           const std::vector<std::filesystem::path>& includePaths,
                           TypeSetContext& globalContext,
                           const TypeSettings& settings)
{
  std::string fileContents;
  readFile(absoluteFileName, fileContents);

  StlScanner scanner(fileContents);
  const auto& tokens = scanner.scanTokens();

  StlParser parser(tokens);
  auto statements = parser.parse();

  ResolverContext resolverContext {originalFileName, absoluteFileName, includePaths};
  StlResolver resolver(statements, resolverContext, globalContext);

  return resolver.resolve(settings);
}

const TypeSet* readFomFile(const std::filesystem::path& originalFileName,
                           const std::filesystem::path& absoluteFileName,
                           TypeSetContext& globalContext,
                           const TypeSettings& settings)
{
  // compute the paths and mappings
  std::vector<std::filesystem::path> fomPaths;
  std::vector<std::filesystem::path> fomMappingFiles;

  if (!absoluteFileName.has_parent_path() && !absoluteFileName.parent_path().has_parent_path())
  {
    throwRuntimeError("invalid FOM folder layout");
  }

  auto grandparent = absoluteFileName.parent_path().parent_path();

  // search in all the directories of the grandparent folder
  for (const auto& entry: std::filesystem::directory_iterator {grandparent})
  {
    if (entry.is_directory())
    {
      // if directory assume it's a fom path
      fomPaths.push_back(entry.path());
    }
    else if (entry.is_regular_file() && entry.path().extension() == ".xml")
    {
      // if xml, should be a mapping file
      fomMappingFiles.push_back(entry.path());
    }
  }

  FomDocumentSet fomDocumentSet(fomPaths, fomMappingFiles, settings);
  auto typeSets = fomDocumentSet.computeTypeSets();

  // find the set
  const TypeSet* result {nullptr};
  for (auto& [document, set]: typeSets)
  {
    if (document->filePath == absoluteFileName)
    {
      result = set.get();
      break;
    }
  }

  if (!result)
  {
    std::string err;
    err.append("did not find any HLA FOM file named ");
    err.append(originalFileName.string());
    throwRuntimeError(err);
  }

  // store the types in the global context for future reuse
  globalContext.append(std::move(fomDocumentSet).getRootTypeSet());

  for (auto& [doc, set]: typeSets)
  {
    globalContext.append(std::move(set));
  }

  return result;
}

const TypeSet* doReadTypesFile(const std::filesystem::path& originalFileName,
                               const std::filesystem::path& absoluteFileName,
                               const std::vector<std::filesystem::path>& includePaths,
                               TypeSetContext& globalContext,
                               const TypeSettings& settings)
{
  if (originalFileName.has_extension() && originalFileName.extension() == ".xml")
  {
    return readFomFile(originalFileName, absoluteFileName, globalContext, settings);
  }

  return readStlFile(originalFileName, absoluteFileName, includePaths, globalContext, settings);
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Resolver
//--------------------------------------------------------------------------------------------------------------

StlResolver::StlResolver(const std::vector<StlStatement>& statements,
                         const ResolverContext& context,
                         TypeSetContext& globalContext) noexcept
  : statements_(statements), context_(context), globalContext_(globalContext)
{
}

const TypeSet* StlResolver::resolve(const TypeSettings& settings)
{
  return StatementVisitor::resolve(statements_, context_, globalContext_, settings);
}

//--------------------------------------------------------------------------------------------------------------
// Utility
//--------------------------------------------------------------------------------------------------------------

[[nodiscard]] const TypeSet* readTypesFile(const std::filesystem::path& fileName,
                                           const std::vector<std::filesystem::path>& includePaths,
                                           TypeSetContext& globalTypeSetContext,
                                           const TypeSettings& settings,
                                           std::string_view from)
{
  // check if we already resolved this
  for (const auto& element: globalTypeSetContext)
  {
    if (element.fileName == fileName)
    {
      return &element;
    }

    for (const auto& includePath: includePaths)
    {
      if (element.fileName == (includePath / fileName))
      {
        return &element;
      }
    }
  }

  auto fileNameToUse = searchFile(fileName, includePaths, from);
  if (fileNameToUse.empty())
  {
    std::string err;
    err.append("could not find file '");
    err.append(fileName.string());
    err.append("' directly or in any of the include paths: ");

    for (const auto& path: includePaths)
    {
      err.append(path.generic_string());
      err.append(" ");
    }

    throwRuntimeError(err);
  }

  return doReadTypesFile(fileName, fileNameToUse.string(), includePaths, globalTypeSetContext, settings);
}

}  // namespace sen::lang
