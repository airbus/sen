// === json_templates.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_APPS_CLI_GEN_SRC_JSON_JSON_TEMPLATES_H
#define SEN_APPS_CLI_GEN_SRC_JSON_JSON_TEMPLATES_H

// inja
#include <inja/environment.hpp>
#include <inja/template.hpp>

/// A template for each supported user type.
struct JsonTemplateSet
{
  inja::Template structType;
  inja::Template enumType;
  inja::Template variantType;
  inja::Template sequenceType;
  inja::Template aliasType;
  inja::Template optionalType;
  inja::Template quantityType;
  inja::Template classType;
  inja::Template schemaFile;
};

/// Templates for the type header.
JsonTemplateSet getJsonTypeTemplates(inja::Environment& env) noexcept;

#endif  // SEN_APPS_CLI_GEN_SRC_JSON_JSON_TEMPLATES_H
