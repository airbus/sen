// === plantuml.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_GEN_INCLUDE_SEN_GEN_PLANTUML_H
#define SEN_LIBS_GEN_INCLUDE_SEN_GEN_PLANTUML_H

#include "sen/core/base/compiler_macros.h"
#include "sen/core/lang/stl_resolver.h"

#include <memory>
#include <string>

namespace sen::gen
{

/// Selects which subset of the type set ends up in the rendered diagram. \ingroup gen
enum class PlantUMLGenerationMode
{
  all,
  onlyClasses,
  onlyBasicTypes
};

/// Controls whether enum types contribute their enumerators to the rendered diagram. \ingroup gen
enum class PlantUMLEnumMode
{
  all,
  noEnumerators
};

/// Renders PlantUML class diagrams out of sen type sets. Stateful (caches the loaded inja
/// templates), so prefer reusing one instance across calls when possible. \ingroup gen
class PlantUMLGenerator
{
  SEN_NOCOPY_NOMOVE(PlantUMLGenerator)

public:
  PlantUMLGenerator();
  ~PlantUMLGenerator();

  /// Renders one PlantUML document covering every type set in the supplied context. The two
  /// mode parameters select the subset of types to include and whether enumerators are
  /// listed inside enum nodes.
  [[nodiscard]] std::string generate(const sen::lang::TypeSetContext& typeSets,
                                     PlantUMLGenerationMode generationMode,
                                     PlantUMLEnumMode enumMode);

private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace sen::gen

#endif  // SEN_LIBS_GEN_INCLUDE_SEN_GEN_PLANTUML_H
