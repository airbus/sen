// === fom_document_set.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_CORE_SRC_LANG_FOM_DOCUMENT_SET_H
#define SEN_LIBS_CORE_SRC_LANG_FOM_DOCUMENT_SET_H

// sen
#include "sen/core/lang/stl_resolver.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"

// xml parser
#include "pugixml.hpp"

// std
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>

namespace sen::lang
{

struct FomDocument
{
  std::filesystem::path filePath;
  pugi::xml_document xmlDoc;
  std::string name;
  std::string packageName;
  std::vector<FomDocument*> deps;
  std::vector<FomDocument*> mappingDeps;
  std::vector<pugi::xml_node> definedClassesNodes;
  std::vector<pugi::xml_node> definedInteractionNodes;
};

struct ParsedDoc
{
  FomDocument* doc;
  CustomTypeRegistry registry;
  std::vector<ConstTypeHandle<>> storage;
};

class FomDocumentSet
{
  SEN_MOVE_ONLY(FomDocumentSet)
public:
  using DocStorageList = std::vector<std::unique_ptr<FomDocument>>;

public:
  explicit FomDocumentSet(const std::vector<std::filesystem::path>& paths,
                          const std::vector<std::filesystem::path>& mappings,
                          TypeSettings settings);
  ~FomDocumentSet() = default;

public:
  [[nodiscard]] std::map<const FomDocument*, std::unique_ptr<lang::TypeSet>> computeTypeSets() const;

  [[nodiscard]] const lang::TypeSet& getRootTypeSet() const& noexcept { return *rootTypeSet_; }
  [[nodiscard]] std::unique_ptr<lang::TypeSet> getRootTypeSet() && noexcept { return std::move(rootTypeSet_); }

private:
  [[nodiscard]] std::optional<std::pair<ConstTypeHandle<>, ParsedDoc*>> searchTypeSenName(
    const std::string& qualName) const;

  std::optional<std::pair<ConstTypeHandle<>, ParsedDoc*>> searchTypeFromFomNameHelper(const std::string& fomTypeName,
                                                                                      ParsedDoc* doc);

  [[nodiscard]] std::optional<std::pair<ConstTypeHandle<>, ParsedDoc*>> searchTypeFromFomName(
    const std::string& fomTypeName,
    ParsedDoc* doc);

  [[nodiscard]] FomDocument* searchDocByIdentification(const std::string& identification);

private:
  enum class CallableEnum : uint8_t
  {
    method,
    event
  };

  struct FindInteractionResult
  {
    pugi::xpath_node node;
    ParsedDoc* doc;
  };

private:
  void readDocs(const std::filesystem::path& path);

  void resolveDeps();

  [[nodiscard]] ParsedDoc* toMutableParsedDoc(const ParsedDoc* doc) { return parsedDocs_.at(doc->doc).get(); }

  [[nodiscard]] ParsedDoc* getParsedDoc(FomDocument* doc);

  void populateTypes(ParsedDoc* result, FomDocument* doc);

  [[nodiscard]] ConstTypeHandle<> storeType(ConstTypeHandle<> type, ParsedDoc* doc);

  void createRootTypeSet();

  template <typename F>
  [[nodiscard]] ConstTypeHandle<> processTypeSource(ParsedDoc* result, const pugi::xpath_node& node, F func);

  template <typename F>
  void processTypeSource(ParsedDoc* result, const pugi::xpath_node_set& nodes, F func);

  template <typename F>
  std::optional<ConstTypeHandle<>> processTypeQuery(ParsedDoc* result,
                                                    const std::string& query,
                                                    const std::string& queryPostFix,
                                                    F func);

  std::pair<ConstTypeHandle<>, ParsedDoc*> getOrCreateTypeFromFomName(const std::string& fomName, ParsedDoc* result);

private:
  [[nodiscard]] ConstTypeHandle<> simpleData(const pugi::xpath_node& node, ParsedDoc* doc);

  [[nodiscard]] ConstTypeHandle<> arrayType(const pugi::xpath_node& node, const ParsedDoc* doc);

  [[nodiscard]] ConstTypeHandle<> recordType(const pugi::xpath_node& node, ParsedDoc* doc);

  [[nodiscard]] ConstTypeHandle<> variantRecord(const pugi::xpath_node& node, const ParsedDoc* doc);

  [[nodiscard]] ConstTypeHandle<> enumeration(const pugi::xpath_node& node, const ParsedDoc* doc);

  [[nodiscard]] ConstTypeHandle<> classNode(const pugi::xpath_node& node, ParsedDoc* doc);

  void basicDataRepresentation(const pugi::xpath_node& node);

private:
  [[nodiscard]] std::string fomTypeNameToSenTypeNameUnequal(const std::string& fomTypeName) const;

  [[nodiscard]] std::string fomTypeNameToSenTypeNameQual(const FomDocument* doc, const std::string& fomTypeName) const;

  [[nodiscard]] ConstTypeHandle<> fomDataRepresentationToSenType(const std::string& representation) const;

  [[nodiscard]] std::string prependSenPackageName(const FomDocument* doc, const std::string& str) const;

  [[nodiscard]] std::vector<Arg> collectArgsFromInteractionNode(const pugi::xpath_node& interactionNode,
                                                                const std::vector<std::string>& ignoredParams,
                                                                ParsedDoc* doc);

  [[nodiscard]] ConstTypeHandle<> getOrMakeStructFromInteraction(const pugi::xpath_node& node,
                                                                 ParsedDoc* doc,
                                                                 const std::vector<std::string>& ignoredParams);

  [[nodiscard]] CallableSpec makeCallableSpec(const pugi::xpath_node& interactionNode,
                                              const std::vector<std::string>& ignoredParams,
                                              ParsedDoc* doc,
                                              CallableEnum callableEnum,
                                              bool packArguments);

  [[nodiscard]] EventSpec makeEventSpec(const pugi::xpath_node& interactionNode,
                                        const std::vector<std::string>& ignoredParams,
                                        ParsedDoc* doc,
                                        bool packArguments);

  [[nodiscard]] MethodSpec makeMethodSpec(const pugi::xpath_node& interactionNode,
                                          const pugi::xpath_node& returnNode,
                                          const std::vector<std::string>& ignoredParams,
                                          ParsedDoc* doc,
                                          bool packArguments,
                                          bool localOnly,
                                          const std::string& qualifiedClassName);

  [[nodiscard]] FindInteractionResult findInteractionNode(const std::string& path, const ParsedDoc* doc);

  [[nodiscard]] FindInteractionResult findInteractionNodeInAllDocs(const std::string& path);

  [[nodiscard]] ConstTypeHandle<> getOptionalPropertyType(const std::string fomElementTypeName, ParsedDoc* doc);

private:
  DocStorageList docs_;  ///< manages the lifetime of all FomDocuments
  std::unordered_map<FomDocument*, std::shared_ptr<ParsedDoc>> parsedDocs_;
  std::unordered_map<std::string, ConstTypeHandle<>> representationToTypeMap_;
  std::unordered_map<std::string, std::string> stringToSenUnitAbbrMap_;
  std::vector<pugi::xml_document> mappingsDocs_;
  std::unique_ptr<lang::TypeSet> rootTypeSet_;
  std::optional<TypeHandle<ClassType>> rootClass_;
  TypeSettings settings_;
};

}  // namespace sen::lang

#endif  // SEN_LIBS_CORE_SRC_LANG_FOM_DOCUMENT_SET_H
