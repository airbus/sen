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

/// Parses HLA FOM files
class FomParser
{
  SEN_NOCOPY_NOMOVE(FomParser)

public:
  /// Stores the tokens for eventual parsing.
  FomParser(const std::vector<std::filesystem::path>& paths,
            const std::vector<std::filesystem::path>& mappings,
            const TypeSettings& settings);
  ~FomParser();

public:
  /// Computes all underlying type sets and puts them into a TypeSetContext.
  ///
  /// Note: By default, the root type set is not included but still stored in the FomParser. For a complete
  /// TypeSetContext call @ref convertToCompleteTypeSetContext.
  TypeSetContext computeTypeSets();

  /// Computes all underlying type sets and returns a complete TypeSetContext including the current root @ref TypeSet.
  TypeSetContext convertToCompleteTypeSetContext() &&;

  [[nodiscard]] const lang::TypeSet& getRootTypeSet() const& noexcept;

private:
  struct FomParserImpl;
  std::unique_ptr<FomParserImpl> pimpl_;
};

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
