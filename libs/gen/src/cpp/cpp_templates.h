// === cpp_templates.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_GEN_SRC_CPP_CPP_TEMPLATES_H
#define SEN_LIBS_GEN_SRC_CPP_CPP_TEMPLATES_H

// inja
#include <inja/environment.hpp>
#include <inja/template.hpp>

namespace sen::gen::detail
{

/// A template for each supported user type.
struct CppTemplateSet
{
  inja::Template structType;
  inja::Template enumType;
  inja::Template variantType;
  inja::Template sequenceType;
  inja::Template aliasType;
  inja::Template optionalType;
  inja::Template quantityType;
  inja::Template classType;
};

/// Templates for the type header.
CppTemplateSet makeCppBaseHeaderTemplates(inja::Environment& env) noexcept;

/// Templates for the implementation source file.
CppTemplateSet makeCppImplTemplates(inja::Environment& env) noexcept;

/// The template for generating stl package exports
inja::Template makeCppPackageExportTemplate(inja::Environment& env);

/// Returns the template of the Base Header File
inja::Template makeCppBaseHeaderFileTemplate(inja::Environment& env);

/// Returns the template of the Implementation File
inja::Template makeCppImplFileTemplate(inja::Environment& env);

}  // namespace sen::gen::detail

#endif  // SEN_LIBS_GEN_SRC_CPP_CPP_TEMPLATES_H
