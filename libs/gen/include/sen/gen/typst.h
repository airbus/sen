// === typst.h =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_GEN_INCLUDE_SEN_GEN_TYPST_H
#define SEN_LIBS_GEN_INCLUDE_SEN_GEN_TYPST_H

#include "sen/core/base/compiler_macros.h"
#include "sen/core/lang/stl_resolver.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace sen::gen
{

/// What the document contains and how it is put together. Every field defaults to the
/// whole model and the standard sections, so a default-constructed value produces a
/// complete document and a caller who wants that need know none of this.
struct TypstOptions
{
  /// Shown on the title page and in the running header.
  std::string title {"Data model"};

  /// Packages to document. Empty means every package in the model.
  std::vector<std::string> includePackages;
  /// Packages to leave out, applied after `includePackages`.
  std::vector<std::string> excludePackages;

  /// The overview: the package table and the class hierarchy across all packages.
  bool overview {true};
  /// The class hierarchy within the overview. Emitted only where classes inherit.
  bool hierarchy {true};
  /// The per-section table of every type with its description and page.
  bool summaries {true};
  /// The alphabetical index of every type, at the back.
  bool index {true};
  /// The "used by" line on each type.
  bool usedBy {true};
  /// What the codes in the Flags column mean, before the first table that uses them.
  bool flagLegend {true};
  /// The table of the built-in types the model names, at the back.
  bool builtIns {true};

  /// Typst files the document includes at named points. The generator never reads them:
  /// it emits `#include` and the paths resolve when the document is compiled.
  std::filesystem::path frontMatter;
  std::filesystem::path beforeReference;
  std::filesystem::path afterReference;

  /// Replaces the emitted `style.typ`. Every visual decision lives in that module,
  /// so a caller changing the look never edits generated output.
  std::filesystem::path style;
};

/// Renders a Sen data model as a Typst document, in the shape of an interface control
/// document. Writes Typst source; producing a PDF is a separate step, as with PlantUML.
/// \ingroup gen
class TypstGenerator
{
  SEN_NOCOPY_NOMOVE(TypstGenerator)

public:
  TypstGenerator();
  ~TypstGenerator();

  /// Map of relative output path -> file contents: the document skeleton, the style
  /// module it imports, and the generated reference the skeleton includes.
  using FileContents = std::map<std::filesystem::path, std::string>;

  /// Renders the document. The three files are separate because they have different
  /// owners: the skeleton and the style are a starting point a caller may replace, and
  /// only the reference is regenerated when the model changes.
  [[nodiscard]] FileContents generate(const sen::lang::TypeSetContext& typeSets, const TypstOptions& options);

private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace sen::gen

#endif  // SEN_LIBS_GEN_INCLUDE_SEN_GEN_TYPST_H
