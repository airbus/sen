// === mkdocs.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_GEN_INCLUDE_SEN_GEN_MKDOCS_H
#define SEN_LIBS_GEN_INCLUDE_SEN_GEN_MKDOCS_H

#include "sen/core/base/compiler_macros.h"
#include "sen/core/lang/stl_resolver.h"

#include <memory>
#include <string>

namespace sen::gen
{

/// Renders an MkDocs-compatible markdown document out of sen type sets. Stateful (caches the
/// loaded inja templates), so prefer reusing one instance across calls when possible. \ingroup gen
class MkDocsGenerator
{
  SEN_NOCOPY_NOMOVE(MkDocsGenerator)

public:
  MkDocsGenerator();
  ~MkDocsGenerator();

  /// Renders a single markdown document covering every type set in the supplied context.
  /// `title` becomes the document title used in the rendered file's heading.
  [[nodiscard]] std::string generate(const sen::lang::TypeSetContext& typeSets, const std::string& title);

private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace sen::gen

#endif  // SEN_LIBS_GEN_INCLUDE_SEN_GEN_MKDOCS_H
