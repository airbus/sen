// === python.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_GEN_INCLUDE_SEN_GEN_PYTHON_H
#define SEN_LIBS_GEN_INCLUDE_SEN_GEN_PYTHON_H

#include "sen/core/base/compiler_macros.h"
#include "sen/core/lang/stl_resolver.h"

#include <memory>
#include <string>

namespace sen::gen
{

/// Renders Python dataclasses out of sen type sets. Stateful (caches the loaded inja
/// templates), so prefer reusing one instance across calls when possible. \ingroup gen
class PythonGenerator
{
  SEN_NOCOPY_NOMOVE(PythonGenerator)

public:
  PythonGenerator();
  ~PythonGenerator();

  /// Renders one Python module for the supplied type set. Imports are derived from the
  /// type set's `importedSets` and rendered as `from <module> import *` lines.
  [[nodiscard]] std::string generateModule(const sen::lang::TypeSet& typeSet);

  /// Sanitises a string so it is a valid Python identifier fragment by replacing the
  /// characters Python forbids in module/file names (`.`, `-`) with underscores.
  static void replaceInvalidCharacters(std::string& str);

private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace sen::gen

#endif  // SEN_LIBS_GEN_INCLUDE_SEN_GEN_PYTHON_H
