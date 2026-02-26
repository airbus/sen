// === main_bar.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "main_bar.h"

// component
#include "event_explorer.h"
#include "icons_font.h"
#include "plotter.h"
#include "util.h"

// sen
#include "sen/core/base/u8string_util.h"
#include "sen/core/io/util.h"
#include "sen/core/obj/interest.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/configuration.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

// imgui
#include "imgui.h"
#include "imgui_stdlib.h"

// std
#include <algorithm>
#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

constexpr std::string_view iniExtension = ".ini";
constexpr std::string_view lastLayoutFilename = "last-layout.ini";

std::string addressToString(const sen::kernel::BusAddress& addr)
{
  std::string result;
  result.append(addr.sessionName);
  result.append(".");
  result.append(addr.busName);
  return result;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// MainBar
//--------------------------------------------------------------------------------------------------------------

MainBar::MainBar(sen::kernel::RunApi& api, EventExplorer* eventExplorer): api_(api), eventExplorer_(eventExplorer)
{
  if (const auto [layoutFile] = sen::toValue<sen::components::explorer::Configuration>(api_.getConfig());
      !layoutFile.empty())
  {
    std::filesystem::path initialPath(layoutFile);
    initialPath = initialPath.lexically_normal();
    activeLayoutPath_ = initialPath;
    customLayouts_.push_back(initialPath);
  }
}

void MainBar::update()
{
  handleShortcuts();
  mainWindow();
  drawModals();
  syncOpenSources();
  busWindows();
  queryWindows();
  updatePlots();
}

const char* MainBar::getWindowName() noexcept
{
  static const std::string windowName = sen::std_util::fromU8string(ICON_HEXAGON) + " Sessions";
  return windowName.c_str();
}

ImVec2 MainBar::getWindowPos() const { return windowPos_; }

ImVec2 MainBar::getWindowSize() const { return windowSize_; }

bool MainBar::loadLayout(const std::filesystem::path& path)
{
  if (std::error_code errorCode; std::filesystem::exists(path, errorCode))
  {
    activeLayoutPath_ = path;
    ImGui::LoadIniSettingsFromDisk(activeLayoutPath_.string().c_str());
    return true;
  }

  setUIError(layoutError_, "Failed to load layout: File does not exist");
  return false;
}

bool MainBar::saveLayout(const std::filesystem::path& path)
{
  if (path.has_parent_path())
  {
    std::error_code errorCode;
    std::filesystem::create_directories(path.parent_path(), errorCode);
    if (errorCode)
    {
      setUIError(layoutError_, "Failed to save layout: The directories or file could not be created");
      return false;
    }
  }

  activeLayoutPath_ = path;
  ImGui::SaveIniSettingsToDisk(activeLayoutPath_.string().c_str());
  return true;
}

void MainBar::scanLayoutFiles()
{
  discoveredLayouts_.clear();
  constexpr auto options = std::filesystem::directory_options::skip_permission_denied;

  for (const auto& entry: std::filesystem::recursive_directory_iterator(".", options))
  {
    if (entry.is_regular_file() && entry.path().extension() == iniExtension)
    {
      if (entry.path().filename().string() != lastLayoutFilename)
      {
        discoveredLayouts_.push_back(entry.path().lexically_normal());
      }
    }
  }
}

void MainBar::handleShortcuts()
{
  const auto& io = ImGui::GetIO();
  const bool ctrl = io.KeyCtrl;
  const bool shift = io.KeyShift;

  if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_O, false))
  {
    openLoadPopup_ = true;
  }

  if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_S, false))
  {
    if (!activeLayoutPath_.empty())
    {
      saveLayout(activeLayoutPath_);
    }
    else
    {
      openSaveAsPopup_ = true;
    }
  }

  if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_S, false))
  {
    openSaveAsPopup_ = true;
  }
}

void MainBar::drawModals()
{
  if (openSaveAsPopup_)
  {
    ImGui::OpenPopup("Save Layout As");
    openSaveAsPopup_ = false;
    clearUIError(layoutError_);
  }

  if (ImGui::BeginPopupModal("Save Layout As", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
  {
    if (ImGui::IsWindowAppearing())
    {
      ImGui::SetKeyboardFocusHere();
    }

    ImGui::InputTextWithHint("##SaveLayoutAsPath", "Full Path to Save or Name", &newLayoutName_);
    ImGui::SameLine();

    if (ImGui::Button("Save") || ImGui::IsKeyPressed(ImGuiKey_Enter))
    {
      if (!newLayoutName_.empty())
      {
        std::filesystem::path filePath(newLayoutName_);
        if (filePath.extension() != iniExtension)
        {
          filePath += iniExtension;
        }

        if (saveLayout(filePath))
        {
          if (std::find(customLayouts_.begin(), customLayouts_.end(), filePath) == customLayouts_.end())
          {
            customLayouts_.push_back(filePath);
          }
          ImGui::CloseCurrentPopup();
        }
        newLayoutName_.clear();
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
      newLayoutName_.clear();
      ImGui::CloseCurrentPopup();
    }

    drawMessageError(layoutError_);
    ImGui::EndPopup();
  }

  if (openLoadPopup_)
  {
    ImGui::OpenPopup("Load Layout");
    openLoadPopup_ = false;
    clearUIError(layoutError_);
  }

  if (ImGui::BeginPopupModal("Load Layout", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
  {
    if (ImGui::IsWindowAppearing())
    {
      ImGui::SetKeyboardFocusHere();
    }

    ImGui::InputTextWithHint("##LoadLayout", "Layout Path or Name", &searchLayoutInput_);
    ImGui::SameLine();

    if (ImGui::Button("Open") || ImGui::IsKeyPressed(ImGuiKey_Enter))
    {
      if (!searchLayoutInput_.empty())
      {
        std::error_code errorCode;
        if (std::filesystem::path inputPath(searchLayoutInput_); std::filesystem::exists(inputPath, errorCode))
        {
          if (inputPath.filename().string() == lastLayoutFilename || inputPath.extension() != iniExtension)
          {
            setUIError(layoutError_, "File is not supported");
          }
          else
          {
            inputPath = inputPath.lexically_normal();
            if (std::find(customLayouts_.begin(), customLayouts_.end(), inputPath) == customLayouts_.end())
            {
              customLayouts_.push_back(inputPath);
            }
            if (loadLayout(inputPath))
            {
              ImGui::CloseCurrentPopup();
            }
            searchLayoutInput_.clear();
          }
        }
        else
        {
          setUIError(layoutError_, "Path does not exist or is not valid");
        }
      }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
      searchLayoutInput_.clear();
      ImGui::CloseCurrentPopup();
    }

    drawMessageError(layoutError_);
    ImGui::EndPopup();
  }
}

void MainBar::mainWindow()
{
  if (ImGui::Begin(getWindowName()))
  {
    auto allSessionNames = api_.getSessionsDiscoverer().getDetectedSources();
    for (const auto& sessionName: allSessionNames)
    {
      auto sessionItr = openSessions_.find(sessionName);
      bool sessionIsOpen = sessionItr != openSessions_.end();

      ImGui::PushID(sessionName.c_str());
      if (ImGui::Button(sessionIsOpen ? sen::std_util::castToCharPtr(ICON_UNLINK)
                                      : sen::std_util::castToCharPtr(ICON_PLUG)))
      {
        if (sessionIsOpen)
        {
          sessionsToClose_.push_back(sessionName);
        }
        else
        {
          sessionsToOpen_.push_back(sessionName);
        }
      }
      ImGui::PopID();

      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      {
        ImGui::SetTooltip(sessionIsOpen ? "disconnect" : "connect");  // NOLINT(hicpp-vararg)
      }

      ImGui::SameLine();
      ImGui::TextUnformatted(sessionName.c_str());

      if (sessionIsOpen)
      {
        ImGui::Indent(10.0);
        auto buses = sessionItr->second->getDetectedSources();
        for (const auto& busName: buses)
        {
          sen::kernel::BusAddress addr;
          addr.sessionName = sessionName;
          addr.busName = busName;

          // bus should be open
          auto sourceItr = busWindows_.find(addressToString(addr));
          bool wasOpen = (sourceItr != busWindows_.end());
          bool shouldBeOpen = wasOpen;

          std::string busLabel = sen::std_util::fromU8string(ICON_SITEMAP) + " ";
          busLabel.append(addr.busName);
          busLabel.append("##");
          busLabel.append(addr.sessionName);

          if (ImGui::Checkbox(busLabel.c_str(), &shouldBeOpen))
          {
            if (shouldBeOpen && !wasOpen)
            {
              sourcesToOpen_.push_back(addr);  // bus is closed, open it
            }
            else if (!shouldBeOpen && wasOpen)
            {
              sourcesToClose_.push_back(addr);  // bus should be closed
            }
            else
            {
              // no code needed
            }
          }
        }
        ImGui::Indent(-10.0);
      }
      ImGui::Separator();
    }
  }

  queriesSection();

  windowPos_ = ImGui::GetWindowPos();
  windowSize_ = ImGui::GetWindowSize();
  ImGui::End();

  ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.4f, 0.4f, 0.4f, 0.6f));
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.5f, 0.5f, 0.5f, 0.6f));
  if (ImGui::BeginMainMenuBar())
  {
    layoutsMenu();

    if (ImGui::MenuItem("New Plot"))
    {
      std::string name = "Plot ";
      name.append(std::to_string(plotters_.size()));
      plotters_.push_back(std::make_unique<Plotter>(name));
    }
  }
  ImGui::PopStyleColor(2);

  ImGui::EndMainMenuBar();
}

void MainBar::layoutsMenu()
{
  if (ImGui::BeginMenu("Layouts"))
  {
    if (ImGui::BeginMenu("Load Layout"))
    {
      if (layoutScanTimer_ <= 0.0f)
      {
        scanLayoutFiles();
        layoutScanTimer_ = 5.0f;
      }
      else
      {
        layoutScanTimer_ -= ImGui::GetIO().DeltaTime;
      }

      bool found = false;
      std::error_code errorCode;

      for (const auto& filePath: discoveredLayouts_)
      {
        found = true;
        if (const bool isActive = (filePath == activeLayoutPath_);
            ImGui::MenuItem(filePath.filename().string().c_str(), nullptr, isActive))
        {
          loadLayout(filePath);
        }

        if (ImGui::IsItemHovered())
        {
          ImGui::SetTooltip("%s", filePath.string().c_str());  // NOLINT(hicpp-vararg)
        }
      }

      for (auto it = customLayouts_.begin(); it != customLayouts_.end();)
      {
        if (!std::filesystem::exists(*it, errorCode))
        {
          it = customLayouts_.erase(it);
          continue;
        }

        if (std::find(discoveredLayouts_.begin(), discoveredLayouts_.end(), *it) == discoveredLayouts_.end())
        {
          found = true;
          if (const bool isActive = (*it == activeLayoutPath_);
              ImGui::MenuItem(it->filename().string().c_str(), nullptr, isActive))
          {
            loadLayout(*it);
          }
          if (ImGui::IsItemHovered())
          {
            ImGui::SetTooltip("%s", it->string().c_str());  // NOLINT(hicpp-vararg)
          }
        }
        ++it;
      }

      if (!found)
      {
        ImGui::MenuItem("No layouts found", nullptr, false, false);
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Load Layout", "Ctrl+O"))
      {
        openLoadPopup_ = true;
      }
      ImGui::EndMenu();
    }
    else
    {
      layoutScanTimer_ = 0.0f;
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Save", "Ctrl+S"))
    {
      if (!activeLayoutPath_.empty())
      {
        saveLayout(activeLayoutPath_);
      }
      else
      {
        openSaveAsPopup_ = true;
      }
    }

    if (ImGui::MenuItem("Save Layout As", "Ctrl+Shift+S"))
    {
      openSaveAsPopup_ = true;
    }

    ImGui::EndMenu();
  }
}

void MainBar::syncOpenSources()
{
  for (const auto& sessionName: sessionsToClose_)
  {
    closeSession(sessionName);
  }
  sessionsToClose_.clear();

  for (const auto& elem: sourcesToClose_)
  {
    closeSource(elem);
  }
  sourcesToClose_.clear();

  for (const auto& name: queriesToClose_)
  {
    closeQuery(name);
  }
  queriesToClose_.clear();

  for (const auto& elem: sessionsToOpen_)
  {
    if (openSessions_.find(elem) == openSessions_.end())
    {
      openSessions_.try_emplace(elem, api_.getSessionsDiscoverer().makeSessionInfoProvider(elem));
    }
  }
  sessionsToOpen_.clear();

  for (const auto& elem: sourcesToOpen_)
  {
    openSource(elem);
  }
  sourcesToOpen_.clear();

  for (const auto& [name, selection]: queriesToOpen_)
  {
    openQuery(name, selection);
  }
  queriesToOpen_.clear();
}

void MainBar::closeSession(const std::string& sessionName)
{
  std::vector<sen::kernel::BusAddress> sourcesToClose;

  // close all the open buses
  for (const auto& [first, second]: busWindows_)
  {
    const auto& sourceAddress = second->getAddress();
    if (sourceAddress.sessionName == sessionName)
    {
      sourcesToClose.push_back(sourceAddress);
    }
  }

  for (const auto& elem: sourcesToClose)
  {
    closeSource(elem);
  }

  // close the session discoverer
  openSessions_.erase(sessionName);
}

void MainBar::queriesSection()
{
  if (ImGui::CollapsingHeader((sen::std_util::fromU8string(ICON_SITEMAP) + " Queries").c_str()))
  {
    ImGui::Indent(10.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5.0f, 5.0f));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::InputTextWithHint("##NameInput", "Name", &newQueryInput_.name);
    ImGui::InputTextWithHint("##QueryInput", "Query", &newQueryInput_.selection);
    ImGui::PopStyleColor();

    drawMessageError(queryError_);

    if (ImGui::Button("Create"))
    {
      createQuery(newQueryInput_.name, newQueryInput_.selection);
    }
    ImGui::PopStyleVar();

    ImGui::Separator();
    static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |  // NOLINT(hicpp-signed-bitwise)
                                   ImGuiTableFlags_Resizable |                        // NOLINT(hicpp-signed-bitwise)
                                   ImGuiTableFlags_Hideable;                          // NOLINT(hicpp-signed-bitwise)

    if (savedQueries_.begin() != savedQueries_.end() && ImGui::BeginTable("queries_table_main", 4, flags))
    {
      ImGui::TableSetupColumn("##active", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 20.0f);
      ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.3f);
      ImGui::TableSetupColumn("Selection", ImGuiTableColumnFlags_WidthStretch, 0.6f);
      ImGui::TableSetupColumn("##delete", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 35.0f);
      ImGui::TableHeadersRow();

      int id = 0;
      for (auto it = savedQueries_.begin(); it != savedQueries_.end();)
      {
        ImGui::PushID(id++);
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        bool isOpen = openQueries_.find(it->name) != openQueries_.end();
        if (ImGui::Checkbox("##check", &isOpen))
        {
          if (isOpen)
          {
            queriesToOpen_.push_back({it->name, it->selection});
          }
          else
          {
            queriesToClose_.push_back(it->name);
          }
        }

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(it->name.c_str());

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(it->selection.c_str());

        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
        if (ImGui::Button(" X "))
        {
          queriesToClose_.push_back(it->name);
          it = savedQueries_.erase(it);
        }
        else
        {
          if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
          {
            ImGui::SetTooltip("Delete query");  // NOLINT(hicpp-vararg)
          }
          ++it;
        }
        ImGui::PopStyleColor();
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
    ImGui::Indent(-10.0f);
  }
}

void MainBar::createQuery(std::string name, std::string selection)
{
  if (name.empty() && selection.empty())
  {
    setUIError(queryError_, "Neither the Name nor the Selection can be empty");
    return;
  }

  if (name.find_first_of(' ') != std::string::npos || name.find_first_of('.') != std::string::npos)
  {
    setUIError(queryError_, "Name cannot contain spaces or dots");
    return;
  }

  try
  {
    const auto interest = sen::Interest::make(selection, api_.getTypes());
    if (!interest->getBusCondition().has_value())
    {
      setUIError(queryError_, "Invalid source specification");
      return;
    }

    const auto& bus = interest->getBusCondition().value();

    std::string fullName = bus.sessionName;
    fullName.append(".").append(bus.busName).append(".").append(name);
    name = std::move(fullName);

    for (const auto& [existingName, existingSelection]: savedQueries_)
    {
      if (existingName == name)
      {
        setUIError(queryError_, "Repeated query name for this bus");
        return;
      }

      if (existingSelection == selection)
      {
        std::string err;
        err.append("A query named '");
        err.append(existingName);
        err.append("' already exists with the same definition");
        setUIError(queryError_, err);
        return;
      }
    }

    savedQueries_.push_back({std::move(name), std::move(selection)});
    newQueryInput_ = Query {};
    clearUIError(queryError_);
  }
  catch (const std::exception& error)
  {
    setUIError(queryError_, error.what());
  }
  catch (...)
  {
    setUIError(queryError_, "Unknown error while saving the query");
  }
}

void MainBar::openQuery(const std::string& name, const std::string& selection)
{
  if (openQueries_.find(name) == openQueries_.end())
  {
    openQueries_[name] = std::make_unique<BusWindow>(name, selection, api_, eventExplorer_);
  }
}

void MainBar::closeQuery(const std::string& name) { openQueries_.erase(name); }

void MainBar::clearDeletedPlots()
{
  while (!plottersToDelete_.empty())
  {
    const auto* elem = plottersToDelete_.back();
    plottersToDelete_.pop_back();

    for (auto itr = plotters_.begin(); itr != plotters_.end(); ++itr)
    {
      if ((*itr).get() == elem)
      {
        plotters_.erase(itr);
        break;
      }
    }
  }
}

void MainBar::busWindows()
{
  for (const auto& [first, second]: busWindows_)
  {
    if (!second->update())
    {
      sourcesToClose_.push_back(second->getAddress());
    }
  }
}

void MainBar::queryWindows()
{
  for (const auto& [name, window]: openQueries_)
  {
    if (!window->update())
    {
      queriesToClose_.push_back(name);
    }
  }
}

void MainBar::updatePlots() const
{
  for (const auto& plotter: plotters_)
  {
    plotter->update();
  }
}

void MainBar::openSource(const sen::kernel::BusAddress& address)
{
  busWindows_.try_emplace(addressToString(address), std::make_unique<BusWindow>(address, api_, eventExplorer_));
}

void MainBar::closeSource(const sen::kernel::BusAddress& address)
{
  auto itr = busWindows_.find(addressToString(address));
  if (itr != busWindows_.end())
  {
    busWindows_.erase(itr);
  }
}
