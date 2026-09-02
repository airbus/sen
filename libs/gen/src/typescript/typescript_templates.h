// === typescript_templates.h ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_GEN_SRC_TYPESCRIPT_TEMPLATES_H
#define SEN_LIBS_GEN_SRC_TYPESCRIPT_TEMPLATES_H

// inja
#include <inja/environment.hpp>
#include <inja/template.hpp>

namespace sen::gen::detail
{

/// One template per supported user type, plus the file shell.
struct TypeScriptTemplateSet
{
  inja::Template structTemplate;
  inja::Template enumTemplate;
  inja::Template variantTemplate;
  inja::Template optionalTemplate;
  inja::Template classTemplate;
  inja::Template sequenceTemplate;
  inja::Template aliasTemplate;
  inja::Template quantityTemplate;
};

TypeScriptTemplateSet makeTypeScriptTypeTemplates(inja::Environment& env);

/// File-level template that wraps per-type rendered output with imports + header.
inja::Template makeTypeScriptFileTemplate(inja::Environment& env);

}  // namespace sen::gen::detail

#endif  // SEN_LIBS_GEN_SRC_TYPESCRIPT_TEMPLATES_H
