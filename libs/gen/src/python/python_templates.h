// === python_templates.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_GEN_SRC_PYTHON_PYTHON_TEMPLATES_H
#define SEN_LIBS_GEN_SRC_PYTHON_PYTHON_TEMPLATES_H

// inja
#include <inja/environment.hpp>
#include <inja/template.hpp>

namespace sen::gen::detail
{

/// A template for each supported user type.
struct PythonTemplateSet
{
  inja::Template structType;
  inja::Template enumType;
  inja::Template variantType;
  inja::Template optionalType;
  inja::Template classType;
  inja::Template sequenceType;
  inja::Template aliasType;
  inja::Template quantityType;
};

PythonTemplateSet makePythonTypeTemplates(inja::Environment& env);

}  // namespace sen::gen::detail

#endif  // SEN_LIBS_GEN_SRC_PYTHON_PYTHON_TEMPLATES_H
