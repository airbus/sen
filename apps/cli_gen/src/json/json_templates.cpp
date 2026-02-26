// === json_templates.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "json/json_templates.h"

// generated code
#include "json_templates/alias_def.h"
#include "json_templates/class_def.h"
#include "json_templates/enum_def.h"
#include "json_templates/file_schema.h"
#include "json_templates/optional_def.h"
#include "json_templates/quantity_def.h"
#include "json_templates/sequence_def.h"
#include "json_templates/struct_def.h"
#include "json_templates/variant_def.h"

// inja
#include <inja/environment.hpp>

// sen
#include "sen/core/base/hash32.h"

JsonTemplateSet getJsonTypeTemplates(inja::Environment& env) noexcept
{
  JsonTemplateSet result;

  result.structType = env.parse(sen::decompressSymbolToString(struct_def, struct_defSize));
  result.enumType = env.parse(sen::decompressSymbolToString(enum_def, enum_defSize));
  result.variantType = env.parse(sen::decompressSymbolToString(variant_def, variant_defSize));
  result.sequenceType = env.parse(sen::decompressSymbolToString(sequence_def, sequence_defSize));
  result.aliasType = env.parse(sen::decompressSymbolToString(alias_def, alias_defSize));
  result.optionalType = env.parse(sen::decompressSymbolToString(optional_def, optional_defSize));
  result.quantityType = env.parse(sen::decompressSymbolToString(quantity_def, quantity_defSize));
  result.classType = env.parse(sen::decompressSymbolToString(class_def, class_defSize));
  result.schemaFile = env.parse(sen::decompressSymbolToString(file_schema, file_schemaSize));

  return result;
}
