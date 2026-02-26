// === fom_document_set.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "fom_document_set.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/io/util.h"
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
#include "sen/core/meta/type.h"
#include "sen/core/meta/unit_registry.h"
#include "sen/core/meta/variant_type.h"

// std
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace sen::lang
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

constexpr std::size_t maxStaticVectorSize = 1024 * 1024;

void collectClassesNodes(pugi::xml_node node, std::vector<pugi::xml_node>& result)
{
  if (!node.select_nodes("semantics").empty())
  {
    result.push_back(node);
  }

  for (auto& child: node.select_nodes("objectClass"))
  {
    collectClassesNodes(child.node(), result);
  }
}

void collectInteractionNodes(pugi::xml_node node, std::vector<pugi::xml_node>& result)
{
  if (!node.select_nodes("semantics").empty())
  {
    result.push_back(node);
  }

  for (auto& child: node.select_nodes("interactionClass"))
  {
    collectInteractionNodes(child.node(), result);
  }
}

[[nodiscard]] bool startsWith(const std::string& str, const std::string subStr) noexcept
{
  return str.rfind(subStr, 0) == 0;  // NOLINT
}

[[nodiscard]] bool isNumber(const std::string& s)
{
  return !s.empty() &&
         std::find_if(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c) == 0; }) == s.end();
}

[[nodiscard]] std::string toUpperCamelCase(const std::string& str)
{
  if (str.empty())
  {
    return str;
  }

  std::string result;
  result.reserve(str.size());

  for (const auto& elem: str)
  {
    if (elem != '-' && elem != '_')
    {
      result.append(1U, elem);
    }
  }

  if (!result.empty())
  {
    result.front() = static_cast<char>(std::toupper(result.front()));
  }
  return result;
}

std::vector<std::string_view> splitString(const std::string_view input, const std::string_view delimiters)
{
  std::vector<std::string_view> result;
  std::size_t lastPos = 0;
  for (std::size_t i = 0; i < input.size(); ++i)
  {
    if (delimiters.find(input[i]) != std::string::npos)
    {
      result.push_back(input.substr(lastPos, i - lastPos));
      ++i;
      lastPos = i;
    }
  }

  if (lastPos < input.size())
  {
    result.push_back(input.substr(lastPos));
  }

  return result;
}

[[nodiscard]] std::string toLowerCamelCaseHelper(const std::string& original)
{
  // split the string into 'words'
  std::vector<std::string_view> words = splitString(original, "_-");

  std::string result;
  result.reserve(original.size());

  // correct each word and append it to the result
  for (auto& word: words)
  {
    // ignore empty words
    if (word.empty())
    {
      continue;
    }

    auto prevResultSize = result.size();
    result.append(word);

    auto wordStart = result.begin() + static_cast<int>(prevResultSize);
    auto wordEnd = result.end();

    // if the word is all upper, make it all lower
    if (std::all_of(wordStart, wordEnd, [](auto c) { return std::isupper(c) || std::isdigit(c); }))
    {
      std::transform(wordStart, wordEnd, wordStart, [](auto c) { return std::tolower(c); });
    }

    // ensure the first letter is upper case
    *wordStart = static_cast<char>(std::toupper(*wordStart));
  }

  if (!result.empty() && std::isdigit(static_cast<int>(result.at(0))) != 0)
  {
    result = std::string("n").append(result);
  }

  return result;
}

[[nodiscard]] char asciiToLower(char in)
{
  if (in <= 'Z' && in >= 'A')
  {
    return in - ('Z' - 'z');  // NOLINT(bugprone-narrowing-conversions)
  }

  return in;
}

[[nodiscard]] std::string toLower(const std::string& original)
{
  std::string data(original);
  std::transform(data.begin(), data.end(), data.begin(), asciiToLower);
  return data;
}

[[nodiscard]] std::string toLowerCamelCase(const std::string& original)
{
  if (original.empty())
  {
    return original;
  }

  std::string result = toLowerCamelCaseHelper(original);
  while (std::any_of(result.begin(), result.end(), [](auto c) { return c == '_' || c == '-'; }))
  {
    result = toLowerCamelCaseHelper(result);
  }

  // ensure the first letter is lower case
  if (!result.empty())
  {
    result.front() = static_cast<char>(std::tolower(result.front()));
  }

  // ensure we are not hitting a reserved word in C++
  if (result == "true" || result == "false" || result == "class" || result == "enum" || result == "struct" ||
      result == "static")
  {
    return result + "Val";
  }

  return result;
}

/// True if a string is found inside another, ignoring case
[[nodiscard]] bool findStringIgnoreCase(const std::string& strHaystack, const std::string& strNeedle)
{
  auto it = std::search(strHaystack.begin(),
                        strHaystack.end(),
                        strNeedle.begin(),
                        strNeedle.end(),
                        [](unsigned char ch1, unsigned char ch2) { return std::toupper(ch1) == std::toupper(ch2); });
  return (it != strHaystack.end());
}

[[nodiscard]] std::string toSenPropertyName(const std::string& original)
{
  auto result = toLowerCamelCase(original);

  if (result == "name" || result == "id" || result == "localName" || result == "lastCommitTime" ||
      result == "propertyUntyped")
  {
    return result + "NonSen";
  }

  return result;
}

[[nodiscard]] std::string formatSemantics(std::string str)
{
  std::replace(str.begin(), str.end(), '\n', ' ');
  std::replace(str.begin(), str.end(), '"', '\'');
  return str;
}

void collectConstructorArgs(std::vector<Arg>& list, const ClassType& type)
{
  auto allProps = type.getProperties(ClassType::SearchMode::includeParents);
  for (const auto& prop: allProps)
  {
    if (prop->getCategory() == PropertyCategory::staticRW)
    {
      list.emplace_back(std::string(prop->getName()), std::string(prop->getDescription()), prop->getType());
    }
  }
}

void collectConstructorArgs(std::vector<Arg>& list, const ClassSpec& spec)
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

TransportMode getTransportMode(const std::string& hlaTransport)
{
  if (hlaTransport == "HLAbestEffort")
  {
    return TransportMode::multicast;
  }

  if (hlaTransport == "HLAreliable")
  {
    return TransportMode::confirmed;
  }

  std::string err;
  err.append("unknown transport mode '");
  err.append(hlaTransport);
  err.append("'");
  throwRuntimeError(err);
}

TransportMode getMethodTransportMode(const std::string& hlaTransport)
{
  if (hlaTransport == "HLAbestEffort")
  {
    return TransportMode::unicast;
  }

  if (hlaTransport == "HLAreliable")
  {
    return TransportMode::confirmed;
  }

  std::string err;
  err.append("unknown transport mode '");
  err.append(hlaTransport);
  err.append("'");
  throwRuntimeError(err);
}

TransportMode getEventTransportMode(const std::string& hlaTransport)
{
  if (hlaTransport == "HLAbestEffort")
  {
    return TransportMode::multicast;
  }

  if (hlaTransport == "HLAreliable")
  {
    return TransportMode::confirmed;
  }

  std::string err;
  err.append("unknown transport mode '");
  err.append(hlaTransport);
  err.append("'");
  throwRuntimeError(err);
}

std::string computeClassPath(const ClassType* parent, const std::string& child, const ClassType* root)
{
  std::string classPath = child;

  while (parent != root)
  {
    classPath = std::string(parent->getName()) + "." + classPath;  // NOLINT
    parent = parent->getParents().empty() ? nullptr : parent->getParents().front().type();
  }

  return classPath;
}

[[nodiscard]] bool charCompareIgnoreCase(char a, char b) noexcept
{
  return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}

[[nodiscard]] bool isTrue(std::string_view str)
{
  if (str.empty())
  {
    return false;
  }

  constexpr std::string_view trueStr("true");
  constexpr std::string_view falseStr("false");

  if (std::equal(trueStr.begin(), trueStr.end(), str.begin(), str.end(), charCompareIgnoreCase))
  {
    return true;
  }

  if (std::equal(falseStr.begin(), falseStr.end(), str.begin(), str.end(), charCompareIgnoreCase))
  {
    return false;
  }

  std::string err;
  err.append("invalid value '");
  err.append(str);
  err.append("' for a boolean parameter (expecting 'true' or 'false')");
  throwRuntimeError(err);
}

[[nodiscard]] std::string computeInteractionPath(const pugi::xpath_node& interactionNode)
{
  std::string path;
  path.reserve(256);
  auto node = interactionNode;

  while (node)
  {
    auto parentName = std::string(node.parent().child_value("name"));

    if (parentName == "HLAinteractionRoot")
    {
      break;
    }

    path =                                                  // NOLINT performance-inefficient-string-concatenation
      path.empty() ? parentName : parentName + '.' + path;  // NOLINT performance-inefficient-string-concatenation
    node = node.parent();
  }

  return path;
}

[[nodiscard]] std::vector<Arg> prependArgs(const std::vector<Arg>& currentArgs, const std::vector<Arg>& argsToPrepend)
{
  if (argsToPrepend.empty())
  {
    return currentArgs;
  }

  auto result = argsToPrepend;
  result.insert(result.end(), currentArgs.begin(), currentArgs.end());
  return result;
}

void tryToAppendMappingDepToDoc(FomDocument* doc, FomDocument* dep)
{
  auto& docDeps = doc->mappingDeps;
  if (std::find(docDeps.begin(), docDeps.end(), dep) == docDeps.end())
  {
    docDeps.push_back(dep);
  }
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// FomDocumentSet
//--------------------------------------------------------------------------------------------------------------

FomDocumentSet::FomDocumentSet(const std::vector<std::filesystem::path>& paths,
                               const std::vector<std::filesystem::path>& mappings,
                               TypeSettings settings)
  : representationToTypeMap_({{"HLAinteger16LE", Int16Type::get()},
                              {"HLAinteger16BE", Int16Type::get()},
                              {"HLAinteger32LE", Int32Type::get()},
                              {"HLAinteger32BE", Int32Type::get()},
                              {"HLAinteger64LE", Int64Type::get()},
                              {"HLAinteger64BE", Int64Type::get()},
                              {"HLAfloat32LE", Float32Type::get()},
                              {"HLAfloat32BE", Float32Type::get()},
                              {"HLAfloat64LE", Float64Type::get()},
                              {"HLAfloat64BE", Float64Type::get()},
                              {"HLAoctet", UInt8Type::get()},
                              {"HLAoctetPairBE", UInt16Type::get()},
                              {"HLAoctetPairLE", UInt16Type::get()}})
  , stringToSenUnitAbbrMap_({{"meter per second squared (m/(s^2))", "m_per_s_sq"},
                             {"degree (deg)", "deg"},
                             {"radian (rad)", "rad"},
                             {"radian per second (rad/s)", "rad_per_s"},
                             {"meter (m)", "m"},
                             {"hertz (Hz)", "hz"},
                             {"interrogations/second", "hz"},
                             {"kilogram (kg)", "kg"},
                             {"revolutions per minute (RPM)", "rpm"},
                             {"degree Celsius (C)", "degC"},
                             {"microsecond", "us"},
                             {"millisecond (ms)", "ms"},
                             {"second (s)", "s"},
                             {"meter per second (m/s)", "m_per_s"},
                             {"micron", "um"},
                             {"RPM", "rpm"},
                             {"decimeter per second (dm/s)", "dm_per_s"}})
  , settings_(std::move(settings))
{
  // create the root type set used for objects inheriting from HLAObjectRoot
  createRootTypeSet();

  for (const auto& mappingDoc: mappings)
  {
    mappingsDocs_.emplace_back();
    auto result = mappingsDocs_.back().load_file(mappingDoc.c_str());
    if (!result)
    {
      std::string err;
      err.append(result.description());
      err.append(" (offset ");
      err.append(std::to_string(result.offset));
      err.append(")");
      throwRuntimeError(err);
    }
  }

  for (const auto& path: paths)
  {
    readDocs(path);
  }

  resolveDeps();

  // ensure all documents are parsed
  for (const auto& doc: docs_)
  {
    std::ignore = getParsedDoc(doc.get());
  }
}

void FomDocumentSet::createRootTypeSet()
{
  {
    // inject parent class with HLA RTI object Id
    ClassSpec hlaBaseSpec {};
    hlaBaseSpec.isInterface = false;
    hlaBaseSpec.name = "ObjectRoot";
    hlaBaseSpec.qualifiedName = "hla.ObjectRoot";
    hlaBaseSpec.description = formatSemantics("HLA Base Class that contains the RTI object ID");

    auto propertySpecName = toSenPropertyName("RtiId");
    auto propertySpecType = MetaTypeTrait<std::string>::meta();
    auto propertySpecDescription = formatSemantics("HLA RTI object ID");
    auto propertySpecCategory = PropertyCategory::dynamicRW;
    auto propertySpecTransportMode = TransportMode::confirmed;

    hlaBaseSpec.properties.emplace_back(std::move(propertySpecName),
                                        std::move(propertySpecDescription),
                                        std::move(propertySpecType),
                                        propertySpecCategory,
                                        propertySpecTransportMode);

    CallableSpec constructorCallableSpec;
    collectConstructorArgs(constructorCallableSpec.args, hlaBaseSpec);
    constructorCallableSpec.name = std::string("constructor") + hlaBaseSpec.name;
    constructorCallableSpec.description = "constructor";
    auto constructorSpecReturnType = sen::VoidType::get();
    auto constructorSpecConstness = Constness::nonConstant;

    MethodSpec constructorSpec(constructorCallableSpec, constructorSpecReturnType, constructorSpecConstness);
    hlaBaseSpec.constructor = std::move(constructorSpec);

    rootClass_ = ClassType::make(hlaBaseSpec);
  }

  rootTypeSet_ = std::make_unique<TypeSet>();
  {
    rootTypeSet_->fileName = "hla.stl";
    rootTypeSet_->package = {"hla"};
    rootTypeSet_->types.emplace_back(rootClass_.value());
  }

  // define optional native types
  for (const auto& elem: getNativeTypes())
  {
    if (elem->isVoidType())
    {
      continue;
    }
    std::string senTypeNameUnequal = std::string("Maybe") + toUpperCamelCase(std::string(elem->getName()));

    OptionalSpec spec(senTypeNameUnequal, std::string("hla.") + senTypeNameUnequal, "" /* not present in HLA */, elem);

    rootTypeSet_->types.emplace_back(OptionalType::make(spec));
  }
}

std::map<const FomDocument*, std::unique_ptr<lang::TypeSet>> FomDocumentSet::computeTypeSets() const
{
  std::map<const FomDocument*, std::unique_ptr<lang::TypeSet>> typesPerDoc;

  // compile all except the dependencies
  for (const auto& elem: parsedDocs_)
  {
    auto doc = elem.second.get();

    auto typeSet = std::make_unique<lang::TypeSet>();
    typeSet->fileName = toLower(doc->doc->filePath.filename().generic_string());
    typeSet->parentDirectory = toLower(doc->doc->filePath.parent_path().filename().generic_string());
    typeSet->package.push_back(doc->doc->packageName);

    // add the types
    typeSet->types.reserve(doc->storage.size());

    auto orderedList = doc->registry.getAllInOrder();
    for (auto& type: orderedList)
    {
      auto isSame = [&](const auto& other)
      {
        auto custom = other->asCustomType();
        return custom != nullptr && custom->getQualifiedName() == type->getQualifiedName();
      };

      // add the type only if present in the current type set
      if (std::find_if(doc->storage.begin(), doc->storage.end(), isSame) != doc->storage.end())
      {
        auto maybeFoundType = doc->registry.get(std::string(type->getQualifiedName()));
        SEN_ASSERT(maybeFoundType.has_value());
        typeSet->types.emplace_back(dynamicTypeHandleCast<const CustomType>(std::move(maybeFoundType).value()).value());
      }
    }

    typesPerDoc.insert({doc->doc, std::move(typeSet)});
  }

  // resolve the dependencies and build the result
  std::vector<std::unique_ptr<const lang::TypeSet>> result;
  for (const auto& elem: typesPerDoc)
  {
    auto doc = elem.first;
    const auto& typeset = elem.second;

    for (const auto& dep: doc->deps)
    {
      typeset->importedSets.push_back(typesPerDoc[dep].get());
    }

    for (const auto& dep: doc->mappingDeps)
    {
      if (std::find(doc->deps.begin(), doc->deps.end(), dep) == doc->deps.end())
      {
        typeset->importedSets.push_back(typesPerDoc[dep].get());
      }
    }

    typeset->importedSets.push_back(rootTypeSet_.get());
  }

  return typesPerDoc;
}

std::optional<std::pair<ConstTypeHandle<>, ParsedDoc*>> FomDocumentSet::searchTypeSenName(
  const std::string& qualName) const
{
  for (const auto& doc: parsedDocs_)
  {
    if (auto type = doc.second->registry.get(qualName))
    {
      return {std::make_pair(type.value(), doc.second.get())};
    }
  }

  for (const auto& nativeType: getNativeTypes())
  {
    if (nativeType->getName() == qualName)
    {
      return {std::make_pair(nativeType, nullptr)};
    }
  }

  for (const auto& elem: rootTypeSet_->types)
  {
    if (elem->getQualifiedName() == qualName)
    {
      return {std::make_pair(elem, nullptr)};
    }
  }

  return std::nullopt;
}

std::optional<std::pair<ConstTypeHandle<>, ParsedDoc*>> FomDocumentSet::searchTypeFromFomNameHelper(
  const std::string& fomTypeName,
  ParsedDoc* doc)
{
  // look for the type in the doc
  for (auto senType: doc->storage)
  {
    if (senType->getName() == fomTypeName)
    {
      return {std::make_pair(std::move(senType), toMutableParsedDoc(doc))};
    }
  }

  // look for the type in the doc dependencies
  for (const auto& dep: doc->doc->deps)
  {
    if (auto lookup = searchTypeFromFomNameHelper(fomTypeName, getParsedDoc(dep)); lookup)
    {
      return lookup;
    }
  }

  return std::nullopt;
}

std::optional<std::pair<ConstTypeHandle<>, ParsedDoc*>> FomDocumentSet::searchTypeFromFomName(
  const std::string& fomTypeName,
  ParsedDoc* doc)
{
  if (auto lookup = searchTypeFromFomNameHelper(fomTypeName, doc))
  {
    return lookup;
  }

  return searchTypeSenName(fomTypeNameToSenTypeNameUnequal(fomTypeName));
}

void FomDocumentSet::readDocs(const std::filesystem::path& path)
{
  const auto packageName = toLower(path.stem().generic_string());

  for (const auto& entry: std::filesystem::directory_iterator {path})
  {
    if (entry.is_regular_file() && entry.path().extension() == ".xml")
    {
      auto docData = std::make_unique<FomDocument>();
      auto result = docData->xmlDoc.load_file(entry.path().c_str());
      if (!result)
      {
        std::string err;
        err.append(result.description());
        err.append(" (offset ");
        err.append(std::to_string(result.offset));
        err.append(")");
        throwRuntimeError(err);
      }

      docData->filePath = entry.path();
      docData->packageName = packageName;
      docData->name = docData->xmlDoc.select_node("//objectModel/modelIdentification").node().child_value("name");

      // save the leaf class nodes
      for (auto& rootClassNode: docData->xmlDoc.select_nodes("//objectModel/objects/objectClass"))
      {
        collectClassesNodes(rootClassNode.node(), docData->definedClassesNodes);
      }

      // save the leaf interaction nodes
      for (auto& rootClassNode: docData->xmlDoc.select_nodes("//objectModel/interactions/interactionClass"))
      {
        collectInteractionNodes(rootClassNode.node(), docData->definedInteractionNodes);
      }

      docs_.push_back(std::move(docData));
    }
  }
}

FomDocument* FomDocumentSet::searchDocByIdentification(const std::string& identification)
{
  for (auto& doc: docs_)
  {
    if (findStringIgnoreCase(doc->name, identification))
    {
      return doc.get();
    }
    if (findStringIgnoreCase(doc->filePath.stem().generic_string(), identification))
    {
      return doc.get();
    }
  }
  return {};
}

void FomDocumentSet::resolveDeps()
{
  for (auto& docData: docs_)
  {
    pugi::xpath_node_set depNodes =
      docData->xmlDoc.select_nodes("//objectModel/modelIdentification/reference[contains(type, 'Dependency')]");

    for (const auto& depNode: depNodes)
    {
      std::string depId = depNode.node().child_value("identification");

      // ignore dependencies to the MIM
      if (depId == "MIM")
      {
        continue;
      }

      auto depPtr = searchDocByIdentification(depId);
      if (!depPtr)
      {
        std::string err;
        err.append("could not find dependent document '");
        err.append(depId);
        err.append("' for '");
        err.append(docData->filePath.string());
        err.append("'");
        throwRuntimeError(err);
      }

      docData->deps.push_back(depPtr);
    }
  }
}

ParsedDoc* FomDocumentSet::getParsedDoc(FomDocument* doc)
{
  auto itr = parsedDocs_.find(doc);
  if (itr != parsedDocs_.end())
  {
    return itr->second.get();
  }

  // ensure the dependencies are parsed
  for (const auto& dep: doc->deps)
  {
    std::ignore = getParsedDoc(dep);
  }

  // parse the document itself
  auto [resultItr, inserted] = parsedDocs_.insert({doc, std::make_unique<ParsedDoc>()});
  resultItr->second->doc = doc;
  populateTypes(resultItr->second.get(), doc);
  return resultItr->second.get();
}

void FomDocumentSet::populateTypes(ParsedDoc* result, FomDocument* doc)
{
  for (const auto& node: doc->xmlDoc.select_nodes("//objectModel/dataTypes/basicDataRepresentations/basicData"))
  {
    basicDataRepresentation(node);
  }

  processTypeSource(result,
                    doc->xmlDoc.select_nodes("//objectModel/dataTypes/simpleDataTypes/simpleData"),
                    [this, result](const pugi::xpath_node& node) { return simpleData(node, result); });

  processTypeSource(result,
                    doc->xmlDoc.select_nodes("//objectModel/dataTypes/arrayDataTypes/arrayData"),
                    [this, result](const pugi::xpath_node& node) { return arrayType(node, result); });

  processTypeSource(result,
                    doc->xmlDoc.select_nodes("//objectModel/dataTypes/fixedRecordDataTypes/fixedRecordData"),
                    [this, result](const pugi::xpath_node& node) { return recordType(node, result); });

  processTypeSource(result,
                    doc->xmlDoc.select_nodes("//objectModel/dataTypes/variantRecordDataTypes/variantRecordData"),
                    [this, result](const pugi::xpath_node& node) { return variantRecord(node, result); });

  processTypeSource(result,
                    doc->xmlDoc.select_nodes("//objectModel/dataTypes/enumeratedDataTypes/enumeratedData"),
                    [this, result](const pugi::xpath_node& node) { return enumeration(node, result); });

  for (const auto& leafNode: doc->definedClassesNodes)
  {
    std::ignore = classNode(leafNode, result);
  }

  for (const auto& node: doc->definedInteractionNodes)
  {
    std::ignore = getOrMakeStructFromInteraction(node, result, {});
  }
}

ConstTypeHandle<> FomDocumentSet::storeType(ConstTypeHandle<> type, ParsedDoc* doc)
{
  // first check if the type is already defined
  if (auto custom = type->asCustomType())
  {
    if (auto found = doc->registry.get(std::string(custom->getQualifiedName())))
    {
      return std::move(found).value();
    }
  }

  doc->storage.push_back(type);
  auto typePtr = doc->storage.back();
  doc->registry.add(typePtr);

  return typePtr;
}

template <typename F>
ConstTypeHandle<> FomDocumentSet::processTypeSource(ParsedDoc* result, const pugi::xpath_node& node, F func)
{
  const std::string typeName = node.node().child_value("name");

  // only store the type if not done so before
  auto itr = std::find_if(
    result->storage.begin(), result->storage.end(), [&](auto& elem) { return elem->getName() == typeName; });
  if (itr == result->storage.end())
  {
    return storeType(func(node), result);
  }
  return *itr;
}

template <typename F>
void FomDocumentSet::processTypeSource(ParsedDoc* result, const pugi::xpath_node_set& nodes, F func)
{
  for (const auto& elem: nodes)
  {
    std::ignore = processTypeSource(result, elem, func);
  }
}

template <typename F>
std::optional<ConstTypeHandle<>> FomDocumentSet::processTypeQuery(ParsedDoc* result,
                                                                  const std::string& query,
                                                                  const std::string& queryPostFix,
                                                                  F func)
{
  std::string finalQuery = query;
  finalQuery.append(queryPostFix);

  auto nodes = result->doc->xmlDoc.select_nodes(finalQuery.c_str());

  if (!nodes.empty())
  {
    return processTypeSource(result, nodes.first(), func);
  }

  return std::nullopt;
}

std::pair<ConstTypeHandle<>, ParsedDoc*> FomDocumentSet::getOrCreateTypeFromFomName(const std::string& fomName,
                                                                                    ParsedDoc* result)
{
  // search for already parsed types
  auto lookup = searchTypeFromFomName(fomName, result);
  if (lookup)
  {
    return std::move(lookup).value();
  }

  // not parsed yet, has to be somewhere in this document
  using Func = std::function<ConstTypeHandle<>(const pugi::xpath_node&)>;
  using Check = std::pair<std::string, Func>;

  // types in this document
  std::vector<Check> checks {{"//objectModel/dataTypes/simpleDataTypes/simpleData",
                              [this, result](const pugi::xpath_node& node) { return simpleData(node, result); }},
                             {"//objectModel/dataTypes/arrayDataTypes/arrayData",
                              [this, result](const pugi::xpath_node& node) { return arrayType(node, result); }},
                             {"//objectModel/dataTypes/fixedRecordDataTypes/fixedRecordData",
                              [this, result](const pugi::xpath_node& node) { return recordType(node, result); }},
                             {"//objectModel/dataTypes/variantRecordDataTypes/variantRecordData",
                              [this, result](const pugi::xpath_node& node) { return variantRecord(node, result); }},
                             {"//objectModel/dataTypes/enumeratedDataTypes/enumeratedData",
                              [this, result](const pugi::xpath_node& node) { return enumeration(node, result); }}};

  const auto condition = std::string("[name = '") + fomName + "']";
  for (auto& check: checks)
  {
    if (auto maybeType = processTypeQuery(result, check.first, condition, check.second))
    {
      return {std::move(maybeType).value(), toMutableParsedDoc(result)};
    }
  }

  if (auto type = result->registry.get(fomTypeNameToSenTypeNameQual(result->doc, fomName)))
  {
    return {std::move(type).value(), toMutableParsedDoc(result)};
  }

  // classes in this document
  for (const auto& definedClassNode: result->doc->definedClassesNodes)
  {
    if (std::string(definedClassNode.child_value("name")) == fomName)
    {
      return {storeType(classNode(definedClassNode, result), result), toMutableParsedDoc(result)};
    }
  }

  // look in the doc dependencies
  for (auto* doc: result->doc->deps)
  {
    auto parsedDoc = getParsedDoc(doc);
    if (parsedDoc != result)
    {
      try
      {
        return getOrCreateTypeFromFomName(fomName, parsedDoc);
      }
      catch (const std::exception& err)
      {
        std::ignore = err;
      }
    }
  }

  std::string err;
  err.append("could not find type '");
  err.append(fomName);
  err.append("'");
  throwRuntimeError(err);
}

ConstTypeHandle<> FomDocumentSet::simpleData(const pugi::xpath_node& node, ParsedDoc* doc)
{
  std::string name = node.node().child_value("name");
  std::string rep = node.node().child_value("representation");
  std::string unit = node.node().child_value("units");

  if (!unit.empty() && unit != "NA")
  {
    auto specElementType = dynamicTypeHandleCast<const NumericType>(fomDataRepresentationToSenType(rep));

    if (!specElementType)
    {
      std::string err;
      err.append(" representation '");
      err.append(rep);
      err.append("' for quantity '");
      err.append(name);
      err.append("' is not numeric");
      throwRuntimeError(err);
    }

    auto specName = fomTypeNameToSenTypeNameUnequal(name);
    auto specQualifiedName = fomTypeNameToSenTypeNameQual(doc->doc, specName);
    auto specDescription = formatSemantics(node.node().child_value("semantics"));

    // compute the unit
    std::string senUnitAbbreviation;
    {
      auto itr = stringToSenUnitAbbrMap_.find(unit);
      if (itr != stringToSenUnitAbbrMap_.end())
      {
        senUnitAbbreviation = itr->second;
      }
    }

    if (!senUnitAbbreviation.empty())
    {
      auto unitLookup = UnitRegistry::get().searchUnitByAbbreviation(senUnitAbbreviation);
      if (!unitLookup.has_value())
      {
        std::string err;
        err.append("invalid sen unit abbreviation '");
        err.append(senUnitAbbreviation);
        err.append("' currently mapped to '");
        err.append(unit);
        err.append("'. Check fom_document_set.cpp");

        throw std::logic_error(err);
      }

      auto specUnit = unitLookup;

      return QuantityType::make(
        QuantitySpec(specName, specQualifiedName, specDescription, std::move(specElementType).value(), specUnit));
    }
  }

  {
    auto specName = fomTypeNameToSenTypeNameUnequal(name);
    return AliasType::make(AliasSpec(specName,
                                     fomTypeNameToSenTypeNameQual(doc->doc, specName),
                                     formatSemantics(node.node().child_value("semantics")),
                                     fomDataRepresentationToSenType(rep)));
  }
}

ConstTypeHandle<> FomDocumentSet::arrayType(const pugi::xpath_node& node, const ParsedDoc* doc)
{
  std::string name = node.node().child_value("name");
  std::string elementType = node.node().child_value("dataType");
  std::string cardinalityStr = node.node().child_value("cardinality");

  if ((elementType == "HLAASCIIchar" || elementType == "HLAunicodeChar") && cardinalityStr == "Dynamic")
  {
    return StringType::get();
  }

  auto specElementType = getOrCreateTypeFromFomName(elementType, toMutableParsedDoc(doc)).first;
  auto specName = fomTypeNameToSenTypeNameUnequal(name);
  auto specQualifiedName = fomTypeNameToSenTypeNameQual(doc->doc, specName);
  auto specDescription = formatSemantics(node.node().child_value("semantics"));
  std::optional<size_t> specMaxSize;
  if (isNumber(cardinalityStr))
  {
    char* pend = nullptr;
    specMaxSize = std::strtol(cardinalityStr.c_str(), &pend, 10);
  }
  else if (!cardinalityStr.empty())
  {
    auto const regex = std::regex(R"(\[(0|[1-9][0-9]*)\.+(0|[1-9][0-9]*)\])");
    auto matchResults = std::smatch {};
    std::regex_match(cardinalityStr, matchResults, regex);

    if (matchResults.size() == 3U)
    {
      char* pend = nullptr;
      auto rangeMax = std::strtoul(matchResults[2].str().c_str(), &pend, 10);
      if (rangeMax < maxStaticVectorSize)
      {
        specMaxSize = rangeMax;
      }
    }
  }

  return SequenceType::make(SequenceSpec(specName, specQualifiedName, specDescription, specElementType, specMaxSize

                                         ));
}

ConstTypeHandle<> FomDocumentSet::recordType(const pugi::xpath_node& node, ParsedDoc* doc)
{
  std::string name = node.node().child_value("name");

  auto specName = fomTypeNameToSenTypeNameUnequal(name);
  auto specQualifiedName = fomTypeNameToSenTypeNameQual(doc->doc, specName);
  auto specDescription = formatSemantics(node.node().child_value("semantics"));

  std::vector<StructField> fields;
  for (const auto& fieldNode: node.node().children("field"))
  {
    StructField field(toLowerCamelCase(fieldNode.child_value("name")),
                      formatSemantics(fieldNode.child_value("semantics")),
                      getOrCreateTypeFromFomName(fieldNode.child_value("dataType"), doc).first);

    fields.push_back(std::move(field));
  }

  StructSpec spec(specName, specQualifiedName, specDescription, fields, std::nullopt);

  return StructType::make(spec);
}

ConstTypeHandle<> FomDocumentSet::variantRecord(const pugi::xpath_node& node, const ParsedDoc* doc)
{
  std::string name = node.node().child_value("name");

  VariantSpec spec {};
  spec.name = fomTypeNameToSenTypeNameUnequal(name);
  spec.qualifiedName = fomTypeNameToSenTypeNameQual(doc->doc, spec.name);
  spec.description = formatSemantics(node.node().child_value("semantics"));

  uint32_t key = 0U;
  for (const auto& alternativeNode: node.node().children("alternative"))
  {
    auto dataType = alternativeNode.child_value("dataType");

    auto fieldType = getOrCreateTypeFromFomName(dataType, toMutableParsedDoc(doc)).first;
    if (fieldType->isVoidType())
    {
      std::string err;
      err.append("field type '");
      err.append(alternativeNode.child_value("name"));
      err.append("' for variant '");
      err.append(name);
      err.append("' is void type");
      throwRuntimeError(err);
    }

    VariantField field(key++, formatSemantics(alternativeNode.child_value("semantics")), fieldType);
    spec.fields.push_back(std::move(field));
  }

  return VariantType::make(spec);
}

ConstTypeHandle<> FomDocumentSet::enumeration(const pugi::xpath_node& node, const ParsedDoc* doc)
{
  std::string name = node.node().child_value("name");
  std::string storageRep = node.node().child_value("representation");

  if (name == "RPRboolean")  // RPRboolean is a dumb enum
  {
    return BoolType::get();
  }

  auto integralType = fomDataRepresentationToSenType(storageRep);
  if (!integralType->isIntegralType())
  {
    std::string err;
    err.append("storage type '");
    err.append(storageRep);
    err.append("' for enumeration '");
    err.append(name);
    err.append("' is not an integral type");
    throwRuntimeError(err);
  }
  auto specName = fomTypeNameToSenTypeNameUnequal(name);
  auto specQualifiedName = fomTypeNameToSenTypeNameQual(doc->doc, specName);
  auto specDescription = formatSemantics(node.node().child_value("semantics"));
  auto specStorageType = dynamicTypeHandleCast<const IntegralType>(integralType);

  std::vector<Enumerator> enums;
  std::map<std::string, uint32_t> collectedSenEnumerators;
  for (const auto& enumeratorNode: node.node().children("enumerator"))
  {
    const std::string senEnumeratorName = toLowerCamelCase(enumeratorNode.child_value("name"));

    Enumerator enumerator {};

    // compute the name and add a counter to the very rare repeated enums
    {
      const auto count = collectedSenEnumerators[senEnumeratorName];
      if (count == 0)
      {
        enumerator.name = senEnumeratorName;
      }
      else
      {
        enumerator.name = senEnumeratorName + std::to_string(count);
      }
    }

    enumerator.key = std::atol(enumeratorNode.child_value("value"));  // NOLINT
    enums.push_back(std::move(enumerator));
    collectedSenEnumerators[senEnumeratorName]++;
  }

  EnumSpec spec(specName, specQualifiedName, specDescription, std::move(enums), std::move(specStorageType).value());

  return EnumType::make(spec);
}

ConstTypeHandle<> FomDocumentSet::getOptionalPropertyType(const std::string fomElementTypeName, ParsedDoc* doc)
{
  auto senTypeNameUnequal =
    std::string("Maybe") + toUpperCamelCase(fomTypeNameToSenTypeNameUnequal(fomElementTypeName));

  if (auto typeLookup = searchTypeFromFomName(senTypeNameUnequal, doc))
  {
    return typeLookup.value().first;
  }

  auto [elementType, elementDoc] = getOrCreateTypeFromFomName(fomElementTypeName, doc);

  if (elementDoc)
  {
    auto* aliasType = elementType->asAliasType();
    bool isAliasOfNative = elementType->isAliasType() && !aliasType->getAliasedType()->isCustomType();

    if (!elementType->isCustomType() || isAliasOfNative)
    {
      auto elementTypeToSearch = isAliasOfNative ? aliasType->getAliasedType() : elementType;

      for (const auto& [name, representedType]: representationToTypeMap_)
      {
        if (representedType->getName() == elementTypeToSearch->getName())
        {
          elementType = representedType;
          break;
        }
      }

      senTypeNameUnequal = std::string("Maybe") + toUpperCamelCase(std::string(elementType->getName()));
      for (const auto& rootType: rootTypeSet_->types)
      {
        if (rootType->getName() == senTypeNameUnequal)
        {
          return rootType;
        }
      }
    }
    else
    {
      // custom or not native

      // store it together in the document where the custom type lives
      OptionalSpec spec(
        senTypeNameUnequal, prependSenPackageName(elementDoc->doc, senTypeNameUnequal), "", elementType);

      return storeType(OptionalType::make(spec), elementDoc);
    }

    throw std::runtime_error("could not find container for native optional type");
  }

  OptionalSpec spec(senTypeNameUnequal, std::string("hla.") + senTypeNameUnequal, "", elementType);

  return storeType(OptionalType::make(spec), elementDoc);
}

// NOLINTNEXTLINE(readability-function-size)
ConstTypeHandle<> FomDocumentSet::classNode(const pugi::xpath_node& node, ParsedDoc* doc)
{
  const std::string fomClassName = node.node().child_value("name");

  MaybeConstTypeHandle<> parentType = std::nullopt;
  auto parent = node.parent();
  if (!parent.empty() && std::string(parent.name()) == "objectClass")
  {
    const std::string parentFomClassName = parent.child_value("name");

    if (parentFomClassName == "HLAobjectRoot")
    {
      parentType = rootClass_.value();
    }
    else
    {
      parentType = getOrCreateTypeFromFomName(parentFomClassName, doc).first;
    }
  }

  ClassSpec classSpec {};
  classSpec.isInterface = false;
  classSpec.name = fomTypeNameToSenTypeNameUnequal(fomClassName);
  classSpec.qualifiedName = fomTypeNameToSenTypeNameQual(doc->doc, classSpec.name);
  classSpec.description = formatSemantics(node.node().child_value("semantics"));
  classSpec.parents.emplace_back(dynamicTypeHandleCast<const ClassType>(parentType.value()).value());

  auto classAnnotationItr = settings_.classAnnotations.find(classSpec.qualifiedName);

  std::vector<PropertySpec> propertySpecs;
  const auto xmlAttributes = node.node().children("attribute");
  propertySpecs.reserve(std::distance(xmlAttributes.begin(), xmlAttributes.end()));

  for (const auto& attrNode: xmlAttributes)
  {
    auto propertySpecName = toSenPropertyName(attrNode.child_value("name"));

    std::string semanticsString = attrNode.child_value("semantics");

    auto propertySpecType = [&]()
    {
      if (startsWith(semanticsString, "Optional.") || startsWith(semanticsString, "Optional (") ||
          startsWith(semanticsString, "Optional:") || startsWith(semanticsString, "Optional,"))
      {
        return getOptionalPropertyType(attrNode.child_value("dataType"), doc);
      }

      return getOrCreateTypeFromFomName(attrNode.child_value("dataType"), doc).first;
    }();

    auto propertySpecDescription = formatSemantics(semanticsString);
    auto propertySpecCategory = PropertyCategory::dynamicRO;
    auto propertySpecTransportMode = getTransportMode(attrNode.child_value("transportation"));

    // compute category
    {
      const std::string updateStr = attrNode.child_value("updateType");
      if (updateStr == "Static")
      {
        const std::string sharingStr = attrNode.child_value("sharing");
        if (sharingStr == "PublishSubscribe" || sharingStr == "Publish")
        {
          propertySpecCategory = PropertyCategory::staticRW;
        }
        else if (sharingStr == "Subscribe" || sharingStr == "Neither")
        {
          propertySpecCategory = PropertyCategory::staticRO;
        }
        else
        {
          std::string err;
          err.append("unknown sharing mode '");
          err.append(sharingStr);
          err.append("'");
          throwRuntimeError(err);
        }
      }
      else
      {
        const std::string sharingStr = attrNode.child_value("sharing");
        if (sharingStr == "Writeable")
        {
          propertySpecCategory = PropertyCategory::dynamicRW;
        }
      }
    }

    // check if the property is marked as checked
    bool propertySpecCheckedSet = classAnnotationItr != settings_.classAnnotations.end() &&
                                  classAnnotationItr->second.checkedProperties.count(propertySpecName) != 0;

    propertySpecs.emplace_back(std::move(propertySpecName),
                               std::move(propertySpecDescription),
                               std::move(propertySpecType),
                               propertySpecCategory,
                               propertySpecTransportMode,
                               propertySpecCheckedSet);
  }

  // check for mappings
  if (!mappingsDocs_.empty())
  {
    std::string query = "/senMapping/class[@name=\"";
    query.append(
      computeClassPath(parentType.value()->asClassType(), classSpec.name, rootClass_.value()->asClassType()));
    query.append("\"]");

    for (auto& mappingDoc: mappingsDocs_)
    {
      auto classMappings = mappingDoc.select_nodes(query.c_str());
      for (const auto& mapping: classMappings)
      {
        // inspect properties
        for (const auto& propertyMapping: mapping.node().select_nodes("property"))
        {
          if (auto itr = std::find_if(propertySpecs.begin(),
                                      propertySpecs.end(),
                                      [&propertyMapping](const auto& elem)
                                      { return elem.name == propertyMapping.node().attribute("name").value(); });
              itr != propertySpecs.end())
          {
            // check if the property is defined as writable in the mappings
            if (isTrue(propertyMapping.node().attribute("writable").value()))
            {
              itr->category = PropertyCategory::dynamicRW;
            }

            // check if the property is defined as  in the mappings
            if (!propertyMapping.node().attribute("checked").empty())
            {
              std::cerr << "Warning: property " << itr->name
                        << " is mapped as 'checked'. This is now deprecated. Please use code generation settings.\n";

              itr->checkedSet = isTrue(propertyMapping.node().attribute("checked").value());
            }
          }
        }

        // inspect events
        for (const auto& eventMapping: mapping.node().select_nodes("event"))
        {
          std::vector<std::string> ignoredAttributes;
          for (const auto& ignoreNode: eventMapping.node().select_nodes("ignore"))
          {
            ignoredAttributes.emplace_back(ignoreNode.node().attribute("parameter").value());
          }

          auto interaction = findInteractionNodeInAllDocs(eventMapping.node().attribute("hlaInteraction").value());
          tryToAppendMappingDepToDoc(doc->doc, interaction.doc->doc);

          const auto packed = isTrue(eventMapping.node().attribute("pack").value());

          classSpec.events.push_back(makeEventSpec(interaction.node, ignoredAttributes, interaction.doc, packed));
        }

        // inspect methods
        for (const auto& methodMapping: mapping.node().select_nodes("method"))
        {
          auto interactionPath = methodMapping.node().attribute("hlaInteraction").value();
          auto interaction = findInteractionNodeInAllDocs(interactionPath);
          tryToAppendMappingDepToDoc(doc->doc, interaction.doc->doc);

          if (interaction.node)
          {
            std::vector<std::string> ignoredAttributes;
            for (const auto& ignoreNode: methodMapping.node().select_nodes("ignore"))
            {
              ignoredAttributes.emplace_back(ignoreNode.node().attribute("parameter").value());
            }

            auto returnNode = methodMapping.node().select_node("return");
            const auto packed = isTrue(methodMapping.node().attribute("pack").value());
            const auto local = isTrue(methodMapping.node().attribute("local").value());

            classSpec.methods.push_back(makeMethodSpec(interaction.node,
                                                       returnNode,
                                                       ignoredAttributes,
                                                       interaction.doc,
                                                       packed,
                                                       local,
                                                       classSpec.qualifiedName));
          }
          else
          {
            std::string err;
            err.append("could not find interaction '");
            err.append(interactionPath);
            err.append("' while processing the mapping for class '");
            err.append(classSpec.name);
            err.append("'");
            throwRuntimeError(err);
          }
        }
      }
    }
  }

  // add modified properties to the classSpec
  classSpec.properties = std::move(propertySpecs);

  // constructor
  {
    CallableSpec constructorCallableSpec;
    collectConstructorArgs(constructorCallableSpec.args, classSpec);
    constructorCallableSpec.name = std::string("constructor") + classSpec.name;
    constructorCallableSpec.description = "constructor";

    auto constructorSpecReturnType = sen::VoidType::get();
    auto constructorSpecConstness = Constness::nonConstant;

    MethodSpec constructorSpec(constructorCallableSpec, constructorSpecReturnType, constructorSpecConstness);
    classSpec.constructor = std::move(constructorSpec);
  }

  return storeType(ClassType::make(classSpec), doc);
}

void FomDocumentSet::basicDataRepresentation(const pugi::xpath_node& node)
{
  std::string name = node.node().child_value("name");
  std::string encoding = node.node().child_value("encoding");

  if (startsWith(encoding, "8-bit unsigned integer"))
  {
    representationToTypeMap_.insert({name, UInt8Type::get()});
  }
  else if (startsWith(encoding, "16-bit unsigned integer"))
  {
    representationToTypeMap_.insert({name, UInt16Type::get()});
  }
  else if (startsWith(encoding, "32-bit unsigned integer"))
  {
    representationToTypeMap_.insert({name, UInt32Type::get()});
  }
  else if (startsWith(encoding, "64-bit unsigned integer"))
  {
    representationToTypeMap_.insert({name, UInt64Type::get()});
  }
  else if (startsWith(encoding, "16-bit signed integer"))
  {
    representationToTypeMap_.insert({name, Int16Type::get()});
  }
  else if (startsWith(encoding, "32-bit signed integer"))
  {
    representationToTypeMap_.insert({name, Int32Type::get()});
  }
  else if (startsWith(encoding, "64-bit signed integer"))
  {
    representationToTypeMap_.insert({name, Int64Type::get()});
  }
  else
  {
    std::string err;
    err.append("unknown encoding '");
    err.append(encoding);
    err.append("' for basic data representation '");
    err.append(name);
    err.append("'");
    throwRuntimeError(err);
  }
}

std::string FomDocumentSet::fomTypeNameToSenTypeNameUnequal(const std::string& fomTypeName) const
{
  if (fomTypeName == "HLAASCIIchar")
  {
    return fomDataRepresentationToSenType("HLAoctet")->getName().data();
  }
  if (fomTypeName == "HLAunicodeChar")
  {
    return fomDataRepresentationToSenType("HLAoctetPairBE")->getName().data();
  }
  if (fomTypeName == "HLAbyte")
  {
    return fomDataRepresentationToSenType("HLAoctet")->getName().data();
  }
  if (fomTypeName == "HLAcount")
  {
    return fomDataRepresentationToSenType("HLAinteger32BE")->getName().data();
  }
  if (fomTypeName == "HLAseconds")
  {
    return fomDataRepresentationToSenType("HLAinteger32BE")->getName().data();
  }
  if (fomTypeName == "HLAmsec")
  {
    return fomDataRepresentationToSenType("HLAinteger32BE")->getName().data();
  }
  if (fomTypeName == "HLAindex")
  {
    return fomDataRepresentationToSenType("HLAinteger32BE")->getName().data();
  }
  if (fomTypeName == "HLAinteger64Time")
  {
    return fomDataRepresentationToSenType("HLAinteger64BE")->getName().data();
  }
  if (fomTypeName == "HLAfloat64Time")
  {
    return fomDataRepresentationToSenType("HLAfloat64BE")->getName().data();
  }
  if (fomTypeName == "HLAboolean")
  {
    return std::string(BoolType::get()->getName());
  }
  if (fomTypeName == "HLAASCIIstring")
  {
    return std::string(StringType::get()->getName());
  }
  if (fomTypeName == "HLAunicodeString")
  {
    return std::string(StringType::get()->getName());
  }

  return toUpperCamelCase(fomTypeName);
}

std::string FomDocumentSet::fomTypeNameToSenTypeNameQual(const FomDocument* doc, const std::string& fomTypeName) const
{
  return prependSenPackageName(doc, fomTypeNameToSenTypeNameUnequal(fomTypeName));
}

ConstTypeHandle<> FomDocumentSet::fomDataRepresentationToSenType(const std::string& representation) const
{
  auto itr = representationToTypeMap_.find(representation);
  if (itr == representationToTypeMap_.end())
  {
    std::string err;
    err.append("data representation '");
    err.append(representation);
    err.append("' is not yet supported");
    throwRuntimeError(err);
  }

  return itr->second;
}

std::string FomDocumentSet::prependSenPackageName(const FomDocument* doc, const std::string& str) const
{
  std::string result;
  result.append(doc->packageName);
  result.append(".");
  result.append(str);
  return result;
}

ConstTypeHandle<> FomDocumentSet::getOrMakeStructFromInteraction(const pugi::xpath_node& node,
                                                                 ParsedDoc* doc,
                                                                 const std::vector<std::string>& ignoredParams)
{
  std::string name = toUpperCamelCase(node.node().child_value("name"));

  // check if the struct was already created
  if (auto typeLookup = searchTypeFromFomName(name, doc))
  {
    return std::move(typeLookup).value().first;
  }

  // NOTE: Some NETN interactions have matching qualified names (e.g. netn::CapabilitiesSupported). In order to prevent
  // a multiple definition of these types in the generated code, we do the following:
  //  - Find types that are repeated among all the FOM documents that will be processed
  //  - Add a different suffix to each of the instances of these types, where the suffix contains the termination of the
  //    file name where the instance was defined (e.g. in the NETN-METOC file the identifier would be METOC)
  //  - The resulting type names have the corresponding suffixes, avoiding type redefinition (e.g.
  //    netn::CapabilitiesSupportedETR and netn::CapabilitiesSupportedMRM)

  const std::string targetQName = fomTypeNameToSenTypeNameQual(doc->doc, name);

  for (const auto& otherDoc: docs_)
  {
    // skip our current doc
    if (otherDoc->name == doc->doc->name)
    {
      continue;
    }

    auto matches = [this, &otherDoc, &targetQName](const auto& otherInteractionNode) -> bool
    {
      const std::string otherQName =
        fomTypeNameToSenTypeNameQual(otherDoc.get(), toUpperCamelCase(otherInteractionNode.child_value("name")));
      return targetQName == otherQName;
    };

    if (std::any_of(
          otherDoc->definedInteractionNodes.begin(), otherDoc->definedInteractionNodes.end(), std::move(matches)))
    {
      name.append(splitString(doc->doc->filePath.stem().string(), "_-").back());
    }
  }

  // new struct
  auto specName = name;
  auto specQualifiedName = fomTypeNameToSenTypeNameQual(doc->doc, specName);
  auto specDescription = formatSemantics(node.node().child_value("semantics"));

  // do the same for fields as we do for args
  std::vector<StructField> fields;
  const auto args = collectArgsFromInteractionNode(node, ignoredParams, doc);
  fields.reserve(args.size());
  for (const auto& arg: args)
  {
    fields.emplace_back(arg.name, arg.description, arg.type);
  }

  StructSpec spec(specName, specQualifiedName, specDescription, fields, std::nullopt);

  // store the type
  return storeType(StructType::make(spec), doc);
}

std::vector<Arg> FomDocumentSet::collectArgsFromInteractionNode(const pugi::xpath_node& interactionNode,
                                                                const std::vector<std::string>& ignoredParams,
                                                                ParsedDoc* doc)
{
  std::vector<Arg> result;

  auto node = interactionNode;
  while (node)
  {
    std::vector<Arg> nodeArgs;
    for (const auto& param: node.node().select_nodes("parameter"))
    {
      std::string paramName = param.node().child_value("name");

      if (std::find(ignoredParams.begin(), ignoredParams.end(), paramName) != ignoredParams.end())
      {
        continue;
      }

      nodeArgs.emplace_back(toLowerCamelCase(paramName),
                            formatSemantics(param.node().child_value("semantics")),
                            getOrCreateTypeFromFomName(param.node().child_value("dataType"), doc).first);
    }
    result = prependArgs(result, nodeArgs);

    // keep going up the hierarchy
    if (!node.parent().empty() && std::string(node.parent().name()) == "interactionClass" &&
        std::string(node.parent().child_value("name")) != "HLAinteractionRoot")
    {
      node = findInteractionNode(computeInteractionPath(node), doc).node;
    }
    else
    {
      return result;
    }
  }

  return result;
}

CallableSpec FomDocumentSet::makeCallableSpec(const pugi::xpath_node& interactionNode,
                                              const std::vector<std::string>& ignoredParams,
                                              ParsedDoc* doc,
                                              CallableEnum callableEnum,
                                              bool packArguments)
{
  auto transportStr = interactionNode.node().child_value("transportation");

  CallableSpec spec {};
  spec.name = toLowerCamelCase(interactionNode.node().child_value("name"));
  spec.description = formatSemantics(interactionNode.node().child_value("semantics"));
  spec.transportMode =
    callableEnum == CallableEnum::method ? getMethodTransportMode(transportStr) : getEventTransportMode(transportStr);

  if (packArguments)
  {
    auto argType = getOrMakeStructFromInteraction(interactionNode, doc, ignoredParams);

    // a single arg
    spec.args.emplace_back("args", "", argType);
  }
  else
  {
    spec.args = collectArgsFromInteractionNode(interactionNode, ignoredParams, doc);
  }

  return spec;
}

EventSpec FomDocumentSet::makeEventSpec(const pugi::xpath_node& interactionNode,
                                        const std::vector<std::string>& ignoredParams,
                                        ParsedDoc* doc,
                                        bool packArguments)
{
  EventSpec spec {};
  spec.callableSpec = makeCallableSpec(interactionNode, ignoredParams, doc, CallableEnum::event, packArguments);
  return spec;
}

MethodSpec FomDocumentSet::makeMethodSpec(const pugi::xpath_node& interactionNode,
                                          const pugi::xpath_node& returnNode,
                                          const std::vector<std::string>& ignoredParams,
                                          ParsedDoc* doc,
                                          bool packArguments,
                                          bool localOnly,
                                          const std::string& qualifiedClassName)
{

  auto specCallableSpec = makeCallableSpec(interactionNode, ignoredParams, doc, CallableEnum::method, packArguments);
  auto specConstness = Constness::nonConstant;
  auto specLocalOnly = localOnly;

  auto annotationItr = settings_.classAnnotations.find(qualifiedClassName);
  auto specDeferred = annotationItr != settings_.classAnnotations.end() &&
                      annotationItr->second.deferredMethods.count(specCallableSpec.name) != 0;

  ConstTypeHandle<> specReturnType = VoidType::get();

  if (returnNode)
  {
    if (!returnNode.node().attribute("deferred").empty())
    {
      const auto deferredStr = std::string(returnNode.node().attribute("deferred").value());
      specDeferred = deferredStr == "true" || deferredStr == "yes";
      std::cerr << "Warning: method " << specCallableSpec.name
                << " is mapped as 'deferred'. This is now deprecated. Please use code generation settings.\n";
    }

    auto interactionAttr = returnNode.node().attribute("hlaInteraction");
    if (interactionAttr)
    {
      auto returnInteractionNode = findInteractionNodeInAllDocs(interactionAttr.value());
      tryToAppendMappingDepToDoc(doc->doc, returnInteractionNode.doc->doc);

      std::vector<std::string> returnIgnoredParams;
      for (const auto& ignoredParamNode: returnNode.node().select_nodes("ignore"))
      {
        returnIgnoredParams.emplace_back(ignoredParamNode.node().attribute("parameter").value());
      }

      auto returnType =
        getOrMakeStructFromInteraction(returnInteractionNode.node, returnInteractionNode.doc, returnIgnoredParams);

      specReturnType = returnType;
    }
    else
    {
      specReturnType = getOrCreateTypeFromFomName(returnNode.node().attribute("dataType").value(), doc).first;
    }
  }
  else
  {
    specReturnType = VoidType::get();
  }

  return {specCallableSpec, specReturnType, specConstness, NonPropertyRelated {}, specDeferred, specLocalOnly};
}

FomDocumentSet::FindInteractionResult FomDocumentSet::findInteractionNode(const std::string& path, const ParsedDoc* doc)
{
  std::string query = "//objectModel/interactions/";
  for (const auto& className: impl::split(path))
  {
    query.append("/interactionClass[name='");
    query.append(className);
    query.append("']");
  }

  // look for the interaction in the current document
  auto node = doc->doc->xmlDoc.select_node(query.c_str());
  if (node && !node.node().child("semantics").empty())  // NOLINT(readability-implicit-bool-conversion)
  {
    return {node, toMutableParsedDoc(doc)};
  }

  const auto& ourPath = doc->doc->filePath;

  // search in the document dependencies
  for (auto& dep: doc->doc->deps)
  {
    if (dep->filePath != ourPath)
    {
      node = dep->xmlDoc.select_node(query.c_str());
      if (node && !node.node().child("semantics").empty())  // NOLINT(readability-implicit-bool-conversion)
      {
        return {node, getParsedDoc(dep)};
      }
    }
  }

  std::string err;
  err.append("could not find interaction '");
  err.append(path);
  err.append("' using query '");
  err.append(query);
  err.append("'");
  throwRuntimeError(err);
}

FomDocumentSet::FindInteractionResult FomDocumentSet::findInteractionNodeInAllDocs(const std::string& path)
{
  std::string query = "//objectModel/interactions/";
  for (const auto& className: impl::split(path))
  {
    query.append("/interactionClass[name='");
    query.append(className);
    query.append("']");
  }

  pugi::xpath_node interactionNode;
  FomDocument* interactionDoc {nullptr};
  // search in all documents
  for (auto& doc: docs_)
  {
    auto node = doc->xmlDoc.select_node(query.c_str());
    if (node && !node.node().child("semantics").empty())  // NOLINT(readability-implicit-bool-conversion)
    {
      if (interactionNode)
      {
        std::string err;
        err.append("multiple '");
        err.append(path);
        err.append("' interactions found");
        throwRuntimeError(err);
      }
      interactionNode = node;
      interactionDoc = doc.get();
    }
  }

  if (!interactionNode)
  {
    std::string err;
    err.append("could not find interaction '");
    err.append(path);
    err.append("' using query '");
    err.append(query);
    err.append("'");
    throwRuntimeError(err);
  }

  return {interactionNode, getParsedDoc(interactionDoc)};
}

}  // namespace sen::lang
