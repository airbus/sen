// === plantuml_templates.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "plantuml/plantuml_templates.h"

// generated code
#include "plantuml_templates/class_decl.h"
#include "plantuml_templates/enum_decl.h"
#include "plantuml_templates/file_decl.h"
#include "plantuml_templates/struct_decl.h"
#include "plantuml_templates/variant_decl.h"

// inja
#include <inja/environment.hpp>

// sen
#include "sen/core/base/hash32.h"

PlantUmlTemplates makePlantumlTemplates(inja::Environment& env)
{
  PlantUmlTemplates result;
  result.structTemplate = env.parse(sen::decompressSymbolToString(struct_decl, struct_declSize));
  result.enumTemplate = env.parse(sen::decompressSymbolToString(enum_decl, enum_declSize));
  result.variantTemplate = env.parse(sen::decompressSymbolToString(variant_decl, variant_declSize));
  result.classTemplate = env.parse(sen::decompressSymbolToString(class_decl, class_declSize));
  return result;
}
