// === signature_renderer.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "signature_renderer.h"

// local
#include "arg_form.h"
#include "styles.h"
#include "unicode.h"
#include "value_formatter.h"

// sen
#include "sen/core/meta/method.h"
#include "sen/core/meta/property.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// std
#include <string>

namespace sen::components::term
{

ftxui::Element renderMethodSignature(std::string_view objectName, const Method& method)
{
  ftxui::Elements lines;

  // Header: METHOD objectName.methodName [const]
  {
    ftxui::Elements header;
    header.push_back(ftxui::text("  METHOD ") | styles::sectionTitle());
    header.push_back(ftxui::text(std::string(objectName) + ".") | styles::descriptionText());
    header.push_back(ftxui::text(std::string(method.getName())) | ftxui::bold | styles::typeName());
    if (method.getConstness() == Constness::constant)
    {
      header.push_back(ftxui::text(" [const]") | styles::descriptionText());
    }
    lines.push_back(ftxui::hbox(std::move(header)));
  }

  // Description
  if (!method.getDescription().empty())
  {
    lines.push_back(ftxui::text(""));
    lines.push_back(ftxui::hbox({ftxui::text("    "), ftxui::paragraph(std::string(method.getDescription()))}));
  }

  // Arguments
  auto methodArgs = method.getArgs();
  if (!methodArgs.empty())
  {
    lines.push_back(ftxui::text(""));
    lines.push_back(ftxui::text("  ARGUMENTS") | styles::sectionTitle());

    for (const auto& arg: methodArgs)
    {
      ftxui::Elements argLine;
      argLine.push_back(ftxui::text("    "));
      argLine.push_back(ftxui::text(arg.name) | ftxui::bold);
      argLine.push_back(ftxui::text(" : "));
      argLine.push_back(ftxui::text(std::string(arg.type->getName())) | styles::typeName());
      // Prefer the @param-style description; fall back to the type's own description when absent.
      auto desc = effectiveDescription(arg.description, arg.type);
      if (!desc.empty())
      {
        argLine.push_back(ftxui::text(" " + std::string(unicode::middleDot) + " " + std::string(desc)) |
                          styles::descriptionText());
      }
      lines.push_back(ftxui::hbox(std::move(argLine)));
    }
  }

  // Return type
  auto retTypeName = std::string(method.getReturnType()->getName());
  if (retTypeName != "void")
  {
    lines.push_back(ftxui::text(""));
    lines.push_back(
      ftxui::hbox({ftxui::text("  RETURNS ") | styles::sectionTitle(), ftxui::text(retTypeName) | styles::typeName()}));
  }

  return ftxui::vbox(std::move(lines));
}

ftxui::Element renderPropertyValue(const Property& prop, const Var& value)
{
  // Empty/monostate values always render inline
  bool isEmpty = value.holds<std::monostate>();
  if (!isEmpty)
  {
    if (auto* m = value.getIf<VarMap>(); m != nullptr && m->empty())
    {
      isEmpty = true;
    }
    else if (auto* l = value.getIf<VarList>(); l != nullptr && l->empty())
    {
      isEmpty = true;
    }
  }

  bool isComplex =
    !isEmpty && (prop.getType()->isStructType() || prop.getType()->isSequenceType() || prop.getType()->isVariantType());

  auto formattedValue = formatValue(value, *prop.getType(), isComplex ? 1 : 0);

  if (isComplex)
  {
    return ftxui::vbox(
      {ftxui::hbox(
         {ftxui::text("  "), ftxui::text(std::string(prop.getName())) | styles::fieldName(), ftxui::text(":")}),
       ftxui::hbox({ftxui::text("  "), formattedValue})});
  }

  return ftxui::hbox({ftxui::text("  "),
                      ftxui::text(std::string(prop.getName())) | styles::fieldName(),
                      ftxui::text(": "),
                      formattedValue});
}

ftxui::Element renderMethodResult(const Var& value, const Type& returnType)
{
  if (returnType.isVoidType())
  {
    return ftxui::text("  OK") | styles::descriptionText();
  }

  auto formatted = formatValue(value, returnType);
  return ftxui::hbox({ftxui::text("  "), formatted});
}

ftxui::Element renderError(std::string_view title, std::string_view message)
{
  return ftxui::vbox(
    {ftxui::hbox(
       {ftxui::text("  Error: ") | styles::errorTitle(), ftxui::text(std::string(title)) | styles::errorTitle()}),
     ftxui::hbox({ftxui::text("    "), ftxui::paragraph(std::string(message)) | styles::errorMessage()})});
}

}  // namespace sen::components::term
