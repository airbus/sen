// === fom_documents.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "fom_documents.h"

// implementation
#include "fom_common.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/scope_guard.h"
#include "sen/core/io/util.h"
#include "sen/core/lang/stl_resolver.h"
#include "sen/core/lang/string_utils.h"
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
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <limits>
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

namespace sen::lang::fom
{

namespace
{

/// The key a declaration is indexed under. The category is part of it so that elements with
/// the same name in different places stay apart.
[[nodiscard]] std::string indexKey(const std::string& category, const std::string& name)
{
  std::string key = category;
  key.append("/");
  key.append(name);
  return key;
}

/// Who asked for an interaction, for a message about one that no module declares or that two do.
[[nodiscard]] std::string mappingAsker(const std::string& className, const std::filesystem::path& mappingPath)
{
  std::string text = "the mapping for class '";
  text.append(className);
  text.append("' in '");
  text.append(mappingPath.string());
  text.append("'");
  return text;
}

/// The data interchange format the reader expects. An older or a newer standard moves elements
/// around, so a file naming a different one would be read into a FOM missing most of what it
/// declares.
constexpr std::string_view difNamespace = "http://standards.ieee.org/IEEE1516-2010";

/// The part of an element name after any namespace prefix. pugixml does not read namespaces, so a
/// prefix stays in the name.
[[nodiscard]] std::string_view localName(std::string_view name)
{
  const auto colon = name.rfind(':');
  return colon == std::string_view::npos ? name : name.substr(colon + 1);
}

/// Refuses a FOM whose data interchange format is not the one the reader expects. Only a root named
/// objectModel is checked, so a mapping file or a schema in the same directory is left alone.
void checkDifVersion(const pugi::xml_node& root, const std::filesystem::path& path)
{
  const std::string_view name = root.name();
  if (localName(name) != "objectModel")
  {
    return;
  }

  const std::string_view declared = root.attribute("xmlns").value();
  if (name == "objectModel" && declared == difNamespace)
  {
    return;
  }

  std::string err;
  err.append("'");
  err.append(path.string());
  err.append("' is not a FOM Sen can read: ");
  if (name != "objectModel")
  {
    err.append("its objectModel carries the namespace prefix '");
    err.append(name.substr(0, name.size() - localName(name).size() - 1));
    err.append("', which the reader does not resolve");
  }
  else if (declared.empty())
  {
    err.append("it declares no xmlns");
  }
  else
  {
    err.append("it declares '");
    err.append(declared);
    err.append("'");
  }
  err.append(". Sen reads the IEEE 1516.2-2010 DIF, '");
  err.append(difNamespace);
  err.append("'");
  throwRuntimeError(err);
}

/// An enumerator's key, from the text of its value, bounded by what its storage can hold. A
/// negative value is taken as its unsigned pattern, because an enumerator key is unsigned here
/// and FOMs do write `Other = -1`.
std::optional<std::uint32_t> enumeratorKey(std::string_view text, const IntegralType& storage)
{
  const bool negative = !text.empty() && text.front() == '-';
  const auto magnitude = parseUnsigned(negative ? text.substr(1) : text);
  if (!magnitude.has_value() || magnitude.value() > std::numeric_limits<std::uint32_t>::max())
  {
    return std::nullopt;
  }

  // Against what the representation can hold, not only 32 bits: an HLAoctet enumeration with
  // 300 would otherwise reach the generated code as an out-of-range enumerator and the
  // compiler would be the one to refuse it.
  const auto bitsAvailable = 8U * static_cast<unsigned>(storage.getByteSize());
  const std::uint64_t limit =
    storage.isSigned() ? std::uint64_t {1} << (bitsAvailable - 1U) : (std::uint64_t {1} << bitsAvailable) - 1U;
  if (magnitude.value() > limit || (!negative && storage.isSigned() && magnitude.value() == limit))
  {
    return std::nullopt;
  }

  const auto bits = static_cast<std::uint32_t>(magnitude.value());
  return negative ? static_cast<std::uint32_t>(-static_cast<std::int64_t>(bits)) : bits;
}

/// The values of every child of a node under one element name, joined. Used to compare the
/// header of two declarations of one type: a record's includes, an enumeration's
/// representation, a variant's discriminant.
std::string joinedChildValues(pugi::xml_node node, const char* element)
{
  std::string result;
  for (const auto& child: node.children(element))
  {
    if (!result.empty())
    {
      result.append(",");
    }
    result.append(child.child_value());
  }
  return result;
}

/// Refuses a contributed declaration whose type header disagrees with the one that owns the
/// type. A contribution normally leaves the header out; carrying a different one is not a
/// merge but a contradiction.
void refuseHeaderClash(const std::string& kind,
                       const std::string& typeName,
                       pugi::xml_node owned,
                       const Declaration& contribution,
                       const char* headerElement)
{
  const auto contributed = joinedChildValues(contribution.node, headerElement);
  if (contributed.empty() || contributed == joinedChildValues(owned, headerElement))
  {
    return;
  }

  std::string err;
  err.append(kind);
  err.append(" '");
  err.append(typeName);
  err.append("' is declared twice with a different ");
  err.append(headerElement);
  err.append(": '");
  err.append(contributed);
  err.append("' in '");
  err.append(contribution.document->path.string());
  err.append("'");
  throwRuntimeError(err);
}

/// Refuses two declarations of one member that do not agree, naming both files. An equivalent
/// repeat is kept once instead, which is what a merge of the two would produce.
[[noreturn]] void refuseMemberClash(const std::string& kind,
                                    const std::string& memberName,
                                    const std::string& typeName,
                                    const Document& owner,
                                    const Document& contributor)
{
  std::string err;
  err.append(kind);
  err.append(" '");
  err.append(memberName);
  err.append("' of '");
  err.append(typeName);
  err.append("' is declared twice and the two do not agree: '");
  err.append(owner.path.string());
  err.append("' and '");
  err.append(contributor.path.string());
  err.append("'");
  throwRuntimeError(err);
}

/// Records that a mapping pulled a document in, so the type set imports it.
void tryToAppendMappingDepToDoc(Document& document, Document* dep)
{
  // A mapping that reaches a type in its own document must not make the document import itself:
  // the generated header would include itself.
  if (dep == &document)
  {
    return;
  }

  auto& imports = document.mappingImports;
  if (std::find(imports.begin(), imports.end(), dep) == imports.end())
  {
    imports.push_back(dep);
  }
}

/// The query that selects the interaction at a path.
std::string interactionQuery(const std::string& path)
{
  std::string query = "//objectModel/interactions/";
  for (const auto& className: impl::split(path))
  {
    query.append("/interactionClass[name='");
    query.append(className);
    query.append("']");
  }
  return query;
}

}  // namespace

std::string FomDocuments::categoryQuery(const Category& category)
{
  std::string query = "//objectModel/dataTypes/";
  query.append(category.container);
  query.append("/");
  query.append(category.element);
  return query;
}

const std::array<FomDocuments::Category, 5>& FomDocuments::dataTypeCategories()
{
  static const std::array<Category, 5> categories {{
    {"simpleDataTypes", "simpleData", &FomDocuments::simpleData},
    {"arrayDataTypes", "arrayData", &FomDocuments::arrayType},
    {"fixedRecordDataTypes", "fixedRecordData", &FomDocuments::recordType},
    {"variantRecordDataTypes", "variantRecordData", &FomDocuments::variantRecord},
    {"enumeratedDataTypes", "enumeratedData", &FomDocuments::enumeration},
  }};
  return categories;
}

FomDocuments::FomDocuments(const std::vector<std::filesystem::path>& paths,
                           const std::vector<std::filesystem::path>& extensions,
                           const std::vector<std::filesystem::path>& mappings,
                           TypeSettings settings)
  : settings_(std::move(settings))
  , representations_({{"HLAinteger16LE", Int16Type::get()},
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
  , units_({{"meter per second squared (m/(s^2))", "m_per_s_sq"},
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
{
  for (const auto& mapping: mappings)
  {
    mappings_.emplace_back();
    mappings_.back().path = mapping;
    auto result = mappings_.back().xml.load_file(mapping.c_str());
    if (!result)
    {
      std::string err;
      err.append(result.description());
      err.append(" (offset ");
      err.append(std::to_string(result.offset));
      err.append(") in '");
      err.append(mapping.string());
      err.append("'");
      throwRuntimeError(err);
    }
  }

  for (const auto& mappingDoc: mappings_)
  {
    // A packed event or method takes one struct of the parameters its ignores leave.
    for (const auto& callable: mappingDoc.xml.select_nodes("/senMapping/class/event | /senMapping/class/method"))
    {
      if (isTrue(callable.node().attribute("pack").value(), "pack") && !callable.node().select_nodes("ignore").empty())
      {
        packedWithIgnores_.emplace_back(callable.node().attribute("hlaInteraction").value());
      }
    }

    // A return is always a struct, so its ignores need no pack attribute to matter.
    for (const auto& returnNode: mappingDoc.xml.select_nodes("/senMapping/class/method/return[@hlaInteraction]"))
    {
      if (!returnNode.node().select_nodes("ignore").empty())
      {
        packedWithIgnores_.emplace_back(returnNode.node().attribute("hlaInteraction").value());
      }
    }
  }

  // The extension paths first, so a directory scan can tell an extension sitting beside the
  // modules from a module.
  for (const auto& path: extensions)
  {
    if (std::filesystem::is_directory(path))
    {
      for (const auto& entry: std::filesystem::directory_iterator {path})
      {
        if (entry.is_regular_file() && entry.path().extension() == ".xml")
        {
          extensionPaths_.push_back(std::filesystem::weakly_canonical(entry.path()));
        }
      }
    }
    else
    {
      extensionPaths_.push_back(std::filesystem::weakly_canonical(path));
    }
  }

  for (const auto& path: paths)
  {
    readDirectory(path);
  }

  for (const auto& path: extensions)
  {
    readExtension(path);
  }

  resolveDependencies();
  indexDeclarations();
  buildRootTypeSet();

  // An extension is never built on its own: what it declares reaches the model through the
  // document that owns the type, when that document is built.
  for (auto& document: documents_)
  {
    if (!document->isExtension)
    {
      ensureBuilt(*document);
    }
  }
}

void FomDocuments::buildRootTypeSet()
{
  {
    // The class every imported HLA class inherits from. It carries the RTI object id so that a
    // bridge to a federation has somewhere to put it; sen never writes to it.
    ClassSpec spec {};
    spec.isInterface = false;
    spec.name = "ObjectRoot";
    spec.qualifiedName = "hla.ObjectRoot";
    spec.description = formatSemantics("HLA Base Class that contains the RTI object ID");

    spec.properties.emplace_back(toSenPropertyName("RtiId"),
                                 formatSemantics("HLA RTI object ID"),
                                 MetaTypeTrait<std::string>::meta(),
                                 PropertyCategory::dynamicRW,
                                 TransportMode::confirmed);

    CallableSpec constructor;
    collectConstructorArgs(constructor.args, spec);
    constructor.name = std::string("constructor") + spec.name;
    constructor.description = "constructor";
    spec.constructor = MethodSpec(constructor, sen::VoidType::get(), Constness::nonConstant);

    rootClass_ = ClassType::make(spec);
  }

  rootSet_ = std::make_unique<TypeSet>();
  rootSet_->fileName = "hla.stl";
  rootSet_->package = {"hla"};
  rootSet_->types.emplace_back(rootClass_.value());

  for (const auto& native: getNativeTypes())
  {
    if (native->isVoidType())
    {
      continue;
    }

    const std::string name = std::string("Maybe") + capitalizeAndRemoveSeparators(std::string(native->getName()));
    rootSet_->types.emplace_back(
      OptionalType::make(OptionalSpec(name, std::string("hla.") + name, "" /* not present in HLA */, native)));
  }
}

//--------------------------------------------------------------------------------------------------------------
// Reading
//--------------------------------------------------------------------------------------------------------------

void FomDocuments::readDirectory(const std::filesystem::path& path)
{
  const auto package = toLower(path.stem().generic_string());

  // By name, not the filesystem's order, so the same directory gives the same model on every
  // machine. What the order decides is where a member contributed by two files lands in the list.
  std::vector<std::filesystem::path> files;
  for (const auto& entry: std::filesystem::directory_iterator {path})
  {
    if (!entry.is_regular_file() || entry.path().extension() != ".xml")
    {
      continue;
    }

    const auto canonical = std::filesystem::weakly_canonical(entry.path());
    if (std::find(extensionPaths_.begin(), extensionPaths_.end(), canonical) != extensionPaths_.end())
    {
      continue;
    }

    files.push_back(entry.path());
  }
  std::sort(files.begin(), files.end());

  for (const auto& file: files)
  {
    std::ignore = readDocument(file, package, false);
  }
}

void FomDocuments::readExtension(const std::filesystem::path& path)
{
  // a file, or a directory of them. By name, because the order decides the order contributed
  // members append in and nothing published depends on it.
  if (std::filesystem::is_directory(path))
  {
    std::vector<std::filesystem::path> files;
    for (const auto& entry: std::filesystem::directory_iterator {path})
    {
      if (entry.is_regular_file() && entry.path().extension() == ".xml")
      {
        files.push_back(entry.path());
      }
    }
    std::sort(files.begin(), files.end());

    for (const auto& file: files)
    {
      std::ignore = readDocument(file, std::string {}, true);
    }
    return;
  }

  std::ignore = readDocument(path, std::string {}, true);
}

Document& FomDocuments::readDocument(const std::filesystem::path& path, const std::string& package, bool isExtension)
{
  auto document = std::make_unique<Document>();

  auto result = document->xml.load_file(path.c_str());
  if (!result)
  {
    std::string err;
    err.append(result.description());
    err.append(" (offset ");
    err.append(std::to_string(result.offset));
    err.append(") in '");
    err.append(path.string());
    err.append("'");
    throwRuntimeError(err);
  }

  checkDifVersion(document->xml.document_element(), path);

  document->path = path;
  document->package = package;
  document->isExtension = isExtension;

  document->objectModel = document->xml.select_node("//objectModel").node();
  if (!document->objectModel && isExtension)
  {
    // An extension was named on the command line, so a file that is not an object model is a
    // mistake worth reporting. A directory of modules may hold other XML, a mapping or a schema,
    // and those are left alone.
    std::string err;
    err.append("'");
    err.append(path.string());
    err.append("' holds no objectModel element, so it cannot add members to a FOM");
    throwRuntimeError(err);
  }
  document->name = document->objectModel.child("modelIdentification").child_value("name");

  for (auto root: document->objectModel.child("objects").children("objectClass"))
  {
    collectClassesNodes(root, document->classNodes);
  }

  for (auto root: document->objectModel.child("interactions").children("interactionClass"))
  {
    collectInteractionNodes(root, document->interactionNodes);
  }

  documents_.push_back(std::move(document));
  return *documents_.back();
}

void FomDocuments::resolveDependencies()
{
  for (auto& document: documents_)
  {
    for (auto reference: document->objectModel.child("modelIdentification").children("reference"))
    {
      const std::string type = reference.child_value("type");
      if (type.find("Dependency") == std::string::npos)
      {
        continue;
      }

      const std::string identification = reference.child_value("identification");
      if (identification == "MIM")
      {
        continue;
      }

      auto* found = findDocument(identification);
      if (found == nullptr)
      {
        std::string err;
        err.append("could not find dependent document '");
        err.append(identification);
        err.append("' for '");
        err.append(document->path.string());
        err.append("'");
        throwRuntimeError(err);
      }

      document->dependencies.push_back(found);
    }
  }
}

Document* FomDocuments::findDocument(const std::string& identification) const
{
  for (const auto& document: documents_)
  {
    // An extension does not join the set a dependency reference resolves against, so it
    // cannot be named as a dependency and cannot be depended upon.
    if (document->isExtension)
    {
      continue;
    }

    if (findStringIgnoreCase(document->name, identification))
    {
      return document.get();
    }

    if (findStringIgnoreCase(document->path.stem().generic_string(), identification))
    {
      return document.get();
    }
  }

  return nullptr;
}

//--------------------------------------------------------------------------------------------------------------
// Indexing
//--------------------------------------------------------------------------------------------------------------

void FomDocuments::indexDeclarations()
{
  for (auto& document: documents_)
  {
    auto dataTypes = document->objectModel.child("dataTypes");

    for (const auto& category: dataTypeCategories())
    {
      for (auto node: dataTypes.child(category.container).children(category.element))
      {
        record(indexKey(category.element, node.child_value("name")), node, *document);
      }
    }

    // A class is identified by its path, not its leaf name, so an extension adding to
    // BaseEntity.PhysicalEntity names that path and nothing else matches it.
    for (auto node: document->classNodes)
    {
      record(indexKey("objectClass", objectClassPath(node)), node, *document);
    }

    for (auto node: document->interactionNodes)
    {
      record(indexKey("interactionClass", interactionClassPath(node)), node, *document);
    }
  }
}

void FomDocuments::record(const std::string& key, pugi::xml_node node, Document& document)
{
  auto& entry = declarations_[key];
  entry.all.push_back(Declaration {node, &document});

  // The complete definition owns the type: the declaration that carries semantics. An extension
  // never owns one. Preferring semantics over read order is what makes the owner, and so the
  // package, the same whichever order the documents arrive in.
  if (document.isExtension)
  {
    return;
  }

  const bool complete = !node.child("semantics").empty();  // NOLINT(readability-implicit-bool-conversion)
  if (entry.owner == nullptr || (complete && !entry.ownerIsComplete))
  {
    entry.owner = &document;
    entry.ownerIsComplete = complete;
  }
}

std::optional<Declaration> FomDocuments::ownedElsewhere(const std::string& key, const Document& document) const
{
  const auto itr = declarations_.find(key);
  if (itr == declarations_.end() || itr->second.owner == nullptr || itr->second.owner == &document ||
      itr->second.owner->package != document.package)
  {
    return std::nullopt;
  }

  for (const auto& declaration: itr->second.all)
  {
    if (declaration.document == itr->second.owner)
    {
      return declaration;
    }
  }
  return std::nullopt;
}

std::vector<Declaration> FomDocuments::contributions(const std::string& key,
                                                     pugi::xml_node own,
                                                     const Document& document) const
{
  const auto itr = declarations_.find(key);
  if (itr == declarations_.end())
  {
    return {};
  }

  // An extension always contributes. A module contributes to a module of its own package, which
  // is what two files of one FOM directory declaring one type have always done. Across packages a
  // module does not: apps/cli_gen/test/test18 keeps two same-named interactions in two modules
  // apart on purpose, and test2, test5, test6 and test8 refuse a cross-package type reference
  // outright rather than resolving it. Merging across packages is a change to those decisions.
  std::vector<Declaration> result;
  for (const auto& declaration: itr->second.all)
  {
    if (declaration.node == own)
    {
      continue;
    }

    if (declaration.document->isExtension || declaration.document->package == document.package)
    {
      result.push_back(declaration);
    }
  }
  return result;
}

std::optional<Declaration> FomDocuments::introducedType(const std::string& category, const std::string& fomName) const
{
  const auto itr = declarations_.find(indexKey(category, fomName));
  if (itr == declarations_.end() || itr->second.owner != nullptr || itr->second.all.empty())
  {
    return std::nullopt;
  }

  return itr->second.all.front();
}

//--------------------------------------------------------------------------------------------------------------
// Building
//--------------------------------------------------------------------------------------------------------------

void FomDocuments::ensureBuilt(Document& document)
{
  if (document.built)
  {
    return;
  }
  document.built = true;

  for (auto* dependency: document.dependencies)
  {
    ensureBuilt(*dependency);
  }

  populateTypes(document);
}

void FomDocuments::populateTypes(Document& document)
{
  for (const auto& node: document.xml.select_nodes("//objectModel/dataTypes/basicDataRepresentations/basicData"))
  {
    basicDataRepresentation(node.node(), document);
  }

  for (const auto& category: dataTypeCategories())
  {
    for (const auto& node: document.xml.select_nodes(categoryQuery(category).c_str()))
    {
      // As for a class: two documents of one package each building this type would emit the
      // same names twice in one namespace.
      if (ownedElsewhere(indexKey(category.element, node.node().child_value("name")), document))
      {
        continue;
      }

      std::ignore = processTypeSource(document,
                                      node,
                                      [this, &document, &category](const pugi::xpath_node& declaring)
                                      { return (this->*category.build)(declaring.node(), document); });
    }
  }

  for (auto node: document.classNodes)
  {
    // Two documents of one package declaring the same class would each build it, and the
    // generated file of each would define the same names in the same namespace.
    if (ownedElsewhere(indexKey("objectClass", objectClassPath(node)), document).has_value())
    {
      continue;
    }

    std::ignore = classNode(node, document);
  }

  for (auto node: document.interactionNodes)
  {
    std::ignore = structFromInteraction(node, document, {});
  }
}

template <typename F>
ConstTypeHandle<> FomDocuments::processTypeSource(Document& document, const pugi::xpath_node& node, F func)
{
  const std::string typeName = node.node().child_value("name");

  // only store the type if not done so before
  auto itr = std::find_if(
    document.storage.begin(), document.storage.end(), [&](auto& elem) { return elem->getName() == typeName; });
  if (itr == document.storage.end())
  {
    return storeType(func(node), document);
  }
  return *itr;
}

template <typename F>
void FomDocuments::processTypeSource(Document& document, const pugi::xpath_node_set& nodes, F func)
{
  for (const auto& elem: nodes)
  {
    std::ignore = processTypeSource(document, elem, func);
  }
}

template <typename F>
std::optional<ConstTypeHandle<>> FomDocuments::processTypeQuery(Document& document,
                                                                const std::string& query,
                                                                const std::string& queryPostFix,
                                                                F func)
{
  std::string finalQuery = query;
  finalQuery.append(queryPostFix);

  auto nodes = document.xml.select_nodes(finalQuery.c_str());
  if (!nodes.empty())
  {
    return processTypeSource(document, nodes.first(), func);
  }

  return std::nullopt;
}

std::optional<std::pair<ConstTypeHandle<>, Document*>> FomDocuments::searchBySenName(
  const std::string& qualifiedName) const
{
  for (const auto& document: documents_)
  {
    if (auto type = document->registry.get(qualifiedName))
    {
      return {std::make_pair(type.value(), document.get())};
    }
  }

  for (const auto& native: getNativeTypes())
  {
    if (native->getName() == qualifiedName)
    {
      return {std::make_pair(native, nullptr)};
    }
  }

  for (const auto& type: rootSet_->types)
  {
    if (type->getQualifiedName() == qualifiedName)
    {
      return {std::make_pair(type, nullptr)};
    }
  }

  return std::nullopt;
}

std::optional<std::pair<ConstTypeHandle<>, Document*>> FomDocuments::searchTypeHere(const std::string& fomName,
                                                                                    Document& document)
{
  // look for the type in the document
  for (auto type: document.storage)
  {
    if (type->getName() == fomName)
    {
      return {std::make_pair(std::move(type), &document)};
    }
  }

  // look for the type in the document dependencies
  for (auto* dependency: document.dependencies)
  {
    ensureBuilt(*dependency);
    if (auto lookup = searchTypeHere(fomName, *dependency))
    {
      return lookup;
    }
  }

  return std::nullopt;
}

std::optional<std::pair<ConstTypeHandle<>, Document*>> FomDocuments::searchType(const std::string& fomName,
                                                                                Document& document)
{
  if (auto lookup = searchTypeHere(fomName, document))
  {
    return lookup;
  }

  return searchBySenName(senName(fomName));
}

std::pair<ConstTypeHandle<>, Document*> FomDocuments::getOrCreateType(const std::string& fomName, Document& document)
{
  // search for already parsed types
  if (auto lookup = searchType(fomName, document))
  {
    return std::move(lookup).value();
  }

  // Guards a type that names itself, directly or through a chain.
  const auto beingResolved = std::make_pair(fomName, static_cast<const Document*>(&document));
  if (std::find(resolving_.begin(), resolving_.end(), beingResolved) != resolving_.end())
  {
    std::string err;
    err.append("circular reference while resolving type '");
    err.append(fomName);
    err.append("' in '");
    err.append(document.path.string());
    err.append("'");
    throwRuntimeError(err);
  }

  resolving_.push_back(beingResolved);
  const auto resolved = makeScopeGuard([this]() { resolving_.pop_back(); });

  // not parsed yet, has to be somewhere in this document
  const auto condition = std::string("[name = '") + fomName + "']";
  for (const auto& category: dataTypeCategories())
  {
    auto build = [this, &document, &category](const pugi::xpath_node& node)
    { return (this->*category.build)(node.node(), document); };

    if (auto maybeType = processTypeQuery(document, categoryQuery(category), condition, build))
    {
      return {std::move(maybeType).value(), &document};
    }
  }

  if (auto type = document.registry.get(senQualifiedName(document, fomName)))
  {
    return {std::move(type).value(), &document};
  }

  // classes in this document
  for (auto node: document.classNodes)
  {
    if (std::string(node.child_value("name")) == fomName)
    {
      // A nested child reaches its parent through here, so without this the document whose
      // class loop was skipped builds the class anyway.
      if (const auto owned = ownedElsewhere(indexKey("objectClass", objectClassPath(node)), document))
      {
        return {storeType(classNode(owned->node, *owned->document), *owned->document), owned->document};
      }

      return {storeType(classNode(node, document), document), &document};
    }
  }

  // A type no complete definition declares came from an extension. It joins this document's set
  // and takes its package, because a set of its own would import and be imported by it.
  for (const auto& category: dataTypeCategories())
  {
    if (const auto introduced = introducedType(category.element, fomName))
    {
      auto type = (this->*category.build)(introduced->node, document);
      return {storeType(std::move(type), document), &document};
    }
  }

  // Only the last dependency failure is an error, since an earlier one may be resolved by a
  // later. The reasons are reported together: the one that matters is usually a nested name.
  std::vector<std::string> reasons;
  for (auto* dependency: document.dependencies)
  {
    ensureBuilt(*dependency);
    if (dependency != &document)
    {
      try
      {
        return getOrCreateType(fomName, *dependency);
      }
      catch (const std::exception& failure)
      {
        // The inner message already names the file it was looking in, so this only indents it.
        std::string reason;
        reason.append("\n  ");
        reason.append(failure.what());
        reasons.push_back(std::move(reason));
      }
    }
  }

  std::string err;
  err.append("could not find type '");
  err.append(fomName);
  err.append("', referenced from '");
  err.append(document.path.string());
  err.append("'");
  for (const auto& reason: reasons)
  {
    err.append(reason);
  }
  throwRuntimeError(err);
}

//--------------------------------------------------------------------------------------------------------------
// Type sets
//--------------------------------------------------------------------------------------------------------------

std::vector<DocumentTypeSet> FomDocuments::takeTypeSets()
{
  std::vector<DocumentTypeSet> result;
  result.reserve(documents_.size());

  std::map<const Document*, TypeSet*> setPerDocument;

  for (auto& document: documents_)
  {
    // An extension owns no type, so it has no type set and generates no file: what it
    // contributed is in the set of the document that owns the type. Nor does a file that
    // declares no object model, which would otherwise generate an empty header and source.
    if (document->isExtension || !document->objectModel)
    {
      continue;
    }

    auto typeSet = std::make_unique<TypeSet>();
    typeSet->fileName = toLower(document->path.filename().generic_string());
    typeSet->parentDirectory = toLower(document->path.parent_path().filename().generic_string());
    typeSet->package.push_back(document->package);

    typeSet->types.reserve(document->storage.size());

    for (auto& type: document->registry.getAllInOrder())
    {
      auto isSame = [&](const auto& other)
      {
        auto custom = other->asCustomType();
        return custom != nullptr && custom->getQualifiedName() == type->getQualifiedName();
      };

      // add the type only if present in the current type set
      if (std::find_if(document->storage.begin(), document->storage.end(), isSame) != document->storage.end())
      {
        auto found = document->registry.get(std::string(type->getQualifiedName()));
        SEN_ASSERT(found.has_value());
        typeSet->types.emplace_back(dynamicTypeHandleCast<const CustomType>(std::move(found).value()).value());
      }
    }

    setPerDocument.insert({document.get(), typeSet.get()});
    result.push_back(DocumentTypeSet {document->path, std::move(typeSet)});
  }

  // the imports, now that every document has a set to point at
  for (const auto& document: documents_)
  {
    if (document->isExtension || !document->objectModel)
    {
      continue;
    }

    auto* typeSet = setPerDocument.at(document.get());

    for (auto* dependency: document->dependencies)
    {
      typeSet->importedSets.push_back(setPerDocument.at(dependency));
    }

    for (auto* extra: document->mappingImports)
    {
      if (std::find(document->dependencies.begin(), document->dependencies.end(), extra) ==
          document->dependencies.end())
      {
        typeSet->importedSets.push_back(setPerDocument.at(extra));
      }
    }

    typeSet->importedSets.push_back(rootSet_.get());
  }

  return result;
}

const TypeSet& FomDocuments::rootTypeSet() const& noexcept { return *rootSet_; }

std::unique_ptr<TypeSet> FomDocuments::takeRootTypeSet() && { return std::move(rootSet_); }

ConstTypeHandle<> FomDocuments::storeType(ConstTypeHandle<> type, Document& document)
{
  // first check if the type is already defined
  if (auto custom = type->asCustomType())
  {
    if (auto found = document.registry.get(std::string(custom->getQualifiedName())))
    {
      return std::move(found).value();
    }
  }

  document.storage.push_back(type);
  auto typePtr = document.storage.back();
  document.registry.add(typePtr);

  return typePtr;
}

ConstTypeHandle<> FomDocuments::simpleData(pugi::xml_node node, Document& document)
{
  std::string name = node.child_value("name");
  std::string rep = node.child_value("representation");
  std::string unit = node.child_value("units");

  if (!unit.empty() && unit != "NA")
  {
    auto specElementType = dynamicTypeHandleCast<const NumericType>(representationToType(rep));

    if (!specElementType)
    {
      std::string err;
      err.append(" representation '");
      err.append(rep);
      err.append("' for quantity '");
      err.append(name);
      err.append("' is not numeric, in '");
      err.append(document.path.string());
      err.append("'");
      throwRuntimeError(err);
    }

    auto specName = senName(name);
    auto specQualifiedName = senQualifiedName(document, specName);
    auto specDescription = formatSemantics(node.child_value("semantics"));

    // compute the unit
    std::string senUnitAbbreviation;
    {
      auto itr = units_.find(unit);
      if (itr != units_.end())
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
        err.append("no sen unit with abbreviation '");
        err.append(senUnitAbbreviation);
        err.append("', which the unit map in fom_documents.cpp gives for '");
        err.append(unit);
        err.append("' as declared by '");
        err.append(name);
        err.append("' in '");
        err.append(document.path.string());
        err.append("'");

        throw std::logic_error(err);
      }

      auto specUnit = unitLookup;

      return QuantityType::make(
        QuantitySpec(specName, specQualifiedName, specDescription, std::move(specElementType).value(), specUnit));
    }
  }

  {
    auto specName = senName(name);
    return AliasType::make(AliasSpec(specName,
                                     senQualifiedName(document, specName),
                                     formatSemantics(node.child_value("semantics")),
                                     representationToType(rep)));
  }
}

ConstTypeHandle<> FomDocuments::arrayType(pugi::xml_node node, Document& document)
{
  std::string name = node.child_value("name");
  std::string elementType = node.child_value("dataType");
  std::string cardinalityStr = node.child_value("cardinality");

  if ((elementType == "HLAASCIIchar" || elementType == "HLAunicodeChar") && cardinalityStr == "Dynamic")
  {
    return StringType::get();
  }

  auto specElementType = getOrCreateType(elementType, document).first;
  auto specName = senName(name);
  auto specQualifiedName = senQualifiedName(document, specName);
  auto specDescription = formatSemantics(node.child_value("semantics"));
  // A cardinality is a count or a range; above the cap the sequence is unbounded. Both forms
  // take the cap, so a count of a million and a range ending at a million agree.
  std::optional<size_t> specMaxSize;
  std::optional<std::uint64_t> cardinality;
  if (isNumber(cardinalityStr))
  {
    cardinality = parseUnsigned(cardinalityStr);
  }
  else if (!cardinalityStr.empty())
  {
    auto const regex = std::regex(R"(\[(0|[1-9][0-9]*)\.+(0|[1-9][0-9]*)\])");
    auto matchResults = std::smatch {};
    std::regex_match(cardinalityStr, matchResults, regex);

    if (matchResults.size() == 3U)
    {
      cardinality = parseUnsigned(matchResults[2].str());
    }
  }

  if (cardinality.has_value() && cardinality.value() < maxStaticVectorSize)
  {
    specMaxSize = static_cast<size_t>(cardinality.value());
  }

  return SequenceType::make(SequenceSpec(specName, specQualifiedName, specDescription, specElementType, specMaxSize));
}

ConstTypeHandle<> FomDocuments::recordType(pugi::xml_node node, Document& document)
{
  std::string name = node.child_value("name");

  auto specName = senName(name);
  auto specQualifiedName = senQualifiedName(document, specName);
  auto specDescription = formatSemantics(node.child_value("semantics"));

  // <include> brings in the fields of another record. A sen struct expresses that as its parent,
  // and takes exactly one, so a record including more than one cannot be represented.
  MaybeConstTypeHandle<StructType> parent;
  for (const auto& includeNode: node.children("include"))
  {
    const std::string includedName = includeNode.child_value();

    if (parent)
    {
      std::string err;
      err.append("record '");
      err.append(name);
      err.append("' in '");
      err.append(document.path.string());
      err.append("' includes more than one record, which sen cannot represent: a struct has a single parent");
      throwRuntimeError(err);
    }

    parent = dynamicTypeHandleCast<const StructType>(getOrCreateType(includedName, document).first);
    if (!parent)
    {
      std::string err;
      err.append("record '");
      err.append(name);
      err.append("' in '");
      err.append(document.path.string());
      err.append("' includes '");
      err.append(includedName);
      err.append("', which is not a record");
      throwRuntimeError(err);
    }
  }

  auto readField = [this, &document](pugi::xml_node fieldNode)
  {
    return StructField(toLowerCamelCase(fieldNode.child_value("name")),
                       formatSemantics(fieldNode.child_value("semantics")),
                       getOrCreateType(fieldNode.child_value("dataType"), document).first);
  };

  std::vector<StructField> fields;
  for (const auto& fieldNode: node.children("field"))
  {
    fields.push_back(readField(fieldNode));
  }

  // Fields contributed by another declaration of this record, appended after its own, and where
  // each came from so a refusal names the two files that disagree.
  std::map<std::string, const Document*> fieldFrom;
  for (const auto& field: fields)
  {
    fieldFrom.emplace(field.name, &document);
  }

  for (const auto& contribution: contributions(indexKey("fixedRecordData", name), node, document))
  {
    refuseHeaderClash("record", name, node, contribution, "include");

    for (const auto& fieldNode: contribution.node.children("field"))
    {
      auto field = readField(fieldNode);

      auto itr =
        std::find_if(fields.begin(), fields.end(), [&field](const auto& elem) { return elem.name == field.name; });
      if (itr == fields.end())
      {
        fieldFrom.emplace(field.name, contribution.document);
        fields.push_back(std::move(field));
      }
      else if (!(*itr == field))
      {
        refuseMemberClash("field", field.name, name, *fieldFrom.at(field.name), *contribution.document);
      }
    }
  }

  StructSpec spec(specName, specQualifiedName, specDescription, fields, parent);

  return StructType::make(spec);
}

ConstTypeHandle<> FomDocuments::variantRecord(pugi::xml_node node, Document& document)
{
  std::string name = node.child_value("name");

  VariantSpec spec {};
  spec.name = senName(name);
  spec.qualifiedName = senQualifiedName(document, spec.name);
  spec.description = formatSemantics(node.child_value("semantics"));

  // Its own alternatives, then those contributed by another declaration of this variant. The
  // key counts on across the two, so contributed alternatives take the keys after the last.
  std::vector<pugi::xml_node> alternativeNodes;
  for (const auto& alternativeNode: node.children("alternative"))
  {
    alternativeNodes.push_back(alternativeNode);
  }
  for (const auto& contribution: contributions(indexKey("variantRecordData", name), node, document))
  {
    refuseHeaderClash("variant record", name, node, contribution, "discriminant");

    for (const auto& alternativeNode: contribution.node.children("alternative"))
    {
      const std::string alternativeName = alternativeNode.child_value("name");

      // Against every alternative so far, not only the ones this variant declared itself, so
      // that two contributions naming one alternative are caught too. An alternative is the
      // same one when its type and its discriminant agree, and is then kept once.
      const auto same = std::find_if(alternativeNodes.begin(),
                                     alternativeNodes.end(),
                                     [&alternativeName](auto existing)
                                     { return std::string(existing.child_value("name")) == alternativeName; });
      if (same != alternativeNodes.end())
      {
        const bool agrees = std::string(same->child_value("dataType")) == alternativeNode.child_value("dataType") &&
                            std::string(same->child_value("enumerator")) == alternativeNode.child_value("enumerator");
        if (!agrees)
        {
          refuseMemberClash("alternative", alternativeName, name, document, *contribution.document);
        }
        continue;
      }

      alternativeNodes.push_back(alternativeNode);
    }
  }

  uint32_t key = 0U;
  for (const auto& alternativeNode: alternativeNodes)
  {
    auto dataType = alternativeNode.child_value("dataType");

    auto fieldType = getOrCreateType(dataType, document).first;
    if (fieldType->isVoidType())
    {
      std::string err;
      err.append("alternative '");
      err.append(alternativeNode.child_value("name"));
      err.append("' of variant '");
      err.append(name);
      err.append("' in '");
      err.append(document.path.string());
      err.append("' has a void type");
      throwRuntimeError(err);
    }

    VariantField field(key++, formatSemantics(alternativeNode.child_value("semantics")), fieldType);
    spec.fields.push_back(std::move(field));
  }

  return VariantType::make(spec);
}

ConstTypeHandle<> FomDocuments::enumeration(pugi::xml_node node, Document& document)
{
  std::string name = node.child_value("name");
  std::string storageRep = node.child_value("representation");

  if (name == "RPRboolean")  // RPRboolean is a dumb enum
  {
    return BoolType::get();
  }

  auto integralType = representationToType(storageRep);
  if (!integralType->isIntegralType())
  {
    std::string err;
    err.append("storage type '");
    err.append(storageRep);
    err.append("' for enumeration '");
    err.append(name);
    err.append("' in '");
    err.append(document.path.string());
    err.append("' is not an integral type");
    throwRuntimeError(err);
  }
  auto specName = senName(name);
  auto specQualifiedName = senQualifiedName(document, specName);
  auto specDescription = formatSemantics(node.child_value("semantics"));
  auto specStorageType = dynamicTypeHandleCast<const IntegralType>(integralType);

  std::vector<Enumerator> enums;
  std::map<std::string, uint32_t> collectedSenEnumerators;

  // Its own enumerators, then those contributed by another declaration of this enumeration.
  std::vector<pugi::xml_node> enumeratorNodes;
  for (const auto& enumeratorNode: node.children("enumerator"))
  {
    enumeratorNodes.push_back(enumeratorNode);
  }
  std::vector<const Document*> enumeratorFrom(enumeratorNodes.size(), &document);

  for (const auto& contribution: contributions(indexKey("enumeratedData", name), node, document))
  {
    refuseHeaderClash("enumeration", name, node, contribution, "representation");

    for (const auto& enumeratorNode: contribution.node.children("enumerator"))
    {
      enumeratorNodes.push_back(enumeratorNode);
      enumeratorFrom.push_back(contribution.document);
    }
  }

  for (std::size_t index = 0; index < enumeratorNodes.size(); ++index)
  {
    const auto enumeratorNode = enumeratorNodes[index];
    const std::string senEnumeratorName = toLowerCamelCase(enumeratorNode.child_value("name"));

    Enumerator enumerator {};
    const std::string valueStr = enumeratorNode.child_value("value");
    const auto value = enumeratorKey(valueStr, *specStorageType.value());
    if (!value.has_value())
    {
      std::string err;
      err.append("enumerator '");
      err.append(enumeratorNode.child_value("name"));
      err.append("' of '");
      err.append(name);
      err.append("' has value '");
      err.append(valueStr);
      err.append("', which is not a whole number its representation '");
      err.append(storageRep);
      err.append("' can hold, in '");
      err.append(document.path.string());
      err.append("'");
      throwRuntimeError(err);
    }
    enumerator.key = value.value();

    // A contributed enumerator that repeats a name already there is the same enumerator and
    // must agree with it. This is checked before the counter below, which would otherwise
    // rename it and let a disagreeing value in as a second enumerator.
    if (enumeratorFrom[index] != &document)
    {
      auto itr = std::find_if(
        enums.begin(), enums.end(), [&senEnumeratorName](const auto& elem) { return elem.name == senEnumeratorName; });
      if (itr != enums.end())
      {
        if (itr->key != enumerator.key)
        {
          refuseMemberClash("enumerator", senEnumeratorName, name, document, *enumeratorFrom[index]);
        }
        continue;
      }
    }

    // The name, with a counter on the very rare repeats within one declaration.
    const auto count = collectedSenEnumerators[senEnumeratorName];
    enumerator.name = count == 0 ? senEnumeratorName : senEnumeratorName + std::to_string(count);

    enums.push_back(std::move(enumerator));
    collectedSenEnumerators[senEnumeratorName]++;
  }

  EnumSpec spec(specName, specQualifiedName, specDescription, std::move(enums), std::move(specStorageType).value());

  return EnumType::make(spec);
}

void FomDocuments::basicDataRepresentation(pugi::xml_node node, const Document& document)
{
  std::string name = node.child_value("name");
  std::string encoding = node.child_value("encoding");

  if (startsWith(encoding, "8-bit unsigned integer"))
  {
    representations_.insert({name, UInt8Type::get()});
  }
  else if (startsWith(encoding, "16-bit unsigned integer"))
  {
    representations_.insert({name, UInt16Type::get()});
  }
  else if (startsWith(encoding, "32-bit unsigned integer"))
  {
    representations_.insert({name, UInt32Type::get()});
  }
  else if (startsWith(encoding, "64-bit unsigned integer"))
  {
    representations_.insert({name, UInt64Type::get()});
  }
  else if (startsWith(encoding, "16-bit signed integer"))
  {
    representations_.insert({name, Int16Type::get()});
  }
  else if (startsWith(encoding, "32-bit signed integer"))
  {
    representations_.insert({name, Int32Type::get()});
  }
  else if (startsWith(encoding, "64-bit signed integer"))
  {
    representations_.insert({name, Int64Type::get()});
  }
  else
  {
    std::string err;
    err.append("unknown encoding '");
    err.append(encoding);
    err.append("' for basic data representation '");
    err.append(name);
    err.append("' in '");
    err.append(document.path.string());
    err.append("'");
    throwRuntimeError(err);
  }
}

PropertySpec FomDocuments::propertyFromAttribute(pugi::xml_node attrNode,
                                                 Document& document,
                                                 const ClassAnnotations* annotations)
{
  auto propertySpecName = toSenPropertyName(attrNode.child_value("name"));

  std::string semanticsString = attrNode.child_value("semantics");

  auto propertySpecType = [&]()
  {
    if (startsWith(semanticsString, "Optional.") || startsWith(semanticsString, "Optional (") ||
        startsWith(semanticsString, "Optional:") || startsWith(semanticsString, "Optional,"))
    {
      return optionalPropertyType(attrNode.child_value("dataType"), document);
    }

    return getOrCreateType(attrNode.child_value("dataType"), document).first;
  }();

  auto propertySpecDescription = formatSemantics(semanticsString);
  auto propertySpecCategory = PropertyCategory::dynamicRO;
  const std::string transportation = attrNode.child_value("transportation");
  const auto maybeTransportMode = getTransportMode(transportation);
  if (!maybeTransportMode.has_value())
  {
    std::string err;
    err.append("unknown transportation '");
    err.append(transportation);
    err.append("' on attribute '");
    err.append(attrNode.child_value("name"));
    err.append("' in '");
    err.append(document.path.string());
    err.append("'");
    throwRuntimeError(err);
  }
  auto propertySpecTransportMode = maybeTransportMode.value();

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
        err.append("' on attribute '");
        err.append(attrNode.child_value("name"));
        err.append("' in '");
        err.append(document.path.string());
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
  bool propertySpecCheckedSet = annotations != nullptr && annotations->checkedProperties.count(propertySpecName) != 0;

  return PropertySpec {std::move(propertySpecName),
                       std::move(propertySpecDescription),
                       std::move(propertySpecType),
                       propertySpecCategory,
                       propertySpecTransportMode,
                       propertySpecCheckedSet};
}

/// Applies the mappings to a class: a property made writable, an interaction turned into an
/// event or a method. The contributed members are already in propertySpecs, so a mapping can
/// name one.
void FomDocuments::applyMappings(ClassSpec& classSpec,
                                 std::vector<PropertySpec>& propertySpecs,
                                 const MaybeConstTypeHandle<>& parentType,
                                 Document& document)
{
  if (!mappings_.empty())
  {
    std::string query = "/senMapping/class[@name=\"";
    query.append(
      computeClassPath(parentType.value()->asClassType(), classSpec.name, rootClass_.value()->asClassType()));
    query.append("\"]");

    for (auto& mappingDoc: mappings_)
    {
      auto classMappings = mappingDoc.xml.select_nodes(query.c_str());
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
            if (isTrue(propertyMapping.node().attribute("writable").value(), "writable"))
            {
              itr->category = PropertyCategory::dynamicRW;
            }

            // check if the property is defined as  in the mappings
            if (!propertyMapping.node().attribute("checked").empty())
            {
              std::cerr << "Warning: property " << itr->name
                        << " is mapped as 'checked'. This is now deprecated. Please use code generation settings.\n";

              itr->checkedSet = isTrue(propertyMapping.node().attribute("checked").value(), "checked");
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

          auto interaction = findInteractionAnywhere(eventMapping.node().attribute("hlaInteraction").value(),
                                                     mappingAsker(classSpec.name, mappingDoc.path));
          tryToAppendMappingDepToDoc(document, interaction.document);

          const auto packed = isTrue(eventMapping.node().attribute("pack").value(), "pack");

          classSpec.events.push_back(makeEventSpec(interaction.node, ignoredAttributes, *interaction.document, packed));
        }

        // inspect methods
        for (const auto& methodMapping: mapping.node().select_nodes("method"))
        {
          auto interaction = findInteractionAnywhere(methodMapping.node().attribute("hlaInteraction").value(),
                                                     mappingAsker(classSpec.name, mappingDoc.path));
          tryToAppendMappingDepToDoc(document, interaction.document);

          std::vector<std::string> ignoredAttributes;
          for (const auto& ignoreNode: methodMapping.node().select_nodes("ignore"))
          {
            ignoredAttributes.emplace_back(ignoreNode.node().attribute("parameter").value());
          }

          auto returnNode = methodMapping.node().select_node("return");
          const auto packed = isTrue(methodMapping.node().attribute("pack").value(), "pack");
          const auto local = isTrue(methodMapping.node().attribute("local").value(), "local");

          classSpec.methods.push_back(makeMethodSpec(interaction.node,
                                                     returnNode.node(),
                                                     ignoredAttributes,
                                                     *interaction.document,
                                                     packed,
                                                     local,
                                                     classSpec.qualifiedName));
        }
      }
    }
  }
}

ConstTypeHandle<> FomDocuments::classNode(pugi::xml_node node, Document& document)
{
  const std::string fomClassName = node.child_value("name");

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
      parentType = getOrCreateType(parentFomClassName, document).first;
    }
  }

  ClassSpec classSpec {};
  classSpec.isInterface = false;
  classSpec.name = senName(fomClassName);
  classSpec.qualifiedName = senQualifiedName(document, classSpec.name);
  classSpec.description = formatSemantics(node.child_value("semantics"));
  classSpec.parents.emplace_back(dynamicTypeHandleCast<const ClassType>(parentType.value()).value());

  const auto annotationItr = settings_.classAnnotations.find(classSpec.qualifiedName);
  const ClassAnnotations* annotations =
    annotationItr != settings_.classAnnotations.end() ? &annotationItr->second : nullptr;

  std::vector<PropertySpec> propertySpecs;
  const auto xmlAttributes = node.children("attribute");
  propertySpecs.reserve(std::distance(xmlAttributes.begin(), xmlAttributes.end()));

  for (const auto& attrNode: xmlAttributes)
  {
    propertySpecs.push_back(propertyFromAttribute(attrNode, document, annotations));
  }

  // Properties contributed by another declaration of this class, appended in the order the
  // documents were read. A name already there must agree; a repeat that agrees is kept once.
  // Where each member came from, so a refusal names the two files that disagree rather than
  // whichever document is being built.
  std::map<std::string, const Document*> propertyFrom;
  for (const auto& property: propertySpecs)
  {
    propertyFrom.emplace(property.name, &document);
  }

  for (const auto& contribution: contributions(indexKey("objectClass", objectClassPath(node)), node, document))
  {
    // An extension declares no types of its own, so its attribute names one this document has. A
    // module of the same package declares its own, so its attribute is resolved there.
    auto& typeSource = contribution.document->isExtension ? document : *contribution.document;

    for (const auto& attrNode: contribution.node.children("attribute"))
    {
      auto property = propertyFromAttribute(attrNode, typeSource, annotations);

      auto itr = std::find_if(propertySpecs.begin(),
                              propertySpecs.end(),
                              [&property](const auto& elem) { return elem.name == property.name; });
      if (itr == propertySpecs.end())
      {
        propertyFrom.emplace(property.name, contribution.document);
        propertySpecs.push_back(std::move(property));
      }
      else if (!(*itr == property))
      {
        refuseMemberClash(
          "property", property.name, fomClassName, *propertyFrom.at(property.name), *contribution.document);
      }
    }
  }

  applyMappings(classSpec, propertySpecs, parentType, document);

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

  return storeType(ClassType::make(classSpec), document);
}

ConstTypeHandle<> FomDocuments::optionalPropertyType(const std::string& fomElementTypeName, Document& document)
{
  auto senTypeNameUnequal = std::string("Maybe") + capitalizeAndRemoveSeparators(senName(fomElementTypeName));

  if (auto typeLookup = searchType(senTypeNameUnequal, document))
  {
    return typeLookup.value().first;
  }

  auto [elementType, elementDoc] = getOrCreateType(fomElementTypeName, document);

  if (elementDoc)
  {
    auto* aliasType = elementType->asAliasType();
    bool isAliasOfNative = elementType->isAliasType() && !aliasType->getAliasedType()->isCustomType();

    if (!elementType->isCustomType() || isAliasOfNative)
    {
      auto elementTypeToSearch = isAliasOfNative ? aliasType->getAliasedType() : elementType;

      for (const auto& [name, representedType]: representations_)
      {
        if (representedType->getName() == elementTypeToSearch->getName())
        {
          elementType = representedType;
          break;
        }
      }

      senTypeNameUnequal = std::string("Maybe") + capitalizeAndRemoveSeparators(std::string(elementType->getName()));
      for (const auto& rootType: rootSet_->types)
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
      OptionalSpec spec(senTypeNameUnequal, withPackage(*elementDoc, senTypeNameUnequal), "", elementType);

      return storeType(OptionalType::make(spec), *elementDoc);
    }

    throw std::runtime_error("could not find container for native optional type");
  }

  // No document owns the element type, so it is native or a root set type. Every native has its
  // optional in the root set, which the search above returns, so this is not reached.
  std::string err;
  err.append("internal error: no FOM file defines the element type of optional '");
  err.append(senTypeNameUnequal);
  err.append("'");
  throwRuntimeError(err);
}

Arg FomDocuments::argFromParameter(pugi::xml_node paramNode, Document& document)
{
  const std::string semanticsString = paramNode.child_value("semantics");

  auto argType = [&]()
  {
    if (startsWith(semanticsString, "Optional.") || startsWith(semanticsString, "Optional (") ||
        startsWith(semanticsString, "Optional:") || startsWith(semanticsString, "Optional,"))
    {
      return optionalPropertyType(paramNode.child_value("dataType"), document);
    }

    return getOrCreateType(paramNode.child_value("dataType"), document).first;
  }();

  return {toLowerCamelCase(paramNode.child_value("name")), formatSemantics(semanticsString), argType};
}

std::vector<Arg> FomDocuments::collectArgs(pugi::xml_node interactionNode,
                                           const std::vector<std::string>& ignoredParams,
                                           Document& document)
{
  std::vector<Arg> result;

  auto ignored = [&ignoredParams](const std::string& paramName)
  { return std::find(ignoredParams.begin(), ignoredParams.end(), paramName) != ignoredParams.end(); };

  auto node = interactionNode;
  while (node)
  {
    std::vector<Arg> nodeArgs;
    for (const auto& param: node.select_nodes("parameter"))
    {
      if (ignored(param.node().child_value("name")))
      {
        continue;
      }

      nodeArgs.push_back(argFromParameter(param.node(), document));
    }

    // A file beside the FOM adds a parameter to an interaction the same way it adds an attribute
    // to a class. Contributed parameters come after the ones this node declares, so the positions
    // of the existing ones do not move.
    //
    // Only an extension contributes here, unlike a class. Two modules of one directory declaring
    // one interaction are kept apart on purpose: apps/cli_gen/test/test18 has exactly that, and
    // structFromInteraction gives their structs different names so both can be generated.
    std::map<std::string, const Document*> argFrom;
    const auto key = indexKey("interactionClass", interactionClassPath(node));
    for (const auto& contribution: contributions(key, node, document))
    {
      if (!contribution.document->isExtension)
      {
        continue;
      }

      for (const auto& paramNode: contribution.node.children("parameter"))
      {
        if (ignored(paramNode.child_value("name")))
        {
          continue;
        }

        // The same choice the class path makes: an extension has no types of its own, a module
        // of the same package has.
        auto& typeSource = contribution.document->isExtension ? document : *contribution.document;
        auto arg = argFromParameter(paramNode, typeSource);
        auto itr =
          std::find_if(nodeArgs.begin(), nodeArgs.end(), [&arg](const auto& elem) { return elem.name == arg.name; });
        if (itr == nodeArgs.end())
        {
          argFrom.emplace(arg.name, contribution.document);
          nodeArgs.push_back(std::move(arg));
        }
        else if (!(*itr == arg))
        {
          const auto owner = argFrom.find(arg.name);
          refuseMemberClash("parameter",
                            arg.name,
                            node.child_value("name"),
                            owner != argFrom.end() ? *owner->second : document,
                            *contribution.document);
        }
      }
    }

    result = prependArgs(result, nodeArgs);

    // keep going up the hierarchy
    if (!node.parent().empty() && std::string(node.parent().name()) == "interactionClass" &&
        std::string(node.parent().child_value("name")) != "HLAinteractionRoot")
    {
      node = findInteraction(computeInteractionPath(node), document).node;
    }
    else
    {
      return result;
    }
  }

  return result;
}

ConstTypeHandle<> FomDocuments::structFromInteraction(pugi::xml_node node,
                                                      Document& document,
                                                      const std::vector<std::string>& ignoredParams)
{
  std::string name = capitalizeAndRemoveSeparators(node.child_value("name"));

  // A packed callable leaves out the parameters its mapping ignores, so it gets a struct of its
  // own rather than the interaction's. Without this both share one type and whichever was built
  // first decides the fields, which made the read order of the documents matter.
  // The struct a callable carries keeps the interaction's name: that is the type a user writes.
  // The one holding every parameter the FOM declares takes the suffix, and only where a mapping
  // packs this interaction while ignoring something, so both are needed at once.
  if (ignoredParams.empty() &&
      std::find(packedWithIgnores_.begin(), packedWithIgnores_.end(), interactionClassPath(node)) !=
        packedWithIgnores_.end())
  {
    name.append("Full");
  }

  auto sortedIgnores = ignoredParams;
  std::sort(sortedIgnores.begin(), sortedIgnores.end());

  // check if the struct was already created
  if (auto typeLookup = searchType(name, document))
  {
    const auto itr = argsStructIgnores_.find(senQualifiedName(document, name));
    if (itr != argsStructIgnores_.end() && itr->second != sortedIgnores)
    {
      std::string err;
      err.append("interaction '");
      err.append(node.child_value("name"));
      err.append("' is packed twice ignoring different parameters, so '");
      err.append(name);
      err.append("' cannot hold both. Ignore the same parameters in both mappings, or pack only one");
      throwRuntimeError(err);
    }
    return std::move(typeLookup).value().first;
  }

  // NOTE: Some NETN interactions have matching qualified names (e.g. netn::CapabilitiesSupported). In order to prevent
  // a multiple definition of these types in the generated code, we do the following:
  //  - Find types that are repeated among all the FOM documents that will be processed
  //  - Add a different suffix to each of the instances of these types, where the suffix contains the termination of the
  //    file name where the instance was defined (e.g. in the NETN-METOC file the identifier would be METOC)
  //  - The resulting type names have the corresponding suffixes, avoiding type redefinition (e.g.
  //    netn::CapabilitiesSupportedETR and netn::CapabilitiesSupportedMRM)

  const std::string targetQName = senQualifiedName(document, name);

  for (const auto& otherDoc: documents_)
  {
    // skip our current document
    if (otherDoc->name == document.name)
    {
      continue;
    }

    auto matches = [this, &otherDoc, &targetQName](const auto& otherInteractionNode) -> bool
    {
      const std::string otherQName =
        senQualifiedName(*otherDoc, capitalizeAndRemoveSeparators(otherInteractionNode.child_value("name")));
      return targetQName == otherQName;
    };

    if (std::any_of(otherDoc->interactionNodes.begin(), otherDoc->interactionNodes.end(), std::move(matches)))
    {
      name.append(splitString(document.path.stem().string(), "_-").back());
    }
  }

  // new struct
  auto specName = name;
  auto specQualifiedName = senQualifiedName(document, specName);
  auto specDescription = formatSemantics(node.child_value("semantics"));

  // do the same for fields as we do for args
  std::vector<StructField> fields;
  const auto args = collectArgs(node, ignoredParams, document);
  fields.reserve(args.size());
  for (const auto& arg: args)
  {
    fields.emplace_back(arg.name, arg.description, arg.type);
  }

  StructSpec spec(specName, specQualifiedName, specDescription, fields, std::nullopt);

  argsStructIgnores_.emplace(specQualifiedName, std::move(sortedIgnores));

  // store the type
  return storeType(StructType::make(spec), document);
}

CallableSpec FomDocuments::makeCallableSpec(pugi::xml_node interactionNode,
                                            const std::vector<std::string>& ignoredParams,
                                            Document& document,
                                            CallableEnum callableEnum,
                                            bool packArguments)
{
  auto transportStr = interactionNode.child_value("transportation");

  CallableSpec spec {};
  spec.name = toLowerCamelCase(interactionNode.child_value("name"));
  spec.description = formatSemantics(interactionNode.child_value("semantics"));
  const auto maybeTransportMode =
    callableEnum == CallableEnum::method ? getMethodTransportMode(transportStr) : getEventTransportMode(transportStr);
  if (!maybeTransportMode.has_value())
  {
    std::string err;
    err.append("unknown transportation '");
    err.append(transportStr);
    err.append("' on interaction '");
    err.append(interactionNode.child_value("name"));
    err.append("' in '");
    err.append(document.path.string());
    err.append("'");
    throwRuntimeError(err);
  }
  spec.transportMode = maybeTransportMode.value();

  if (packArguments)
  {
    auto argType = structFromInteraction(interactionNode, document, ignoredParams);

    // a single arg
    spec.args.emplace_back("args", "", argType);
  }
  else
  {
    spec.args = collectArgs(interactionNode, ignoredParams, document);
  }

  return spec;
}

EventSpec FomDocuments::makeEventSpec(pugi::xml_node interactionNode,
                                      const std::vector<std::string>& ignoredParams,
                                      Document& document,
                                      bool packArguments)
{
  EventSpec spec {};
  spec.callableSpec = makeCallableSpec(interactionNode, ignoredParams, document, CallableEnum::event, packArguments);
  return spec;
}

MethodSpec FomDocuments::makeMethodSpec(pugi::xml_node interactionNode,
                                        pugi::xml_node returnNode,
                                        const std::vector<std::string>& ignoredParams,
                                        Document& document,
                                        bool packArguments,
                                        bool localOnly,
                                        const std::string& qualifiedClassName)
{

  auto specCallableSpec =
    makeCallableSpec(interactionNode, ignoredParams, document, CallableEnum::method, packArguments);
  auto specConstness = Constness::nonConstant;
  auto specLocalOnly = localOnly;

  auto annotationItr = settings_.classAnnotations.find(qualifiedClassName);
  auto specDeferred = annotationItr != settings_.classAnnotations.end() &&
                      annotationItr->second.deferredMethods.count(specCallableSpec.name) != 0;

  ConstTypeHandle<> specReturnType = VoidType::get();

  if (returnNode)
  {
    if (!returnNode.attribute("deferred").empty())
    {
      const auto deferredStr = std::string(returnNode.attribute("deferred").value());
      specDeferred = deferredStr == "true" || deferredStr == "yes";
      std::cerr << "Warning: method " << specCallableSpec.name
                << " is mapped as 'deferred'. This is now deprecated. Please use code generation settings.\n";
    }

    auto interactionAttr = returnNode.attribute("hlaInteraction");
    if (interactionAttr)
    {
      std::string askedBy = "the return of method '";
      askedBy.append(specCallableSpec.name);
      askedBy.append("' on '");
      askedBy.append(qualifiedClassName);
      askedBy.append("'");
      auto returnInteractionNode = findInteractionAnywhere(interactionAttr.value(), askedBy);
      tryToAppendMappingDepToDoc(document, returnInteractionNode.document);

      std::vector<std::string> returnIgnoredParams;
      for (const auto& ignoredParamNode: returnNode.select_nodes("ignore"))
      {
        returnIgnoredParams.emplace_back(ignoredParamNode.node().attribute("parameter").value());
      }

      auto returnType =
        structFromInteraction(returnInteractionNode.node, *returnInteractionNode.document, returnIgnoredParams);

      specReturnType = returnType;
    }
    else
    {
      specReturnType = getOrCreateType(returnNode.attribute("dataType").value(), document).first;
    }
  }
  else
  {
    specReturnType = VoidType::get();
  }

  return {specCallableSpec, specReturnType, specConstness, NonPropertyRelated {}, specDeferred, specLocalOnly};
}

FomDocuments::FoundInteraction FomDocuments::findInteraction(const std::string& path, Document& document)
{
  const auto query = interactionQuery(path);

  // look for the interaction in the current document
  auto node = document.xml.select_node(query.c_str());
  if (node && !node.node().child("semantics").empty())  // NOLINT(readability-implicit-bool-conversion)
  {
    return {node.node(), &document};
  }

  const auto& ourPath = document.path;

  // search in the document dependencies
  for (auto* dependency: document.dependencies)
  {
    if (dependency->path != ourPath)
    {
      node = dependency->xml.select_node(query.c_str());
      if (node && !node.node().child("semantics").empty())  // NOLINT(readability-implicit-bool-conversion)
      {
        return {node.node(), dependency};
      }
    }
  }

  std::string err;
  err.append("could not find interaction '");
  err.append(path);
  err.append("', which '");
  err.append(document.path.string());
  err.append("' names as a parent, in that file or the modules it depends on");
  throwRuntimeError(err);
}

FomDocuments::FoundInteraction FomDocuments::findInteractionAnywhere(const std::string& path,
                                                                     const std::string& askedBy)
{
  const auto query = interactionQuery(path);

  pugi::xml_node interactionNode;
  Document* interactionDocument {nullptr};

  // search in all documents. An extension is skipped: it has no package and no dependencies, so
  // an interaction found there cannot be built, and reaching it ends in a type set nobody made.
  for (auto& document: documents_)
  {
    if (document->isExtension)
    {
      continue;
    }

    auto node = document->xml.select_node(query.c_str());
    if (node && !node.node().child("semantics").empty())  // NOLINT(readability-implicit-bool-conversion)
    {
      if (interactionNode)
      {
        std::string err;
        err.append("interaction '");
        err.append(path);
        err.append("' is declared by both '");
        err.append(interactionDocument->path.string());
        err.append("' and '");
        err.append(document->path.string());
        err.append("', so ");
        err.append(askedBy);
        err.append(" cannot say which one it means");
        throwRuntimeError(err);
      }
      interactionNode = node.node();
      interactionDocument = document.get();
    }
  }

  if (!interactionNode)
  {
    std::string err;
    err.append("could not find interaction '");
    err.append(path);
    err.append("', named by ");
    err.append(askedBy);
    err.append(": no module declares it");
    throwRuntimeError(err);
  }

  return {interactionNode, interactionDocument};
}

std::string FomDocuments::senName(const std::string& fomTypeName) const
{
  if (fomTypeName == "HLAASCIIchar")
  {
    return representationToType("HLAoctet")->getName().data();
  }
  if (fomTypeName == "HLAunicodeChar")
  {
    return representationToType("HLAoctetPairBE")->getName().data();
  }
  if (fomTypeName == "HLAbyte")
  {
    return representationToType("HLAoctet")->getName().data();
  }
  if (fomTypeName == "HLAcount")
  {
    return representationToType("HLAinteger32BE")->getName().data();
  }
  if (fomTypeName == "HLAseconds")
  {
    return representationToType("HLAinteger32BE")->getName().data();
  }
  if (fomTypeName == "HLAmsec")
  {
    return representationToType("HLAinteger32BE")->getName().data();
  }
  if (fomTypeName == "HLAindex")
  {
    return representationToType("HLAinteger32BE")->getName().data();
  }
  if (fomTypeName == "HLAinteger64Time")
  {
    return representationToType("HLAinteger64BE")->getName().data();
  }
  if (fomTypeName == "HLAfloat64Time")
  {
    return representationToType("HLAfloat64BE")->getName().data();
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

  return capitalizeAndRemoveSeparators(fomTypeName);
}

std::string FomDocuments::senQualifiedName(const Document& document, const std::string& fomTypeName) const
{
  return withPackage(document, senName(fomTypeName));
}

std::string FomDocuments::withPackage(const Document& document, const std::string& str) const
{
  std::string result;
  result.append(document.package);
  result.append(".");
  result.append(str);
  return result;
}

ConstTypeHandle<> FomDocuments::representationToType(const std::string& representation) const
{
  auto itr = representations_.find(representation);
  if (itr == representations_.end())
  {
    std::string err;
    err.append("data representation '");
    err.append(representation);
    err.append("' is not yet supported");
    throwRuntimeError(err);
  }

  return itr->second;
}

}  // namespace sen::lang::fom
