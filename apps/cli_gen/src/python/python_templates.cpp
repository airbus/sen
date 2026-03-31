// === python_templates.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "python/python_templates.h"

// generated code
#include "python_templates/alias_decl.h"
#include "python_templates/class_decl.h"
#include "python_templates/enum_decl.h"
#include "python_templates/file_decl.h"
#include "python_templates/optional_decl.h"
#include "python_templates/quantity_decl.h"
#include "python_templates/sequence_decl.h"
#include "python_templates/struct_decl.h"
#include "python_templates/variant_decl.h"

// inja
#include <inja/environment.hpp>

// sen
#include "sen/core/base/hash32.h"

PythonTemplates makePythonTemplates(inja::Environment& env)
{
  PythonTemplates result;
  result.structTemplate = env.parse(sen::decompressSymbolToString(struct_decl, struct_declSize));
  result.enumTemplate = env.parse(sen::decompressSymbolToString(enum_decl, enum_declSize));
  result.variantTemplate = env.parse(sen::decompressSymbolToString(variant_decl, variant_declSize));
  result.optionalTemplate = env.parse(sen::decompressSymbolToString(optional_decl, optional_declSize));
  result.classTemplate = env.parse(sen::decompressSymbolToString(class_decl, class_declSize));
  result.sequenceTemplate = env.parse(sen::decompressSymbolToString(sequence_decl, sequence_declSize));
  result.aliasTemplate = env.parse(sen::decompressSymbolToString(alias_decl, alias_declSize));
  result.quantityTemplate = env.parse(sen::decompressSymbolToString(quantity_decl, quantity_declSize));
  return result;
}
