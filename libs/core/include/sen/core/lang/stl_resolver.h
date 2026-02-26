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

/// A set of types coming from a STL file.
struct TypeSet
{
  std::string fileName;
  std::string parentDirectory;
  std::vector<std::string> package;
  std::vector<ConstTypeHandle<sen::CustomType>> types;
  std::vector<const TypeSet*> importedSets;
};

/// The environment where a resolution takes place.
struct ResolverContext
{
  std::filesystem::path originalFileName;
  std::filesystem::path absoluteFileName;
  std::vector<std::filesystem::path> includePaths;
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

/// Resolves a list of statements into a set of types.
class StlResolver
{
public:
  /// Stores the statements, context, and the global context.
  StlResolver(const std::vector<StlStatement>& statements,
              const ResolverContext& context,
              TypeSetContext& globalContext) noexcept;

  /// Do the resolution based on the inputs passed on the constructor.
  [[nodiscard]] const TypeSet* resolve(const TypeSettings& settings);

private:
  const std::vector<StlStatement>& statements_;
  const ResolverContext& context_;
  TypeSetContext& globalContext_;
};

/// Helper function to use the resolver.
[[nodiscard]] const TypeSet* readTypesFile(const std::filesystem::path& fileName,
                                           const std::vector<std::filesystem::path>& includePaths,
                                           TypeSetContext& globalTypeSetContext,
                                           const TypeSettings& settings,
                                           std::string_view from = {});

/// @}

}  // namespace sen::lang

#endif  // SEN_CORE_LANG_STL_RESOLVER_H
