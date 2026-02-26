// === cpp_templates.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_APPS_CLI_GEN_SRC_CPP_CPP_TEMPLATES_H
#define SEN_APPS_CLI_GEN_SRC_CPP_CPP_TEMPLATES_H

// inja
#include <inja/environment.hpp>
#include <inja/template.hpp>

/// A template for each supported user type.
struct TemplateSet
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
TemplateSet getBaseHeaderTemplates(inja::Environment& env) noexcept;

/// Templates for the implementation source file.
TemplateSet getImplTemplates(inja::Environment& env) noexcept;

/// The template for generating stl package exports
inja::Template getPackageExportTemplate(inja::Environment& env);

/// Returns the template of the Base Header File
inja::Template getBaseHeaderFileTemplate(inja::Environment& env);

/// Returns the template of the Implementation File
inja::Template getImplFileTemplate(inja::Environment& env);

/// Returns the template of the Traits File
inja::Template getTraitsFileTemplate(inja::Environment& env);

#endif  // SEN_APPS_CLI_GEN_SRC_CPP_CPP_TEMPLATES_H
