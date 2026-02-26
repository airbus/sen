// === bus_window.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_EXPLORER_SRC_BUS_WINDOW_H
#define SEN_COMPONENTS_EXPLORER_SRC_BUS_WINDOW_H

#include "event_explorer.h"
#include "object_state.h"

// imgui
#include "imgui.h"

// std
#include <string>

class MainBar;

class BusWindow
{
  SEN_NOCOPY_NOMOVE(BusWindow)

public:
  BusWindow(sen::kernel::BusAddress address, sen::kernel::RunApi& api, EventExplorer* eventExplorer);
  BusWindow(const std::string& name, std::string selection, sen::kernel::RunApi& api, EventExplorer* eventExplorer);

  ~BusWindow();

public:
  bool update();
  const sen::kernel::BusAddress& getAddress() const noexcept;

private:
  class App
  {
    SEN_MOVE_ONLY(App)

  public:
    explicit App(sen::kernel::ProcessInfo ownerInfo);
    ~App();

  public:
    void updateBarElements(const ImGuiTextFilter& filter) const;
    void updateObjectWindow() const;
    void addState(std::shared_ptr<ObjectState> state);
    [[nodiscard]] bool removeState(sen::ObjectId id);
    [[nodiscard]] const sen::kernel::ProcessInfo& getOwnerInfo() const noexcept;
    [[nodiscard]] bool hasObjects() const noexcept;

  private:
    sen::kernel::ProcessInfo ownerInfo_;
    std::vector<std::shared_ptr<ObjectState>> objectStates_;
    std::string label_;
  };

private:
  void open();

private:
  using AppList = std::vector<std::unique_ptr<App>>;

private:
  sen::kernel::BusAddress address_;
  std::string querySelection_;
  sen::kernel::RunApi& api_;
  EventExplorer* eventExplorer_;
  std::string windowLabel_;
  int windowId_ = 0;
  std::shared_ptr<sen::Subscription<sen::Object>> objects_;
  AppList apps_;
  PrinterRegistry printerRegistry_;
  ReporterRegistry reporterRegistry_;
  bool checked_ = true;
  ImGuiTextFilter textFilter_;
};

#endif  // SEN_COMPONENTS_EXPLORER_SRC_BUS_WINDOW_H
