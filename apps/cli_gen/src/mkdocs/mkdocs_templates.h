// === mkdocs_templates.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_APPS_CLI_GEN_SRC_MKDOCS_TEMPLATES_H
#define SEN_APPS_CLI_GEN_SRC_MKDOCS_TEMPLATES_H

#include "mkdocs/classdoc_tree.h"

// sen
#include "sen/core/meta/type.h"

// inja
#include <inja/environment.hpp>
#include <inja/template.hpp>

// std
#include <string>
#include <vector>

/// A template for each supported user type.
struct MkDocsTemplates
{
  inja::Template aliasTemplate;
  inja::Template classTemplate;
  inja::Template classDiagram;
  inja::Template enumTemplate;
  inja::Template quantityTemplate;
  inja::Template sequenceTemplate;
  inja::Template structTemplate;
  inja::Template variantTemplate;
  inja::Template optionalTemplate;
};

struct TypeGroups
{
  std::vector<const sen::Type*> classTypes;
  std::vector<const sen::Type*> enumTypes;
  std::vector<const sen::Type*> structTypes;
  std::vector<const sen::Type*> sequenceTypes;
  std::vector<const sen::Type*> quantityTypes;
  std::vector<const sen::Type*> aliasTypes;
  std::vector<const sen::Type*> variantTypes;
  std::vector<const sen::Type*> optionalTypes;
};

struct TemplateVisitorResult
{
  std::string enumerationsDoc;
  std::string structuresDoc;
  std::string variantsDoc;
  std::string sequencesDoc;
  std::string quantitiesDoc;
  std::string aliasesDoc;
  std::string classesDoc;
  std::string optionalsDoc;

  std::vector<Node> rootClasses;
  TypeGroups groups;
};

/// Templates for the type header.
MkDocsTemplates makeMkDocsTemplates(inja::Environment& env);

#endif  // SEN_APPS_CLI_GEN_SRC_MKDOCS_TEMPLATES_H
