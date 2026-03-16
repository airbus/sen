// === plantuml_templates.h ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_APPS_CLI_GEN_SRC_PLANTUML_TEMPLATES_H
#define SEN_APPS_CLI_GEN_SRC_PLANTUML_TEMPLATES_H

// inja
#include <inja/environment.hpp>
#include <inja/template.hpp>

/// A template for each supported user type.
struct PlantUmlTemplates
{
  inja::Template structTemplate;
  inja::Template enumTemplate;
  inja::Template variantTemplate;
  inja::Template classTemplate;
};

/// Templates for the type header.
PlantUmlTemplates makePlantumlTemplates(inja::Environment& env);

#endif  // SEN_APPS_CLI_GEN_SRC_PLANTUML_TEMPLATES_H
