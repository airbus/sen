// === fom_documents.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_CORE_SRC_LANG_FOM_DOCUMENTS_H
#define SEN_LIBS_CORE_SRC_LANG_FOM_DOCUMENTS_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/lang/stl_resolver.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"

// xml parser
#include "pugixml.hpp"

// std
#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sen::lang::fom
{

/// One document that was read, and the nodes in it that declare something.
struct Document
{
  std::filesystem::path path;
  pugi::xml_document xml;
  pugi::xml_node objectModel;
  std::string name;     ///< modelIdentification/name, used to resolve dependency references
  std::string package;  ///< the directory name in lower case
  bool isExtension {};  ///< from the extensions input: adds members, never owns a type
  bool built {};        ///< guards ensureBuilt's recursion
  std::vector<Document*> dependencies;
  std::vector<Document*> mappingImports;  ///< documents a mapping pulled in
  std::vector<pugi::xml_node> classNodes;
  std::vector<pugi::xml_node> interactionNodes;

  /// The types this document owns. They become its type set, so this decides which generated
  /// file each type lands in.
  CustomTypeRegistry registry;
  std::vector<ConstTypeHandle<>> storage;
};

/// A mapping file and where it was read from, so a refusal can name the file that asked for
/// something the FOM does not have.
struct MappingDocument final
{
  std::filesystem::path path;
  pugi::xml_document xml;
};

/// A type set and the path it was built from. One caller resolves a single FOM file by path.
struct DocumentTypeSet
{
  std::filesystem::path path;
  std::unique_ptr<TypeSet> typeSet;
};

/// One declaring node and the document it came from, so a refusal can name the file.
struct Declaration
{
  pugi::xml_node node;
  Document* document {};
};

/// Everything that declares one type. The complete definition owns the type and decides its
/// package; every other declaration adds its members to it. HLA 4 calls this the union over the
/// collective set of modules, and uses the same two words for the two roles.
struct Declarations
{
  std::vector<Declaration> all;  ///< in the order the documents were read
  Document* owner {};            ///< null when only extensions declare the type
  bool ownerIsComplete {false};  ///< the owner's declaration carries semantics
};

/// Reads every document, indexes what each one declares, then builds the types. A type is built
/// on first reference, from every declaration of it rather than only the first, which is what
/// lets a file beside the FOM add members to it.
class FomDocuments
{
  SEN_NOCOPY_NOMOVE(FomDocuments)

public:
  FomDocuments(const std::vector<std::filesystem::path>& paths,
               const std::vector<std::filesystem::path>& extensions,
               const std::vector<std::filesystem::path>& mappings,
               TypeSettings settings);
  ~FomDocuments() = default;

public:
  /// One set per document, in the order the documents were read, which follows the directories
  /// given on the command line. An extension gets none: it owns no type.
  [[nodiscard]] std::vector<DocumentTypeSet> takeTypeSets();
  [[nodiscard]] const TypeSet& rootTypeSet() const& noexcept;

  /// Hands over the root set, which leaves this reader without one. Rvalue-qualified so a
  /// second call is a compile error rather than a null dereference.
  [[nodiscard]] std::unique_ptr<TypeSet> takeRootTypeSet() &&;

private:
  //--------------------------------------------------------------------------------------------------------------
  // Reading
  //--------------------------------------------------------------------------------------------------------------

  /// Reads every `.xml` in a directory as a document of that package, except a file that was
  /// also named as an extension: putting one beside the modules is natural, and read as a module
  /// it would declare types in that package instead of adding members.
  void readDirectory(const std::filesystem::path& path);

  /// Reads one file, or every `.xml` in a directory, as an extension. An extension takes no
  /// package: it adds members to types the modules own.
  void readExtension(const std::filesystem::path& path);

  /// Loads one file and records what it declares. An extension whose root is not an object
  /// model is refused; a module directory may hold other XML, and that is left alone.
  Document& readDocument(const std::filesystem::path& path, const std::string& package, bool isExtension);

  /// Resolves the Dependency references of every document. A reference to a document that is
  /// not in the set is an error; one to the MIM is ignored.
  void resolveDependencies();

  /// The document a dependency reference names. The match is a case-insensitive substring of
  /// the model name or the file stem, because a reference carries a short form of the name.
  [[nodiscard]] Document* findDocument(const std::string& identification) const;

  //--------------------------------------------------------------------------------------------------------------
  // Indexing
  //--------------------------------------------------------------------------------------------------------------

  /// Records every declaration against its key. The merge needs them all before any type is built.
  void indexDeclarations();

  /// Adds one declaring node under a key, and settles the owner on the first that can own.
  void record(const std::string& key, pugi::xml_node node, Document& document);

  /// The owner's declaration under a key, when another document of the same package declared it
  /// first. Two documents of one package building one type would emit the same names twice in
  /// one namespace, which does not compile.
  [[nodiscard]] std::optional<Declaration> ownedElsewhere(const std::string& key, const Document& document) const;

  /// The declarations under a key that are not the given one. These add members to the type.
  [[nodiscard]] std::vector<Declaration> contributions(const std::string& key,
                                                       pugi::xml_node own,
                                                       const Document& document) const;

  /// The sole declaration of a type no complete definition declares, or nothing. This is a type
  /// an extension introduced, which the document that referenced it adopts.
  [[nodiscard]] std::optional<Declaration> introducedType(const std::string& category,
                                                          const std::string& fomName) const;

  /// Builds the `hla` type set: the object root class and one optional per native type.
  void buildRootTypeSet();

  //--------------------------------------------------------------------------------------------------------------
  // Building
  //--------------------------------------------------------------------------------------------------------------

  /// Builds a document's types, its dependencies' first. A reference into a dependency needs
  /// that dependency's types, so this is not the order the documents were read.
  void ensureBuilt(Document& document);

  /// Builds every type one document declares, in the order the categories are listed.
  void populateTypes(Document& document);

  /// Builds the type a node declares, keeping the first of a repeat within one document.
  template <typename F>
  [[nodiscard]] ConstTypeHandle<> processTypeSource(Document& document, const pugi::xpath_node& node, F func);

  template <typename F>
  void processTypeSource(Document& document, const pugi::xpath_node_set& nodes, F func);

  /// The same for the first node a query selects, or nothing if it selects none.
  template <typename F>
  [[nodiscard]] std::optional<ConstTypeHandle<>> processTypeQuery(Document& document,
                                                                  const std::string& query,
                                                                  const std::string& queryPostFix,
                                                                  F func);

  /// Keeps a type against its qualified name. A name already there is returned instead.
  [[nodiscard]] ConstTypeHandle<> storeType(ConstTypeHandle<> type, Document& document);

  /// The type for a FOM name, built if it is not there yet. Looked for in this document, then
  /// its dependencies; the one the search succeeds in owns the type.
  [[nodiscard]] std::pair<ConstTypeHandle<>, Document*> getOrCreateType(const std::string& fomName, Document& document);

  /// A type already built, by FOM name or by sen name.
  [[nodiscard]] std::optional<std::pair<ConstTypeHandle<>, Document*>> searchType(const std::string& fomName,
                                                                                  Document& document);
  [[nodiscard]] std::optional<std::pair<ConstTypeHandle<>, Document*>> searchTypeHere(const std::string& fomName,
                                                                                      Document& document);
  [[nodiscard]] std::optional<std::pair<ConstTypeHandle<>, Document*>> searchBySenName(
    const std::string& qualifiedName) const;

  /// One datatype category: the element that declares it, the container it sits in, and the
  /// builder for it.
  struct Category
  {
    const char* container;
    const char* element;
    ConstTypeHandle<> (FomDocuments::*build)(pugi::xml_node, Document&);
  };

  /// The categories in the order a name is looked up in them, so a name declared in two of
  /// them resolves to the first. Stated here once; indexing, building and resolution all read it.
  [[nodiscard]] static const std::array<Category, 5>& dataTypeCategories();

  /// The query that selects every declaration of a category.
  [[nodiscard]] static std::string categoryQuery(const Category& category);

  [[nodiscard]] ConstTypeHandle<> simpleData(pugi::xml_node node, Document& document);
  [[nodiscard]] ConstTypeHandle<> arrayType(pugi::xml_node node, Document& document);
  [[nodiscard]] ConstTypeHandle<> recordType(pugi::xml_node node, Document& document);
  [[nodiscard]] ConstTypeHandle<> variantRecord(pugi::xml_node node, Document& document);
  [[nodiscard]] ConstTypeHandle<> enumeration(pugi::xml_node node, Document& document);
  [[nodiscard]] ConstTypeHandle<> classNode(pugi::xml_node node, Document& document);

  /// Applies the mappings to a class: a property made writable, an interaction turned into an
  /// event or a method.
  void applyMappings(ClassSpec& classSpec,
                     std::vector<PropertySpec>& propertySpecs,
                     const MaybeConstTypeHandle<>& parentType,
                     Document& document);

  /// One property, from the attribute node that declares it. A contributed attribute is read
  /// the same way as one the class declares itself. The annotations are null when the class has
  /// none.
  [[nodiscard]] PropertySpec propertyFromAttribute(pugi::xml_node attrNode,
                                                   Document& document,
                                                   const ClassAnnotations* annotations);

  /// Reads a basicData entry so a FOM's own representations resolve.
  void basicDataRepresentation(pugi::xml_node node, const Document& document);

  /// The optional that wraps a member's type.
  [[nodiscard]] ConstTypeHandle<> optionalPropertyType(const std::string& fomElementTypeName, Document& document);

  //--------------------------------------------------------------------------------------------------------------
  // Interactions
  //--------------------------------------------------------------------------------------------------------------

  enum class CallableEnum : uint8_t
  {
    method,
    event
  };

  struct FoundInteraction
  {
    pugi::xml_node node;
    Document* document {};
  };

  [[nodiscard]] FoundInteraction findInteraction(const std::string& path, Document& document);
  [[nodiscard]] FoundInteraction findInteractionAnywhere(const std::string& path, const std::string& askedBy);
  [[nodiscard]] Arg argFromParameter(pugi::xml_node paramNode, Document& document);
  [[nodiscard]] std::vector<Arg> collectArgs(pugi::xml_node interactionNode,
                                             const std::vector<std::string>& ignoredParams,
                                             Document& document);
  [[nodiscard]] ConstTypeHandle<> structFromInteraction(pugi::xml_node node,
                                                        Document& document,
                                                        const std::vector<std::string>& ignoredParams);
  [[nodiscard]] CallableSpec makeCallableSpec(pugi::xml_node interactionNode,
                                              const std::vector<std::string>& ignoredParams,
                                              Document& document,
                                              CallableEnum callableEnum,
                                              bool packArguments);
  [[nodiscard]] EventSpec makeEventSpec(pugi::xml_node interactionNode,
                                        const std::vector<std::string>& ignoredParams,
                                        Document& document,
                                        bool packArguments);
  [[nodiscard]] MethodSpec makeMethodSpec(pugi::xml_node interactionNode,
                                          pugi::xml_node returnNode,
                                          const std::vector<std::string>& ignoredParams,
                                          Document& document,
                                          bool packArguments,
                                          bool localOnly,
                                          const std::string& qualifiedClassName);

  //--------------------------------------------------------------------------------------------------------------
  // Names
  //--------------------------------------------------------------------------------------------------------------

  [[nodiscard]] std::string senName(const std::string& fomTypeName) const;
  [[nodiscard]] std::string senQualifiedName(const Document& document, const std::string& fomTypeName) const;
  [[nodiscard]] std::string withPackage(const Document& document, const std::string& str) const;
  [[nodiscard]] ConstTypeHandle<> representationToType(const std::string& representation) const;

private:
  TypeSettings settings_;
  std::vector<std::unique_ptr<Document>> documents_;
  std::vector<MappingDocument> mappings_;

  /// The ignored parameters each callable's struct was built from, by qualified name. A packed
  /// callable trims the interaction's parameters, so two callables trimming one interaction
  /// differently would otherwise be given whichever struct was built first.
  std::map<std::string, std::vector<std::string>> argsStructIgnores_;

  /// Interactions some mapping packs while ignoring a parameter. Those need two structs: the one
  /// the callable carries, and the one holding every parameter the FOM declares. The callable's
  /// keeps the interaction's own name, because that is the name written in code; the complete one
  /// takes a suffix. Interactions not named here are unaffected and keep their single struct.
  std::vector<std::string> packedWithIgnores_;

  /// The extension files, so a directory scan does not read one of them as a module.
  std::vector<std::filesystem::path> extensionPaths_;

  /// Keyed on the declaring element and the name, so the kinds cannot collide.
  std::map<std::string, Declarations> declarations_;

  std::unordered_map<std::string, ConstTypeHandle<>> representations_;
  std::unordered_map<std::string, std::string> units_;
  std::unique_ptr<TypeSet> rootSet_;
  std::optional<TypeHandle<ClassType>> rootClass_;

  /// Guards a type that names itself, directly or through a chain. The document is part of the
  /// key because resolving the same name in a dependency is how the search descends.
  std::vector<std::pair<std::string, const Document*>> resolving_;
};

}  // namespace sen::lang::fom

#endif  // SEN_LIBS_CORE_SRC_LANG_FOM_DOCUMENTS_H
