// === main_bar.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_EXPLORER_SRC_MAIN_BAR_H
#define SEN_COMPONENTS_EXPLORER_SRC_MAIN_BAR_H

// component
#include "bus_window.h"
#include "plotter.h"
#include "util.h"

// imgui
#include "imgui.h"

// sen
#include "sen/kernel/component.h"

// std
#include <filesystem>

class MainBar
{
  SEN_NOCOPY_NOMOVE(MainBar)

public:
  MainBar(sen::kernel::RunApi& api, EventExplorer* eventExplorer);
  ~MainBar() = default;

public:
  void update();
  void closeSource(const sen::kernel::BusAddress& address);
  static const char* getWindowName() noexcept;
  ImVec2 getWindowPos() const;
  ImVec2 getWindowSize() const;

private:
  void handleShortcuts();
  void drawModals();
  void mainWindow();
  void layoutsMenu();
  void scanLayoutFiles();
  bool loadLayout(const std::filesystem::path& path);
  bool saveLayout(const std::filesystem::path& path);
  void busWindows();
  void queryWindows();
  void updatePlots() const;
  void clearDeletedPlots();
  void openSource(const sen::kernel::BusAddress& address);
  void syncOpenSources();
  void closeSession(const std::string& sessionName);

  void queriesSection();
  void createQuery(std::string name, std::string selection);
  void openQuery(const std::string& name, const std::string& selection);
  void closeQuery(const std::string& name);

private:
  struct Query
  {
    std::string name;
    std::string selection;
  };

private:
  using StringList = std::vector<std::string>;
  using BusAddrList = std::vector<sen::kernel::BusAddress>;
  using QueryList = std::vector<Query>;

private:
  sen::kernel::RunApi& api_;
  EventExplorer* eventExplorer_;
  std::string newLayoutName_;
  std::filesystem::path activeLayoutPath_;
  std::string searchLayoutInput_;
  std::vector<std::filesystem::path> discoveredLayouts_;
  std::vector<std::filesystem::path> customLayouts_;
  float layoutScanTimer_ = 0.0f;
  bool openLoadPopup_ = false;
  bool openSaveAsPopup_ = false;
  sen::components::explorer::UIError layoutError_;
  std::vector<std::unique_ptr<Plotter>> plotters_;
  std::vector<Plotter*> plottersToDelete_;
  std::unordered_map<std::string, std::shared_ptr<sen::kernel::SessionInfoProvider>> openSessions_;
  std::unordered_map<std::string, std::unique_ptr<BusWindow>> busWindows_;
  StringList sessionsToClose_;
  StringList sessionsToOpen_;
  BusAddrList sourcesToClose_;
  BusAddrList sourcesToOpen_;
  Query newQueryInput_;
  StringList queriesToClose_;
  QueryList queriesToOpen_;
  QueryList savedQueries_;
  std::unordered_map<std::string, std::unique_ptr<BusWindow>> openQueries_;
  sen::components::explorer::UIError queryError_;
  ImVec2 windowPos_;
  ImVec2 windowSize_;
};

#endif  // SEN_COMPONENTS_EXPLORER_SRC_MAIN_BAR_H
