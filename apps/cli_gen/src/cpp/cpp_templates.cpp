// === cpp_templates.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "cpp/cpp_templates.h"

#include "cpp_templates/alias_decl.h"
#include "cpp_templates/class_decl.h"
#include "cpp_templates/class_impl.h"
#include "cpp_templates/enum_decl.h"
#include "cpp_templates/enum_impl.h"
#include "cpp_templates/file_impl.h"
#include "cpp_templates/file_package_exports.h"
#include "cpp_templates/file_types.h"
#include "cpp_templates/optional_decl.h"
#include "cpp_templates/optional_impl.h"
#include "cpp_templates/quantity_decl.h"
#include "cpp_templates/quantity_impl.h"
#include "cpp_templates/sequence_decl.h"
#include "cpp_templates/sequence_impl.h"
#include "cpp_templates/struct_decl.h"
#include "cpp_templates/struct_impl.h"
#include "cpp_templates/variant_decl.h"
#include "cpp_templates/variant_impl.h"
#include "sen/core/base/hash32.h"

// inja
#include <inja/environment.hpp>

TemplateSet getBaseHeaderTemplates(inja::Environment& env) noexcept
{
  TemplateSet result;

  result.structType = env.parse(sen::decompressSymbolToString(struct_decl, struct_declSize));
  result.enumType = env.parse(sen::decompressSymbolToString(enum_decl, enum_declSize));
  result.variantType = env.parse(sen::decompressSymbolToString(variant_decl, variant_declSize));
  result.sequenceType = env.parse(sen::decompressSymbolToString(sequence_decl, sequence_declSize));
  result.aliasType = env.parse(sen::decompressSymbolToString(alias_decl, alias_declSize));
  result.optionalType = env.parse(sen::decompressSymbolToString(optional_decl, optional_declSize));
  result.quantityType = env.parse(sen::decompressSymbolToString(quantity_decl, quantity_declSize));
  result.classType = env.parse(sen::decompressSymbolToString(class_decl, class_declSize));

  return result;
}

TemplateSet getImplTemplates(inja::Environment& env) noexcept
{
  TemplateSet result;

  result.structType = env.parse(sen::decompressSymbolToString(struct_impl, struct_implSize));
  result.enumType = env.parse(sen::decompressSymbolToString(enum_impl, enum_implSize));
  result.variantType = env.parse(sen::decompressSymbolToString(variant_impl, variant_implSize));
  result.sequenceType = env.parse(sen::decompressSymbolToString(sequence_impl, sequence_implSize));
  result.aliasType = env.parse("");
  result.optionalType = env.parse(sen::decompressSymbolToString(optional_impl, optional_implSize));
  result.quantityType = env.parse(sen::decompressSymbolToString(quantity_impl, quantity_implSize));
  result.classType = env.parse(sen::decompressSymbolToString(class_impl, class_implSize));

  return result;
}

inja::Template getPackageExportTemplate(inja::Environment& env)
{
  static auto templateStr = sen::decompressSymbolToString(file_package_exports, file_package_exportsSize);
  return env.parse(templateStr);
}

inja::Template getBaseHeaderFileTemplate(inja::Environment& env)
{
  return env.parse(sen::decompressSymbolToString(file_types, file_typesSize));
}

inja::Template getImplFileTemplate(inja::Environment& env)
{
  return env.parse(sen::decompressSymbolToString(file_impl, file_implSize));
}
