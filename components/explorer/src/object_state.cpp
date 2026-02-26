// === object_state.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "object_state.h"

// component
#include "event_explorer.h"
#include "icons_font.h"
#include "plotter.h"
#include "property_data.h"
#include "value.h"

// sen
#include "sen/core/base/u8string_util.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/object.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// imgui
#include "imgui.h"

// std
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

void collectClassAndParents(sen::ConstTypeHandle<sen::ClassType> meta, sen::ClassList& list)
{
  list.push_back(meta);
  for (const auto& parent: meta->getParents())
  {
    collectClassAndParents(parent, list);
  }
}

[[nodiscard]] bool takeFirstSeparator(std::string& str)
{
  if (auto separator = str.find_first_of('.'); separator != std::string::npos && separator < str.size() - 1U)
  {
    str = str.substr(separator + 1U);
    return true;
  }
  return false;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// ObjectState
//--------------------------------------------------------------------------------------------------------------

std::shared_ptr<ObjectState> ObjectState::make(sen::kernel::RunApi& api,
                                               sen::Object* instance,
                                               sen::impl::WorkQueue* workQueue,
                                               EventExplorer* eventExplorer,
                                               PrinterRegistry& printerRegistry,
                                               ReporterRegistry& reporterRegistry)
{
  std::shared_ptr<ObjectState> result(
    new ObjectState(api, instance, workQueue, eventExplorer, printerRegistry, reporterRegistry));

  result->init();
  return result;
}

ObjectState::ObjectState(sen::kernel::RunApi& api,
                         sen::Object* instance,
                         sen::impl::WorkQueue* workQueue,
                         EventExplorer* eventExplorer,
                         PrinterRegistry& printerRegistry,
                         ReporterRegistry& reporterRegistry)
  : api_(api)
  , instance_(instance->shared_from_this())
  , workQueue_(workQueue)
  , eventExplorer_(eventExplorer)
  , printerRegistry_(printerRegistry)
  , reporterRegistry_(reporterRegistry)
{
  elementLabel_ = instance->getLocalName();
  elementLabel_.append(" [");
  elementLabel_.append(instance->getClass()->getQualifiedName());
  elementLabel_.append("]");

  // take the component name
  if (takeFirstSeparator(elementLabel_))
  {
    // take the session name
    if (takeFirstSeparator(elementLabel_))
    {
      // take the bus name
      std::ignore = takeFirstSeparator(elementLabel_);
    }
  }

  windowLabel_ = elementLabel_;
}

ObjectState::~ObjectState()
{
  eventExplorer_->stopTracking(instance_->getId());

  close();

  for (const auto& [first, second]: observers_)
  {
    for (auto* plot: second)
    {
      plot->deleteSelf();
    }
  }
}

void ObjectState::init()
{
  // collect all parents
  sen::ClassList classes;
  collectClassAndParents(instance_->getClass(), classes);

  // create a group for each parent class, start with the top one
  valueGroups_.reserve(classes.size());

  // save all events and store a flag to indicate if we are tracking it
  auto events = instance_->getClass()->getEvents(sen::ClassType::SearchMode::includeParents);
  for (const auto& ev: events)
  {
    allEvents_.emplace_back(ev, false);
  }

  for (auto itr = classes.rbegin(); itr != classes.rend(); ++itr)
  {
    const auto funcs = (*itr)->getMethods(sen::ClassType::SearchMode::doNotIncludeParents);
    const auto props = (*itr)->getProperties(sen::ClassType::SearchMode::doNotIncludeParents);

    // create the group
    valueGroups_.emplace_back();
    auto& group = valueGroups_.back();

    // set the group data
    group.name = (*itr)->getName();
    group.label = elementLabel_ + "." + group.name;

    // set the values
    group.values.reserve(props.size());
    group.methodPrinters.reserve(funcs.size());
    for (const auto& prop: props)
    {
      const auto propName = prop->getName();

      std::string valueLabel = elementLabel_;
      valueLabel.append(".");
      valueLabel.append(propName);

      auto initialValue = instance_->getPropertyUntyped(prop.get());

      auto propData =
        std::make_unique<PropertyData>(prop.get(), this, workQueue_, initialValue, printerRegistry_, reporterRegistry_);

      group.values.push_back(std::move(propData));

      auto* propDataPtr = group.values.back().get();

      // keep a shared pointer to us in the queue, to prevent deletion even if we keep
      // receiving messages for a while
      auto guard = instance_->onPropertyChangedUntyped(prop.get(),
                                                       {workQueue_,
                                                        [ourselves = shared_from_this(), propDataPtr](auto& /*arg*/)
                                                        {
                                                          if (!ourselves->drawingStopped_)
                                                          {
                                                            propDataPtr->changed(ourselves->observedData_);
                                                          }
                                                        }});

      connectionGuards_.push_back(std::move(guard));

      // trigger the change to refresh the initial value
      propDataPtr->changed(observedData_);
    }
    for (const auto& func: funcs)
    {
      group.methodPrinters.emplace_back(func, instance_, workQueue_);
    }
  }

  ownerInfo_ = api_.fetchOwnerInfo(instance_.get());
}

void ObjectState::close()
{
  // stop listening to property changes
  connectionGuards_.clear();
}

bool& ObjectState::open() { return checked_; }

sen::Object* ObjectState::getInstance() noexcept { return instance_.get(); }

const sen::kernel::ProcessInfo* ObjectState::getOwnerInfo() const noexcept { return ownerInfo_; }

void ObjectState::stopDrawing() { drawingStopped_ = true; }

void ObjectState::window()
{
  if (!checked_)
  {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(430, 450), ImGuiCond_FirstUseEver);  // NOLINT
  if (ImGui::Begin(windowLabel_.c_str(), &checked_))
  {
    ImGui::PushID(instance_.get());
    basicObjectInfo();
    events();
    properties();
    ImGui::PopID();
  }

  ImGui::End();
}

void ObjectState::basicObjectInfo() const
{
  if (ImGui::CollapsingHeader("Object", ImGuiTreeNodeFlags_DefaultOpen))
  {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

    ImGui::PushID(this);
    if (ImGui::BeginTable(elementLabel_.c_str(), 2, Value::tableFlags))
    {
      Value::setupColumns();

      Value::writeName("class");
      Value::printString(std::string(instance_->getClass()->getQualifiedName()));

      Value::writeName("id");
      Value::printObjectId(instance_->getId());

      Value::writeName(sen::std_util::fromU8string(ICON_TIME) + " time");
      Value::printTime(instance_->getLastCommitTime());

      ImGui::EndTable();
    }
    ImGui::PopID();
    ImGui::PopStyleVar();
  }
}

void ObjectState::events()
{
  if (!allEvents_.empty() &&
      ImGui::CollapsingHeader((sen::std_util::fromU8string(ICON_BOLT) + " Events").c_str(), ImGuiTreeNodeFlags_None))
  {
    for (auto& [first, second]: allEvents_)
    {
      ImGui::PushID(first->getId().get());  // NOLINT

      if (bool isChecked = second; ImGui::Checkbox(first->getName().data(), &isChecked))
      {
        if (isChecked != second)
        {
          if (isChecked)
          {
            eventExplorer_->startTracking(instance_.get(), first.get());
          }
          else
          {
            eventExplorer_->stopTracking(instance_->getId(), first.get());
          }
          second = isChecked;
        }
      }
      ImGui::PopID();
    }
  }
}

void ObjectState::properties()
{
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

  for (auto& group: valueGroups_)
  {
    if (ImGui::CollapsingHeader(group.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
    {
      ImGui::PushID(&group);
      if (ImGui::BeginTable(group.name.c_str(), 2, Value::tableFlags))
      {
        Value::setupColumns();

        for (const auto& propData: group.values)
        {
          ImGui::TableNextRow();
          propData->update();
        }
        ImGui::EndTable();
      }
      ImGui::Separator();
      if (ImGui::BeginTable("methods", 2, Value::tableFlags))
      {
        Value::setupColumns();

        for (auto& method: group.methodPrinters)
        {
          ImGui::TableNextRow();
          method.draw();
        }

        ImGui::EndTable();
      }
      ImGui::PopID();
    }
  }

  ImGui::PopStyleVar();
}

void ObjectState::startObservingElement(uint32_t elementHash, Plot* plot)
{
  observers_[elementHash].push_back(plot);

  auto itr = observedData_.find(elementHash);
  if (itr == observedData_.end())
  {
    ObservedDataMap map = {{elementHash, {plot}}};

    // fetch the last known status
    for (const auto& group: valueGroups_)
    {
      for (const auto& prop: group.values)
      {
        prop->changed(map);
      }
    }

    observedData_.insert({elementHash, {plot}});
  }
  else
  {
    itr->second.emplace_back(plot);
  }
}

void ObjectState::stopObservingElement(uint32_t elementHash, Plot* plot)
{
  auto& list = observers_[elementHash];
  auto itr = std::find(list.begin(), list.end(), plot);
  if (itr != list.end())
  {
    list.erase(itr);

    if (list.empty())
    {
      observers_.erase(elementHash);
      observedData_.erase(elementHash);
    }
  }
}
