// === fom_parser.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/lang/fom_parser.h"

// implementation
#include "fom_documents.h"

// sen
#include "sen/core/lang/stl_resolver.h"

// std
#include <filesystem>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

namespace sen::lang
{

struct FomParser::FomParserImpl
{
  FomParserImpl(const std::vector<std::filesystem::path>& paths,
                const std::vector<std::filesystem::path>& extensions,
                const std::vector<std::filesystem::path>& mappings,
                const TypeSettings& settings)
    : documents(paths, extensions, mappings, settings)
  {
  }

  // NOLINTNEXTLINE(misc-non-private-member-variables-in-classes): no invariance
  fom::FomDocuments documents;
};

/// Stores the tokens for eventual parsing.
FomParser::FomParser(const std::vector<std::filesystem::path>& paths,
                     const std::vector<std::filesystem::path>& extensions,
                     const std::vector<std::filesystem::path>& mappings,
                     const TypeSettings& settings)
  : pimpl_(std::make_unique<FomParserImpl>(paths, extensions, mappings, settings))
{
}

FomParser::~FomParser() = default;

TypeSetContext FomParser::computeTypeSets()
{
  auto sets = pimpl_->documents.takeTypeSets();

  TypeSetContext result;
  result.reserve(sets.size());

  for (auto& [path, set]: sets)
  {
    std::ignore = path;
    result.append(std::move(set));
  }

  return result;
}

TypeSetContext FomParser::convertToCompleteTypeSetContext() &&
{
  auto typeSetContext = computeTypeSets();
  typeSetContext.prepend(std::move(pimpl_->documents).takeRootTypeSet());
  return typeSetContext;
}

const lang::TypeSet& FomParser::getRootTypeSet() const& noexcept { return pimpl_->documents.rootTypeSet(); }

}  // namespace sen::lang
