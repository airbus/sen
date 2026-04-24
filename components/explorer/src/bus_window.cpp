// === bus_window.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "bus_window.h"

// component
#include "event_explorer.h"
#include "icons_font.h"
#include "object_state.h"
#include "value.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/hash32.h"
#include "sen/core/base/u8string_util.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_source.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// imgui
#include "imgui.h"

// std
#include <algorithm>
#include <cfloat>
#include <memory>
#include <tuple>
#include <utility>

using sen::std_util::fromU8string;

//--------------------------------------------------------------------------------------------------------------
// BusWindow::App
//--------------------------------------------------------------------------------------------------------------

BusWindow::App::App(sen::kernel::ProcessInfo ownerInfo): ownerInfo_(std::move(ownerInfo))
{
  label_ = fromU8string(ICON_CUBE) + " ";
  label_.append(ownerInfo_.appName);
}

BusWindow::App::~App()
{
  // prevent future updates from triggering draw operations
  for (const auto& elem: objectStates_)
  {
    elem->stopDrawing();
  }
}

void BusWindow::App::updateBarElements(const ImGuiTextFilter& filter) const
{
  if (ImGui::TreeNodeEx(label_.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
  {
    if (ownerInfo_.processId != 0 &&
        ImGui::TreeNodeEx((fromU8string(ICON_UNIF2D0) + " Process Info").c_str(), ImGuiTreeNodeFlags_SpanAvailWidth))
    {
      if (ImGui::BeginTable("", 2, Value::tableFlags))
      {
        Value::setupColumns();

        Value::writeName(fromU8string(ICON_DESKTOP) + " Host Name");
        Value::printString(ownerInfo_.hostName);

        Value::writeName(fromU8string(ICON_TERMINAL) + " Process ID");
        Value::printIntegral(ownerInfo_.processId);

        Value::writeName(fromU8string(ICON_CHEVRON_RIGHT) + " OS Name");
        Value::printString(ownerInfo_.osName);

        Value::writeName(fromU8string(ICON_UNIF2DB) + " CPU");
        Value::printEnum(std::string(sen::toString(ownerInfo_.cpuArch)));

        ImGui::EndTable();

        ImGui::TreePop();
      }
    }

    if (ImGui::TreeNodeEx((fromU8string(ICON_UL) + " Objects").c_str(),
                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
    {
      if (ImGui::BeginTable("", 2, Value::tableFlags))
      {
        Value::setupColumns();
        for (const auto& obj: objectStates_)
        {
          const auto& instanceName = obj->getInstance()->getName();
          if (filter.IsActive() && !filter.PassFilter(instanceName.c_str()))
          {
            continue;
          }

          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::AlignTextToFramePadding();
          ImGui::Checkbox(instanceName.c_str(), &obj->open());
          ImGui::TableSetColumnIndex(1);
          ImGui::SetNextItemWidth(-FLT_MIN);

          Value::printString(obj->getInstance()->getClass()->getQualifiedName().data());
        }

        ImGui::EndTable();
      }
      ImGui::TreePop();
    }

    ImGui::TreePop();
  }
}

void BusWindow::App::updateObjectWindow() const
{
  for (const auto& obj: objectStates_)
  {
    obj->window();
  }
}

void BusWindow::App::addState(std::shared_ptr<ObjectState> state) { objectStates_.emplace_back(std::move(state)); }

bool BusWindow::App::removeState(sen::ObjectId id)
{
  for (auto itr = objectStates_.begin(); itr != objectStates_.end(); ++itr)
  {
    if (itr->get()->getInstance()->getId() == id)
    {
      objectStates_.erase(itr);
      return true;
    }
  }
  return false;
}

const sen::kernel::ProcessInfo& BusWindow::App::getOwnerInfo() const noexcept { return ownerInfo_; }

bool BusWindow::App::hasObjects() const noexcept { return !objectStates_.empty(); }

//--------------------------------------------------------------------------------------------------------------
// BusWindow
//--------------------------------------------------------------------------------------------------------------

BusWindow::BusWindow(sen::kernel::BusAddress address, sen::kernel::RunApi& api, EventExplorer* eventExplorer)
  : address_(std::move(address)), api_(api), eventExplorer_(eventExplorer)
{
  windowLabel_ = fromU8string(ICON_SITEMAP) + " ";
  windowLabel_.append(address_.sessionName);
  windowLabel_.append(".");
  windowLabel_.append(address_.busName);

  windowId_ = sen::std_util::ignoredLossyConversion<int>(sen::crc32(windowLabel_));
  open();
}

BusWindow::BusWindow(const std::string& name,
                     std::string selection,
                     sen::kernel::RunApi& api,
                     EventExplorer* eventExplorer)
  : querySelection_(std::move(selection)), api_(api), eventExplorer_(eventExplorer)
{
  windowLabel_ = fromU8string(ICON_SITEMAP) + " ";
  windowLabel_.append(name);

  windowId_ = sen::std_util::ignoredLossyConversion<int>(sen::crc32(windowLabel_));
  open();
}

BusWindow::~BusWindow()
{
  for (const auto& obj: objects_->list.getUntypedObjects())
  {
    eventExplorer_->stopTracking(obj->getId());
  }
  apps_.clear();
}

bool BusWindow::update()
{
  if (!checked_)
  {
    return false;
  }

  // our window
  {
    ImGui::SetNextWindowSize(ImVec2(430, 450), ImGuiCond_FirstUseEver);  // NOLINT

    ImGui::PushID(windowId_);
    if (ImGui::Begin(windowLabel_.c_str(), &checked_))
    {
      textFilter_.Draw((fromU8string(ICON_FILTER) + " Filter").c_str(), 300);  // NOLINT
      ImGui::Separator();

      for (const auto& app: apps_)
      {
        app->updateBarElements(textFilter_);
      }
    }
    ImGui::End();
    ImGui::PopID();
  }

  // object windows
  for (const auto& app: apps_)
  {
    app->updateObjectWindow();
  }

  return checked_;
}

const sen::kernel::BusAddress& BusWindow::getAddress() const noexcept { return address_; }

void BusWindow::open()
{
  if (objects_)
  {
    return;
  }

  std::shared_ptr<sen::Interest> interest;
  std::shared_ptr<sen::ObjectSource> source;

  if (querySelection_.empty())
  {
    objects_ = api_.selectAllFrom<sen::Object>(address_);
  }
  else
  {
    try
    {
      interest = sen::Interest::make(querySelection_, api_.getTypes());

      if (const auto& busCondition = interest->getBusCondition())
      {
        address_.sessionName = busCondition->sessionName;
        address_.busName = busCondition->busName;
      }
    }
    catch (...)
    {
      return;
    }

    source = api_.getSource(address_.sessionName + "." + address_.busName);

    if (!source)
    {
      return;
    }

    objects_ = std::make_shared<sen::Subscription<sen::Object>>();
  }

  SEN_ASSERT(objects_ != nullptr);

  auto* queue = api_.getWorkQueue();

  std::ignore = objects_->list.onAdded(
    [this, queue](const auto& addedObjects)
    {
      for (auto obj: addedObjects)
      {
        auto state = ObjectState::make(api_, obj, queue, eventExplorer_, printerRegistry_, reporterRegistry_);

        sen::kernel::ProcessInfo ownerInfo {};
        if (auto objOwnerInfo = state->getOwnerInfo(); objOwnerInfo != nullptr)
        {
          ownerInfo = *objOwnerInfo;
        }
        else
        {
          ownerInfo.appName = "explorer";
          ownerInfo.hostName = "local";
          ownerInfo.processId = 0;
        }

        bool appFound = false;
        for (const auto& app: apps_)
        {
          const auto& appOwner = app->getOwnerInfo();
          if (appOwner.appName == ownerInfo.appName && appOwner.hostName == ownerInfo.hostName &&
              appOwner.processId == ownerInfo.processId)
          {
            app->addState(std::move(state));
            appFound = true;
            break;
          }
        }

        if (!appFound)
        {
          // app not found, let's create one
          apps_.push_back(std::make_unique<App>(ownerInfo));
          apps_.back()->addState(std::move(state));
        }
      }
    });

  std::ignore = objects_->list.onRemoved(
    [this](const auto& removedObjects)
    {
      for (auto obj: removedObjects)
      {
        auto objectId = obj->getId();
        for (const auto& app: apps_)
        {
          if (app->removeState(objectId))
          {
            break;
          }
        }
      }

      apps_.erase(
        std::remove_if(apps_.begin(), apps_.end(), [](const auto& app) -> bool { return !app->hasObjects(); }),
        apps_.end());
    });

  if (interest)
  {
    objects_->attachTo(std::move(source), std::move(interest), true);
  }
}
