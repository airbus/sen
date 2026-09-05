// === html.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_GEN_INCLUDE_SEN_GEN_HTML_H
#define SEN_LIBS_GEN_INCLUDE_SEN_GEN_HTML_H

#include "sen/core/base/compiler_macros.h"
#include "sen/core/lang/stl_resolver.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>

namespace sen::gen
{

/// Renders a browsable reference for a sen data model as a self-contained web page. The
/// output needs no server, no network and no host site: it opens from a file path and
/// carries its own navigation and search. \ingroup gen
class HtmlGenerator
{
  SEN_NOCOPY_NOMOVE(HtmlGenerator)

public:
  HtmlGenerator();
  ~HtmlGenerator();

  /// Map of relative output path -> rendered file contents: an `index.html` and the four
  /// files it names, which together are the application and the model it displays.
  using FileContents = std::map<std::filesystem::path, std::string>;

  /// Renders the application and the model it displays. `title` names the model on screen.
  /// The model is written as a script assigning a global rather than as JSON read at run
  /// time, because a fetch fails outright from a `file://` path.
  [[nodiscard]] FileContents generate(const sen::lang::TypeSetContext& typeSets, const std::string& title);

private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace sen::gen

#endif  // SEN_LIBS_GEN_INCLUDE_SEN_GEN_HTML_H
