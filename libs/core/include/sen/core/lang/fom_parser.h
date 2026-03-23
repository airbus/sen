// === fom_parser.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_LANG_FOM_PARSER_H
#define SEN_CORE_LANG_FOM_PARSER_H

#include "sen/core/lang/stl_resolver.h"

// std
#include <filesystem>
#include <vector>

namespace sen::lang
{

/// \addtogroup lang
/// @{

/// Parses one or more HLA FOM (Federation Object Model) files and produces a `TypeSetContext`.
///
/// The parser reads FOM documents, applies optional name-mapping rules, and runs the full
/// STL resolver pipeline to build a resolved set of Sen types. Use `parseFomDocuments()` as
/// a one-shot convenience wrapper, or construct a `FomParser` directly when you need the
/// intermediate `getRootTypeSet()` result.
class FomParser
{
  SEN_NOCOPY_NOMOVE(FomParser)

public:
  /// Scans and tokenises all FOM documents, ready for parsing.
  /// @param paths     Paths to the FOM files to parse (processed in order).
  /// @param mappings  Paths to name-mapping files that rename HLA types to Sen conventions.
  /// @param settings  Type settings controlling how types are created (e.g. transport defaults).
  FomParser(const std::vector<std::filesystem::path>& paths,
            const std::vector<std::filesystem::path>& mappings,
            const TypeSettings& settings);
  ~FomParser();

public:
  /// Resolves all loaded FOM documents and returns a `TypeSetContext` of the non-root type sets.
  ///
  /// The root type set (containing built-in Sen types) is excluded from the result but is
  /// retained by this `FomParser` instance and accessible via `getRootTypeSet()`.
  /// For a context that also includes the root type set, call `convertToCompleteTypeSetContext()`.
  /// @return `TypeSetContext` of resolved, non-root type sets.
  TypeSetContext computeTypeSets();

  /// Resolves all loaded FOM documents and returns a complete `TypeSetContext` that also includes
  /// the root type set. Consumes the `FomParser` (rvalue-only).
  /// @return Complete `TypeSetContext` including both root and non-root type sets.
  TypeSetContext convertToCompleteTypeSetContext() &&;

  /// @return The resolved root `TypeSet` containing built-in Sen types.
  [[nodiscard]] const lang::TypeSet& getRootTypeSet() const& noexcept;

private:
  struct FomParserImpl;
  std::unique_ptr<FomParserImpl> pimpl_;
};

/// One-shot helper: parses @p paths with @p mappings and returns the complete `TypeSetContext`.
/// Equivalent to constructing a `FomParser` and calling `convertToCompleteTypeSetContext()`.
/// @param paths     Paths to the FOM files to parse.
/// @param mappings  Paths to name-mapping files.
/// @param settings  Type settings for the resolver pipeline.
/// @return Complete `TypeSetContext` with all resolved types.
inline TypeSetContext parseFomDocuments(const std::vector<std::filesystem::path>& paths,
                                        const std::vector<std::filesystem::path>& mappings,
                                        const TypeSettings& settings)
{
  sen::lang::FomParser documents(paths, mappings, settings);
  return std::move(documents).convertToCompleteTypeSetContext();
}

/// @}

}  // namespace sen::lang

#endif  // SEN_CORE_LANG_FOM_PARSER_H
