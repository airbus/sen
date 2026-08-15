// === json.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_GEN_INCLUDE_SEN_GEN_JSON_H
#define SEN_LIBS_GEN_INCLUDE_SEN_GEN_JSON_H

#include "sen/core/base/compiler_macros.h"
#include "sen/core/lang/stl_resolver.h"

#include <memory>
#include <string>
#include <vector>

namespace sen::gen
{

/// Options for `JsonGenerator::generatePackage`. \ingroup gen
struct PackageOptions
{
  /// Becomes the `schemaName` field in the rendered JSON. Caller is expected to derive it
  /// from the desired output file name (typically the file stem without extension).
  std::string schemaName;

  /// Qualified names of classes implemented by the user. Each is materialised as a
  /// concrete class deriving from the matching abstract class in the type set.
  std::vector<std::string> classes;
};

/// Options for `JsonGenerator::generateComponent`. \ingroup gen
struct ComponentOptions
{
  /// Becomes the `schemaName` field in the rendered JSON.
  std::string schemaName;

  /// Becomes the `component` field in the rendered JSON.
  std::string componentName;
};

/// Renders JSON-Schema descriptions of sen type sets. Stateful (caches the loaded inja
/// templates), so prefer reusing one instance across calls when possible. \ingroup gen
class JsonGenerator
{
  SEN_NOCOPY_NOMOVE(JsonGenerator)

public:
  JsonGenerator();
  ~JsonGenerator();

  /// Renders a package schema for the supplied type set.
  [[nodiscard]] std::string generatePackage(const sen::lang::TypeSetContext& typeSets, const PackageOptions& opts = {});

  /// Renders a component schema (i.e. one that exposes a Configuration class) for the
  /// supplied type set.
  [[nodiscard]] std::string generateComponent(const sen::lang::TypeSetContext& typeSets,
                                              const ComponentOptions& opts = {});

  /// Renders a kernel configuration file that combines an arbitrary number of previously
  /// rendered schemas into one document. `schemaContents` are the raw rendered strings
  /// (as produced by `generatePackage` / `generateComponent` or read from disk).
  [[nodiscard]] std::string combineSchemas(const std::vector<std::string>& schemaContents,
                                           const std::string& schemaName);

  /// Renders one type's JSON-Schema fragment in the same shape the file-level renderers use
  /// (`"<qualifiedName>": { "$id": "...", ... }`). Cross-type references are emitted as
  /// `$ref` strings keyed by qualified name; the caller resolves them.
  [[nodiscard]] std::string renderTypeSchema(const sen::CustomType& type);

private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace sen::gen

#endif  // SEN_LIBS_GEN_INCLUDE_SEN_GEN_JSON_H
