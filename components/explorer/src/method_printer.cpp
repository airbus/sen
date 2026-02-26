// === method_printer.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "method_printer.h"

// component
#include "editable_printer_maker.h"

// sen
#include "sen/core/meta/method.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/object.h"

// imgui
#include "imgui.h"

// std
#include <memory>
#include <string>
#include <tuple>
#include <utility>

MethodPrinter::MethodPrinter(std::shared_ptr<const sen::Method> method,
                             std::shared_ptr<sen::Object> owner,
                             sen::impl::WorkQueue* queue)
  : methodName_(method->getName()), method_(method), owner_(std::move(owner))
{
  const auto args = method->getArgs();
  for (const auto& arg: args)
  {
    sen::Var initialVar {};
    if (const auto variantType = arg.type->asVariantType(); variantType != nullptr)
    {
      if (!variantType->getFields().empty())
      {
        auto defaultKey = variantType->getFields()[0].key;
        initialVar = sen::Var(sen::KeyedVar(defaultKey, std::make_shared<sen::Var>()));
      }
    }
    auto tup =
      std::make_tuple(arg, initialVar, getOrCreateEditablePrinter(queue, &propertiesStateMap_, *arg.type), false);
    argDrawers_.emplace_back(std::move(tup));
  }
}

void MethodPrinter::draw()
{
  ImGui::PushID(this);
  methodLabel();
  methodArguments();
  executeButton();
  ImGui::PopID();
}

std::string MethodPrinter::getMethodName() const { return methodName_; }

void MethodPrinter::methodLabel() const
{
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::TableHeader(methodName_.c_str());
  ImGui::TableSetColumnIndex(1);
  ImGui::TableHeader("");
}

void MethodPrinter::methodArguments()
{
  ImGui::TableNextRow();
  for (auto& argDrawer: argDrawers_)
  {
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", std::get<0>(argDrawer).name.c_str());  // NOLINT(hicpp-vararg)
    ImGui::TableSetColumnIndex(1);
    sen::Var& var = std::get<1>(argDrawer);
    std::get<2>(argDrawer)(var, "", nullptr);

    bool isValid = !var.isEmpty();
    if (var.holds<std::string>() && var.get<std::string>().empty())
    {
      isValid = false;
    }

    std::get<3>(argDrawer) = isValid;

    ImGui::TableNextRow(ImGuiTableRowFlags_None);
  }
}

void MethodPrinter::executeButton() const
{
  ImGui::TableSetColumnIndex(1);
  ImGui::PushStyleColor(
    ImGuiCol_Border,
    ImU32(buttonStateLUT.at(areInputsValid() ? valid : invalid)));  // NOLINT(google-readability-casting)
  if (method_->getLocalOnly())
  {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button("Execute", ImVec2 {100, 25}) && areInputsValid())
  {
    sen::VarList args {};
    for (const auto& argDrawer: argDrawers_)
    {
      args.emplace_back(std::get<1>(argDrawer));
    }
    owner_->invokeUntyped(method_.get(), args);
  }
  if (method_->getLocalOnly())
  {
    ImGui::EndDisabled();
  }
  ImGui::PopStyleColor();
}

[[nodiscard]] bool MethodPrinter::areInputsValid() const
{
  for (auto& argDrawer: argDrawers_)
  {
    if (!std::get<3>(argDrawer))
    {
      return false;
    }
  }
  return true;
}
