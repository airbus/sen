// === cpp.h ===========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_GEN_INCLUDE_SEN_GEN_CPP_H
#define SEN_LIBS_GEN_INCLUDE_SEN_GEN_CPP_H

#include "sen/core/base/compiler_macros.h"
#include "sen/core/lang/stl_resolver.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace sen::gen
{

/// Options controlling how `CppGenerator::generate` renders one type set. \ingroup gen
struct CppOptions
{
  /// When true, the generator also renders sources for every transitively imported type
  /// set found in `TypeSet::importedSets`.
  bool recursive = false;

  /// When true, generated classes use public C++ symbols (rather than the default
  /// translation-unit-private visibility).
  bool publicSymbols = false;

  /// Optional base path embedded into the rendered `#include` lines so generated headers
  /// resolve correctly under the consumer's include layout.
  std::string basePath;
};

/// Options for `CppGenerator::generateExports`. \ingroup gen
struct CppExportsOptions
{
  /// The Sen package name. Becomes the hashed package id embedded into the file.
  std::string packageName;

  /// Names of every STL file participating in the package.
  std::vector<std::string> stlFiles;

  /// Names of every package this one depends on. Each becomes a hashed package id in the
  /// rendered file's dependency block.
  std::vector<std::string> packageDependencies;

  /// Qualified names of types the user implements in this package. Each is materialised
  /// as a registration entry pointing at its `make` / `getType` symbols.
  std::vector<std::string> userImplementedClasses;
};

/// Renders C++ sources out of Sen type sets. Stateful (caches the loaded inja templates),
/// so prefer reusing one instance across calls when possible. \ingroup gen
class CppGenerator
{
  SEN_NOCOPY_NOMOVE(CppGenerator)

public:
  CppGenerator();
  ~CppGenerator();

  /// Map of relative output path -> rendered file contents. Keys are produced from the
  /// type set's `parentDirectory` joined with the per-file stem (one `.h`, one `.cpp` per
  /// type set rendered).
  using FileContents = std::map<std::filesystem::path, std::string>;

  /// Renders the types header + impl source for the supplied type set. When
  /// `opts.recursive` is true, transitively imported sets are rendered too and merged
  /// into the returned map. Recursion is cycle-safe via a visited-set keyed on
  /// `TypeSet*`. Two distinct type sets resolving to the same `parentDirectory /
  /// fileName` are deduplicated; first-written body wins.
  [[nodiscard]] FileContents generate(const sen::lang::TypeSet& typeSet, const CppOptions& opts = {});

  /// Renders the package exports source file (`sen_exported_types.cpp`).
  [[nodiscard]] std::string generateExports(const CppExportsOptions& opts);

private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace sen::gen

#endif  // SEN_LIBS_GEN_INCLUDE_SEN_GEN_CPP_H
