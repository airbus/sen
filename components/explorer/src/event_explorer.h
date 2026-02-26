// === event_explorer.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_EXPLORER_SRC_EVENT_EXPLORER_H
#define SEN_COMPONENTS_EXPLORER_SRC_EVENT_EXPLORER_H

#include "circular_list.h"
#include "imgui.h"
#include "sen/kernel/component.h"

#include <string>
#include <vector>

class ObjectState;

class EventExplorer
{
  SEN_NOCOPY_NOMOVE(EventExplorer)

public:
  explicit EventExplorer(sen::impl::WorkQueue* workQueue);
  ~EventExplorer() = default;

public:
  void update();
  void stopTracking(sen::ObjectId emitter);
  void startTracking(sen::Object* emitter, const sen::Event* event);
  void stopTracking(sen::ObjectId emitter, const sen::Event* event);
  [[nodiscard]] static const char* getWindowName() noexcept;

private:
  struct EventLogEntry
  {
    sen::ObjectId emitterId = {0};
    std::string emitterName = {};
    const sen::ClassType* emitterClass = nullptr;
    const sen::Event* ev = nullptr;
    sen::EventInfo eventInfo {};
    sen::VarList args {};
    std::string text {};
    std::string valueText {};
  };

  struct Track
  {
    const sen::Event* event = nullptr;
    sen::ObjectId objectId;
    sen::ConnectionGuard guard;
  };

  void draw(const EventLogEntry& entry) const;

private:
  void clearHistory();

  void registerEvent(sen::ObjectId emitterId,
                     const std::string& emitterName,
                     const sen::ClassType* emitterClass,
                     const sen::EventInfo& info,
                     const sen::Event* event,
                     const sen::VarList& args);

private:
  sen::impl::WorkQueue* workQueue_;
  CircularList<EventLogEntry> history_;
  bool autoScroll_ = true;
  ImGuiTextFilter textFilter_;
  std::vector<Track> tracks_;
};

#endif  // SEN_COMPONENTS_EXPLORER_SRC_EVENT_EXPLORER_H
