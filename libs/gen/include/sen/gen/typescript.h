// === typescript.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_GEN_INCLUDE_SEN_GEN_TYPESCRIPT_H
#define SEN_LIBS_GEN_INCLUDE_SEN_GEN_TYPESCRIPT_H

#include "sen/core/base/compiler_macros.h"
#include "sen/core/lang/stl_resolver.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>

namespace sen::gen
{

/// Renders TypeScript modules out of sen type sets. Stateful (caches the loaded inja templates),
/// so prefer reusing one instance across calls when possible. \ingroup gen
class TypeScriptGenerator
{
  SEN_NOCOPY_NOMOVE(TypeScriptGenerator)

public:
  TypeScriptGenerator();
  ~TypeScriptGenerator();

  /// Map of relative output path -> rendered file contents. Keys are `<stem>.ts` per type set in
  /// the transitive closure of the input, plus an `index.ts` barrel that re-exports every per-
  /// input file.
  using FileContents = std::map<std::filesystem::path, std::string>;

  /// Renders one `.ts` file per type set transitively reachable from `typeSets`, plus the
  /// barrel. Cross-file imports are minimised to only the symbols each file actually references.
  [[nodiscard]] FileContents generate(const sen::lang::TypeSetContext& typeSets);

private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace sen::gen

#endif  // SEN_LIBS_GEN_INCLUDE_SEN_GEN_TYPESCRIPT_H
