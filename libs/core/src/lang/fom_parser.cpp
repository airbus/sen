// === fom_parser.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/lang/fom_parser.h"

// implementation
#include "fom_document_set.h"

// sen
#include "sen/core/lang/stl_resolver.h"

// std
#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

namespace sen::lang
{

struct FomParser::FomParserImpl
{
  explicit FomParserImpl(FomDocumentSet fomDocumentSet): set(std::move(fomDocumentSet)) {}
  FomDocumentSet set;  // NOLINT(misc-non-private-member-variables-in-classes): no invariance
};

/// Stores the tokens for eventual parsing.
FomParser::FomParser(const std::vector<std::filesystem::path>& paths,
                     const std::vector<std::filesystem::path>& mappings,
                     const TypeSettings& settings)
  : pimpl_(std::make_unique<FomParserImpl>(FomDocumentSet(paths, mappings, settings)))
{
}

FomParser::~FomParser() = default;

TypeSetContext FomParser::computeTypeSets()
{
  auto map = pimpl_->set.computeTypeSets();

  TypeSetContext result;
  result.reserve(map.size());

  for (auto& [doc, set]: map)
  {
    result.append(std::move(set));
  }

  return result;
}

TypeSetContext FomParser::convertToCompleteTypeSetContext() &&
{
  auto typeSetContext = computeTypeSets();
  typeSetContext.prepend(std::move(pimpl_->set).getRootTypeSet());
  return typeSetContext;
}

const lang::TypeSet& FomParser::getRootTypeSet() const& noexcept { return pimpl_->set.getRootTypeSet(); }

}  // namespace sen::lang
