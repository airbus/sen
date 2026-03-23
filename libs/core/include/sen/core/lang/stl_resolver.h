// === stl_resolver.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_LANG_STL_RESOLVER_H
#define SEN_CORE_LANG_STL_RESOLVER_H

#include "sen/core/lang/stl_statement.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/type.h"

// std
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sen::lang
{

/// \addtogroup lang
/// @{

struct TypeSet;

/// The resolved output of a single STL file: the package path, all declared types,
/// and pointers to any imported `TypeSet`s that this file depends on.
struct TypeSet
{
  std::string fileName;              ///< Base file name (without directory) of the STL source.
  std::string parentDirectory;       ///< Directory containing the STL source file.
  std::vector<std::string> package;  ///< Fully-qualified package path (e.g. `{"sen", "geometry"}`).
  std::vector<ConstTypeHandle<sen::CustomType>>
    types;                                   ///< All custom types declared in this file, in declaration order.
  std::vector<const TypeSet*> importedSets;  ///< Non-owning pointers to directly imported `TypeSet`s.
};

/// Filesystem context supplied to `StlResolver` so it can locate imported STL files.
struct ResolverContext
{
  std::filesystem::path originalFileName;           ///< Path as written in the import statement or command line.
  std::filesystem::path absoluteFileName;           ///< Canonical absolute path of the STL file being resolved.
  std::vector<std::filesystem::path> includePaths;  ///< Search paths tried in order when resolving imports.
};

/// The set of types used during resolution.
class TypeSetContext
{
  using TypeSetStorageContainerType = std::vector<std::unique_ptr<TypeSet>>;

public:
  class TypeSetIterator: public TypeSetStorageContainerType::const_iterator
  {
    using IterBaseTy = TypeSetStorageContainerType::const_iterator;

  public:
    // NOLINTNEXTLINE(readability-identifier-naming)
    using value_type = typename TypeSetStorageContainerType::const_iterator::value_type::element_type;
    using pointer = value_type*;    // NOLINT(readability-identifier-naming)
    using reference = value_type&;  // NOLINT(readability-identifier-naming)

    reference operator*() const { return *this->IterBaseTy::operator*(); }
    pointer operator->() const { return this->IterBaseTy::operator*().get(); }
    reference operator[](IterBaseTy::difference_type idx) const { return *(this->IterBaseTy::operator[](idx)); }
  };

public:
  [[nodiscard]] TypeSet* createNewTypeSet() noexcept(noexcept(TypeSet()))
  {
    return typeSets_.emplace_back(std::make_unique<TypeSet>()).get();
  }

  void prepend(std::unique_ptr<TypeSet> newTypeSet) { typeSets_.insert(typeSets_.begin(), std::move(newTypeSet)); }
  void append(std::unique_ptr<TypeSet> newTypeSet) { typeSets_.push_back(std::move(newTypeSet)); }
  void reserve(size_t n) { typeSets_.reserve(n); }
  [[nodiscard]] size_t size() const { return typeSets_.size(); }

  [[nodiscard]] TypeSetIterator begin() const { return {typeSets_.cbegin()}; }
  [[nodiscard]] TypeSetIterator end() const { return {typeSets_.cend()}; }

private:
  TypeSetStorageContainerType typeSets_;
};

struct ClassAnnotations
{
  std::unordered_set<std::string> checkedProperties;
  std::unordered_set<std::string> deferredMethods;
};

struct TypeSettings
{
  /// Qualified class name to its annotations.
  std::unordered_map<std::string, ClassAnnotations> classAnnotations;
};

/// Semantic analyser that walks a parsed STL statement list and builds a `TypeSet`.
/// Third stage of the STL pipeline: AST (`StlStatement` list) â†’ `TypeSet` â†’ type registry.
class StlResolver
{
public:
  /// Constructs the resolver with all inputs needed for a single-file resolution pass.
  /// @param statements   AST produced by `StlParser::parse()` for the file being resolved.
  /// @param context      Filesystem context used to locate imported STL files.
  /// @param globalContext Shared registry of all previously resolved `TypeSet`s; new sets
  ///                     for this file and its imports are added here during `resolve()`.
  StlResolver(const std::vector<StlStatement>& statements,
              const ResolverContext& context,
              TypeSetContext& globalContext) noexcept;

  /// Runs the full resolution pass and returns the `TypeSet` for the primary file.
  /// Imports are resolved recursively; all resulting sets are registered in `globalContext`.
  /// @param settings  Per-type annotations (e.g. checked properties, deferred methods).
  /// @return Non-owning pointer to the newly created `TypeSet`; owned by `globalContext`.
  [[nodiscard]] const TypeSet* resolve(const TypeSettings& settings);

private:
  const std::vector<StlStatement>& statements_;
  const ResolverContext& context_;
  TypeSetContext& globalContext_;
};

/// Convenience helper that runs the full STL pipeline (scan â†’ parse â†’ resolve) for a file.
/// Handles include-path resolution and recursive import processing automatically.
/// @param fileName           Path to the `.stl` file to read and resolve.
/// @param includePaths       Directories to search when resolving `import` statements.
/// @param globalTypeSetContext Shared `TypeSet` registry; resolved sets are appended here.
/// @param settings           Per-type annotations forwarded to `StlResolver::resolve()`.
/// @param from               Optional caller name used in error messages (e.g. a parent file path).
/// @return Non-owning pointer to the `TypeSet` for `fileName`; owned by `globalTypeSetContext`.
/// @throws std::runtime_error if the file cannot be read, scanned, parsed, or resolved.
[[nodiscard]] const TypeSet* readTypesFile(const std::filesystem::path& fileName,
                                           const std::vector<std::filesystem::path>& includePaths,
                                           TypeSetContext& globalTypeSetContext,
                                           const TypeSettings& settings,
                                           std::string_view from = {});

/// @}

}  // namespace sen::lang

#endif  // SEN_CORE_LANG_STL_RESOLVER_H
