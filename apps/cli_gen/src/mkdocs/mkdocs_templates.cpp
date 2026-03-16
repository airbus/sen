// === mkdocs_templates.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "mkdocs/mkdocs_templates.h"

// generated code
#include "mkdocs_templates/alias_doc.h"
#include "mkdocs_templates/class_diagram.h"
#include "mkdocs_templates/class_doc.h"
#include "mkdocs_templates/enum_doc.h"
#include "mkdocs_templates/file_doc.h"
#include "mkdocs_templates/optional_doc.h"
#include "mkdocs_templates/quantity_doc.h"
#include "mkdocs_templates/sequence_doc.h"
#include "mkdocs_templates/struct_doc.h"
#include "mkdocs_templates/variant_doc.h"

// inja
#include <inja/environment.hpp>

// sen
#include "sen/core/base/hash32.h"

MkDocsTemplates makeMkDocsTemplates(inja::Environment& env)
{
  MkDocsTemplates result;
  result.aliasTemplate = env.parse(sen::decompressSymbolToString(alias_doc, alias_docSize));
  result.classTemplate = env.parse(sen::decompressSymbolToString(class_doc, class_docSize));
  result.classDiagram = env.parse(sen::decompressSymbolToString(class_diagram, class_diagramSize));
  result.enumTemplate = env.parse(sen::decompressSymbolToString(enum_doc, enum_docSize));
  result.quantityTemplate = env.parse(sen::decompressSymbolToString(quantity_doc, quantity_docSize));
  result.sequenceTemplate = env.parse(sen::decompressSymbolToString(sequence_doc, sequence_docSize));
  result.structTemplate = env.parse(sen::decompressSymbolToString(struct_doc, struct_docSize));
  result.variantTemplate = env.parse(sen::decompressSymbolToString(variant_doc, variant_docSize));
  result.optionalTemplate = env.parse(sen::decompressSymbolToString(optional_doc, optional_docSize));
  return result;
}
