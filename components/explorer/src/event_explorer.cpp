// === event_explorer.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "event_explorer.h"

// component
#include "icons_font.h"
#include "object_state.h"
#include "value.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/u8string_util.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/var.h"
#include "sen/core/meta/variant_type.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/object.h"

// imgui
#include "imgui.h"

// std
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>
#include <utility>

using sen::std_util::fromU8string;

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

namespace
{

constexpr std::size_t maxHistorySize = 64U;

constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable |       // NOLINT
                                       ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Hideable |  // NOLINT
                                       ImGuiTableFlags_NoHostExtendY | ImGuiTableFlags_ScrollY;     // NOLINT

class StringPrinter final: public sen::TypeVisitor
{
  SEN_NOCOPY_NOMOVE(StringPrinter)

public:
  StringPrinter(const sen::Var& value, std::string& resultStr, std::size_t indent)
    : value_(value), resultStr_(resultStr), indent_(indent)
  {
  }

  ~StringPrinter() override = default;

public:
  void apply(const sen::EnumType& type) override
  {
    if (value_.holds<std::string>())
    {
      resultStr_.append(value_.get<std::string>());
    }
    else
    {
      resultStr_.append(type.getEnumFromKey(value_.getCopyAs<uint32_t>())->name);
    }
  }

  void apply(const sen::BoolType& type) override
  {
    std::ignore = type;
    resultStr_.append(value_.get<bool>() ? "true" : "false");
  }

  void apply(const sen::UInt8Type& type) override
  {
    std::ignore = type;
    resultStr_.append(value_.getCopyAs<std::string>());
  }

  void apply(const sen::Int16Type& type) override
  {
    std::ignore = type;
    resultStr_.append(value_.getCopyAs<std::string>());
  }

  void apply(const sen::UInt16Type& type) override
  {
    std::ignore = type;
    resultStr_.append(value_.getCopyAs<std::string>());
  }

  void apply(const sen::Int32Type& type) override
  {
    std::ignore = type;
    resultStr_.append(value_.getCopyAs<std::string>());
  }

  void apply(const sen::UInt32Type& type) override
  {
    std::ignore = type;
    resultStr_.append(value_.getCopyAs<std::string>());
  }

  void apply(const sen::Int64Type& type) override
  {
    std::ignore = type;
    resultStr_.append(value_.getCopyAs<std::string>());
  }

  void apply(const sen::UInt64Type& type) override
  {
    std::ignore = type;
    resultStr_.append(value_.getCopyAs<std::string>());
  }

  void apply(const sen::Float32Type& type) override
  {
    std::ignore = type;
    resultStr_.append(value_.getCopyAs<std::string>());
  }

  void apply(const sen::Float64Type& type) override
  {
    std::ignore = type;
    resultStr_.append(value_.getCopyAs<std::string>());
  }

  void apply(const sen::TimestampType& type) override
  {
    std::ignore = type;
    resultStr_.append(value_.getCopyAs<std::string>());
  }

  void apply(const sen::StringType& type) override
  {
    std::ignore = type;
    resultStr_.append(value_.get<std::string>());
  }

  void apply(const sen::StructType& type) override
  {
    const auto indentStr = std::string(indent_, ' ');

    if (const auto* map = value_.getIf<sen::VarMap>(); map)
    {
      resultStr_.append("\n");

      for (const auto& [name, var]: *map)
      {
        resultStr_.append(indentStr);
        resultStr_.append(name);
        resultStr_.append(": ");

        StringPrinter printer(var, resultStr_, indent_ + 2U);
        type.getFieldFromName(name)->type->accept(printer);

        resultStr_.append("\n");
      }
    }
    else
    {
      resultStr_.append("<empty>");
    }
  }

  void apply(const sen::VariantType& type) override
  {
    if (value_.holds<sen::KeyedVar>())
    {
      const auto& [key, var] = value_.get<sen::KeyedVar>();

      auto fieldVar = *var;
      StringPrinter printer(fieldVar, resultStr_, indent_);
      type.getFieldFromKey(key)->type->accept(printer);
    }
    else
    {
      std::string err;
      err.append("variant does not hold a KeyedVar ");
      err.append("of Variant '");
      err.append(type.getQualifiedName());
      err.append("'");
      sen::throwRuntimeError(err);
    }
  }

  void apply(const sen::SequenceType& type) override
  {
    const auto indentStr = std::string(indent_, ' ');

    if (const auto* list = value_.getIf<sen::VarList>(); list != nullptr)
    {
      resultStr_.append("\n");

      for (const auto& elem: *list)
      {
        resultStr_.append(indentStr);
        resultStr_.append("- ");

        StringPrinter printer(elem, resultStr_, indent_ + 4U);
        type.getElementType()->accept(printer);
        resultStr_.append("\n");
      }
    }
    else
    {
      resultStr_.append("<empty>");
    }
  }

  void apply(const sen::QuantityType& type) override
  {
    resultStr_.append(value_.getCopyAs<std::string>());
    resultStr_.append(" ");
    if (type.getUnit())
    {
      resultStr_.append(std::string(type.getUnit(sen::Unit::ensurePresent).getAbbreviation()));
    }
  }

  void apply(const sen::AliasType& type) override { type.getAliasedType()->accept(*this); }

  void apply(const sen::OptionalType& type) override
  {
    if (value_.isEmpty())
    {
      resultStr_.append("<empty>");
    }
    else
    {
      type.getType()->accept(*this);
    }
  }

private:
  const sen::Var& value_;
  std::string& resultStr_;
  std::size_t indent_;
};

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// EventExplorer
//--------------------------------------------------------------------------------------------------------------

EventExplorer::EventExplorer(sen::impl::WorkQueue* workQueue): workQueue_(workQueue), history_(maxHistorySize) {}

void EventExplorer::startTracking(sen::Object* emitter, const sen::Event* event)
{
  for (const auto& track: tracks_)
  {
    if (track.event == event && track.objectId == emitter->getId())
    {
      return;  // already present
    }
  }

  const auto emitterId = emitter->getId();
  std::string emitterName = emitter->getLocalName();
  auto emitterClass = emitter->getClass();

  auto guard = emitter->onEventUntyped(
    event,
    {workQueue_,
     [this, event, emitterId, emitterName, emitterClass](const sen::EventInfo& info, const sen::VarList& args) mutable
     { registerEvent(emitterId, emitterName, emitterClass.type(), info, event, args); }});

  tracks_.push_back({event, emitterId, std::move(guard)});
}

void EventExplorer::stopTracking(sen::ObjectId emitter, const sen::Event* event)
{
  tracks_.erase(
    std::remove_if(tracks_.begin(),
                   tracks_.end(),
                   [&](const auto& elem) -> bool { return elem.objectId == emitter && elem.event == event; }),
    tracks_.end());
}

void EventExplorer::stopTracking(sen::ObjectId emitter)
{
  tracks_.erase(
    std::remove_if(tracks_.begin(), tracks_.end(), [&](const auto& elem) -> bool { return elem.objectId == emitter; }),
    tracks_.end());
}

const char* EventExplorer::getWindowName() noexcept
{
  static const std::string windowName = fromU8string(ICON_BOLT) + " Events";
  return windowName.c_str();
}

void EventExplorer::update()
{
  ImGui::Begin(getWindowName());

  textFilter_.Draw((fromU8string(ICON_FILTER) + " Filter").c_str(), 300);  // NOLINT
  ImGui::SameLine(0.0f, 100.0);                                            // NOLINT
  ImGui::Checkbox("Auto scroll", &autoScroll_);
  ImGui::SameLine();
  if (ImGui::Button("Clear"))
  {
    clearHistory();
  }

  ImGui::Separator();
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

  if (ImGui::BeginTable("events", 5, tableFlags))  // NOLINT
  {
    ImGui::TableSetupColumn("time");
    ImGui::TableSetupColumn("emitter");
    ImGui::TableSetupColumn("class");
    ImGui::TableSetupColumn("event", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("params", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupScrollFreeze(0, 1);

    ImGui::TableHeadersRow();

    if (textFilter_.IsActive())
    {
      for (std::size_t i = 0; i < history_.size(); ++i)
      {
        const auto& entry = history_.at(i);

        if (textFilter_.PassFilter(entry.text.c_str()))
        {
          draw(entry);
        }
      }
    }
    else
    {
      ImGuiListClipper clipper;
      clipper.Begin(static_cast<int>(history_.size()));
      while (clipper.Step())
      {
        for (auto entry = clipper.DisplayStart; entry < clipper.DisplayEnd; entry++)
        {
          draw(history_.at(entry));
        }
      }
      clipper.End();
    }

    if (autoScroll_)
    {
      ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndTable();
  }

  ImGui::PopStyleVar();

  ImGui::End();
}

void EventExplorer::clearHistory() { history_.clear(); }

void EventExplorer::registerEvent(sen::ObjectId emitterId,
                                  const std::string& emitterName,
                                  const sen::ClassType* emitterClass,
                                  const sen::EventInfo& info,
                                  const sen::Event* event,
                                  const sen::VarList& args)
{
  std::string text;
  text.append(info.creationTime.toLocalString());
  text.append(" ");
  text.append(emitterName);
  text.append(" ");
  text.append(emitterClass->getQualifiedName());
  text.append(" ");
  text.append(event->getName());

  std::string valueText;
  valueText.append("\n");
  for (std::size_t i = 0U; i < args.size(); ++i)
  {
    valueText.append("  ");
    valueText.append(event->getArgs()[i].name);
    valueText.append(": ");

    StringPrinter printer(args[i], valueText, 4U);
    event->getArgs()[i].type->accept(printer);

    valueText.append("\n");
  }

  history_.push({emitterId, emitterName, emitterClass, event, info, args, std::move(text), std::move(valueText)});
}

void EventExplorer::draw(const EventLogEntry& entry) const
{
  ImGui::TableNextRow();

  ImGui::TableSetColumnIndex(0);
  Value::printTime(entry.eventInfo.creationTime);

  ImGui::TableSetColumnIndex(1);
  ImGui::TextUnformatted(entry.emitterName.data());

  ImGui::TableSetColumnIndex(2);
  ImGui::TextUnformatted(entry.emitterClass->getQualifiedName().data());

  ImGui::TableSetColumnIndex(3);
  ImGui::TextUnformatted(entry.ev->getName().data());

  ImGui::TableSetColumnIndex(4);

  if (ImGui::BeginTable("## table", 2, Value::tableFlags))
  {
    Value::setupColumns();

    ImGui::EndTable();
  }

  ImGui::TextWrapped("%s", entry.valueText.c_str());  // NOLINT
}
