// === command_engine_watch.cpp ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "command_engine.h"

// component
#include "parse_utils.h"

// component
#include "styles.h"
#include "unicode.h"
#include "value_formatter.h"

// sen
#include "sen/core/base/span.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/obj/callback.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// std
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sen::components::term
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

/// Build the FTXUI element for a single event emission.
ftxui::Element formatEventEmission(std::string_view objectName,
                                   std::string_view eventName,
                                   const EventInfo& info,
                                   const VarList& values,
                                   Span<const Arg> eventArgs)
{
  auto fullTime = info.creationTime.toLocalString();
  auto shortTime = (fullTime.size() >= 19U) ? fullTime.substr(11U, 8U) : fullTime;

  // Object name: split session.bus prefix (dimmed) from object name (bold).
  // The objectName uses the full local name (component.session.bus.object...), skip the
  // component prefix (first segment).
  ftxui::Elements nameParts;
  auto firstDot = objectName.find('.');
  if (firstDot != std::string_view::npos)
  {
    auto withoutComponent = objectName.substr(firstDot + 1);
    auto busDot = withoutComponent.find('.');
    if (busDot != std::string_view::npos)
    {
      auto objectStart = withoutComponent.find('.', busDot + 1);
      if (objectStart != std::string_view::npos)
      {
        nameParts.push_back(ftxui::text(std::string(withoutComponent.substr(0, objectStart + 1))) |
                            styles::descriptionText());
        nameParts.push_back(ftxui::text(std::string(withoutComponent.substr(objectStart + 1))) | ftxui::bold);
      }
      else
      {
        nameParts.push_back(ftxui::text(std::string(withoutComponent)) | ftxui::bold);
      }
    }
    else
    {
      nameParts.push_back(ftxui::text(std::string(withoutComponent)) | ftxui::bold);
    }
  }
  else
  {
    nameParts.push_back(ftxui::text(std::string(objectName)) | ftxui::bold);
  }

  auto header = ftxui::hbox({
    ftxui::text(std::string("  ") + unicode::bullet + " ") | styles::eventBadge(),
    ftxui::hbox(std::move(nameParts)),
    ftxui::text(std::string(" ") + unicode::arrowRight + " ") | styles::descriptionText(),
    ftxui::text(std::string(eventName)) | ftxui::bold | styles::eventBadge(),
    ftxui::filler(),
    ftxui::text(shortTime + " ") | styles::descriptionText(),
  });

  if (values.empty())
  {
    return header;
  }

  // Line 2+: arguments, indented, with paragraph wrapping.
  ftxui::Elements argParts;
  for (std::size_t i = 0; i < values.size(); ++i)
  {
    if (i > 0)
    {
      argParts.push_back(ftxui::text("  "));
    }
    if (i < eventArgs.size())
    {
      argParts.push_back(ftxui::text(std::string(eventArgs[i].name) + ": ") | styles::fieldName());
    }
    argParts.push_back(formatValue(values[i], i < eventArgs.size() ? *eventArgs[i].type : *StringType::get(), 0, true));
  }
  auto argLine = ftxui::hbox({ftxui::text("     "), ftxui::hbox(std::move(argParts))});

  return ftxui::vbox({header, argLine});
}

/// Split `<object>.<property>` on the last dot. `reject`, as for a method call: "obj." names nothing to
/// listen to.
bool splitObjectProperty(std::string_view token, std::string_view& object, std::string_view& property)
{
  return splitObjectMember(token, TrailingSeparator::reject, object, property);
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Listen commands
//--------------------------------------------------------------------------------------------------------------

/// Register a single property watch. Returns true on success.
//--------------------------------------------------------------------------------------------------------------

namespace
{

/// True when `fullLocalName` names the object the user referred to as `typed`, which may be a
/// scope-relative name. A suffix on a segment boundary, so no scope is consulted.
bool nameRefersTo(std::string_view fullLocalName, std::string_view typed)
{
  if (fullLocalName == typed)
  {
    return true;
  }
  if (typed.empty() || fullLocalName.size() <= typed.size())
  {
    return false;
  }
  auto start = fullLocalName.size() - typed.size();
  return fullLocalName.compare(start, typed.size(), typed) == 0 && fullLocalName[start - 1U] == '.';
}

}  // namespace

/// Register a single event listener. Returns true on success.
bool CommandEngine::listenEvent(std::string_view objectName,
                                std::string_view eventName,
                                const std::shared_ptr<Object>& target,
                                const Event* event)
{
  const std::string key = std::string(objectName) + "." + std::string(eventName);
  if (listeners_.count(key) != 0U)
  {
    return false;
  }

  // The key carries the name the user typed, so the same event reached by a relative name and by a
  // qualified one would install two guards and print every emission twice.
  for (const auto& [existingKey, existing]: listeners_)
  {
    if (existing.fullName == target->getLocalName() && existing.eventName == eventName)
    {
      return false;
    }
  }

  const auto eventArgs = event->getArgs();
  auto fullObjectName = std::string(target->getLocalName());
  auto evName = std::string(eventName);
  EventCallback<VarList> callback {
    api_.getWorkQueue(),
    [this, fullObjectName, evName, eventArgs](const EventInfo& info, const VarList& values)
    { app_.appendEventElement(formatEventEmission(fullObjectName, evName, info, values, eventArgs)); }};

  auto guard = target->onEventUntyped(event, std::move(callback));

  Listener l;
  l.objectName = std::string(objectName);
  l.fullName = fullObjectName;
  l.eventName = std::string(eventName);
  l.object = target;
  l.event = event;
  l.guard = std::move(guard);
  listeners_.emplace(key, std::move(l));
  return true;
}

void CommandEngine::cmdListen(std::string_view args)
{
  if (args.empty())
  {
    reportError("Usage", "listen <object>[.<event>]");
    return;
  }

  auto wholeObject = completer_.findObject(args);
  if (wholeObject)
  {
    const auto* classType = wholeObject->getClass().type();
    auto events = classType->getEvents(ClassType::SearchMode::includeParents);
    std::size_t added = 0;
    for (const auto& ev: events)
    {
      if (listenEvent(args, ev->getName(), wholeObject, ev.get()))
      {
        ++added;
      }
    }
    if (added > 0)
    {
      app_.appendInfo("Listening to " + std::to_string(added) + " events on '" + std::string(args) + "'.");
    }
    else if (events.empty())
    {
      app_.appendInfo("'" + std::string(args) + "' has no events.");
    }
    else
    {
      app_.appendInfo("Already listening to all events on '" + std::string(args) + "'.");
    }
    return;
  }

  std::string_view objectName;
  std::string_view eventName;
  if (splitObjectProperty(args, objectName, eventName))
  {
    auto target = completer_.findObject(objectName);
    if (target)
    {
      const auto* classType = target->getClass().type();
      const Event* event = nullptr;
      auto events = classType->getEvents(ClassType::SearchMode::includeParents);
      for (const auto& ev: events)
      {
        if (ev->getName() == eventName)
        {
          event = ev.get();
          break;
        }
      }
      if (event == nullptr)
      {
        reportError("Unknown Event",
                    "Event '" + std::string(eventName) + "' not found on '" + std::string(objectName) + "'.");
        return;
      }
      if (!listenEvent(objectName, eventName, target, event))
      {
        app_.appendInfo("Already listening to '" + std::string(args) + "'.");
        return;
      }
      app_.appendInfo("Listening to '" + std::string(args) + "'.");
      return;
    }
  }

  const std::string key = std::string(args);
  if (listeners_.count(key) != 0U)
  {
    app_.appendInfo("Already listening to '" + key + "'.");
    return;
  }

  Listener l;
  l.objectName = std::string(args);
  listeners_.emplace(key, std::move(l));
  app_.appendInfo("'" + key + "' is not here yet. Listening from the moment it appears.");
}

void CommandEngine::cmdUnlisten(std::string_view args)
{
  if (args.empty())
  {
    reportError("Usage", "unlisten <object>.<event> | all");
    return;
  }

  if (args == "all")
  {
    if (listeners_.empty())
    {
      app_.appendInfo("No active listeners.");
      return;
    }
    auto count = listeners_.size();
    listeners_.clear();
    app_.appendInfo("Cleared " + std::to_string(count) + " listener" + (count == 1 ? "" : "s") + ".");
    return;
  }

  const std::string key {args};
  auto it = listeners_.find(key);
  if (it != listeners_.end())
  {
    listeners_.erase(it);
    app_.appendInfo("Stopped listening to '" + key + "'.");
    return;
  }

  const std::string prefix = std::string(args) + ".";
  std::size_t removed = 0;
  for (auto lit = listeners_.begin(); lit != listeners_.end();)
  {
    if (lit->first.compare(0, prefix.size(), prefix) == 0)
    {
      lit = listeners_.erase(lit);
      ++removed;
    }
    else
    {
      ++lit;
    }
  }
  if (removed > 0)
  {
    app_.appendInfo("Stopped " + std::to_string(removed) + " listener" + (removed == 1 ? "" : "s") + " on '" +
                    std::string(args) + "'.");
    return;
  }

  reportError("Not Listening", "No active listener matching '" + std::string(args) + "'.");
}

void CommandEngine::cmdListeners(std::string_view /*args*/)
{
  if (listeners_.empty())
  {
    app_.appendInfo("No active listeners.  Use 'listen <object>.<event>' to start one.");
    return;
  }

  ftxui::Elements rows;
  rows.push_back(ftxui::text("Active listeners:") | ftxui::bold);
  for (const auto& [key, listener]: listeners_)
  {
    ftxui::Elements row {ftxui::text("  "), ftxui::text(key)};
    if (listener.fullName.empty())
    {
      row.push_back(ftxui::text("  (not here yet)") | styles::mutedText());
    }
    else if (listener.object.lock() == nullptr)
    {
      row.push_back(ftxui::text("  (gone, waiting for it to come back)") | styles::mutedText());
    }
    rows.push_back(ftxui::hbox(std::move(row)));
  }
  app_.appendElement(ftxui::vbox(std::move(rows)));
}

//--------------------------------------------------------------------------------------------------------------
// Object lifecycle callbacks (listen reconnection)
//--------------------------------------------------------------------------------------------------------------

void CommandEngine::onObjectRemoved(const std::shared_ptr<Object>& obj)
{
  for (auto& [key, listener]: listeners_)
  {
    auto locked = listener.object.lock();
    if (locked == obj)
    {
      listener.guard = ConnectionGuard {};
      // Drop our own handles too: the reconnect gate tests this weak_ptr, and leaving it set made
      // being reconnectable depend on every other holder having released the object first.
      listener.object.reset();
      listener.event = nullptr;
    }
  }
}

void CommandEngine::onObjectAdded(const std::shared_ptr<Object>& obj)
{
  std::vector<std::string> pendingListenAllKeys;

  for (auto& [key, listener]: listeners_)
  {
    if (listener.object.lock() != nullptr)
    {
      continue;
    }

    const bool firstBind = listener.fullName.empty();

    // Match on the full local name recorded when the listener was created. Re-resolving the typed
    // name through the Completer made reconnection depend on the user's current scope, and on the
    // completer's index being up to date in the same tick the object arrived.
    auto arrived = obj->getLocalName();
    if (listener.fullName.empty())
    {
      // Registered before its object existed, so there was no full name to record. The typed name
      // may be scope-relative; a segment-boundary suffix match relates it to the arriving object
      // without asking what scope the user is in.
      if (nameRefersTo(arrived, listener.objectName))
      {
        pendingListenAllKeys.push_back(key);
        continue;
      }
      std::string_view objPart;
      std::string_view evPart;
      if (!splitObjectProperty(listener.objectName, objPart, evPart) || !nameRefersTo(arrived, objPart))
      {
        continue;
      }
      listener.fullName = std::string(arrived);
      listener.objectName = std::string(objPart);
      listener.eventName = std::string(evPart);
    }
    else if (listener.fullName != arrived)
    {
      continue;
    }

    const auto* classType = obj->getClass().type();
    const Event* event = nullptr;
    auto events = classType->getEvents(ClassType::SearchMode::includeParents);
    for (const auto& ev: events)
    {
      if (ev->getName() == listener.eventName)
      {
        event = ev.get();
        break;
      }
    }
    if (event == nullptr)
    {
      continue;
    }

    const auto eventArgs = event->getArgs();
    auto fullObjName = std::string(obj->getLocalName());
    auto evName = listener.eventName;
    EventCallback<VarList> callback {
      api_.getWorkQueue(),
      [this, fullObjName, evName, eventArgs](const EventInfo& info, const VarList& values)
      { app_.appendEventElement(formatEventEmission(fullObjName, evName, info, values, eventArgs)); }};

    listener.guard = obj->onEventUntyped(event, std::move(callback));
    listener.object = obj;
    listener.event = event;

    if (firstBind)
    {
      // A relative name can fit several objects and the first to arrive takes it, so name the one
      // this bound to.
      app_.appendInfo("Listening to '" + listener.objectName + "." + listener.eventName + "' (" + listener.fullName +
                      ").");
    }
  }

  for (const auto& key: pendingListenAllKeys)
  {
    auto it = listeners_.find(key);
    if (it == listeners_.end())
    {
      continue;
    }
    const auto objectName = it->second.objectName;
    listeners_.erase(it);

    const auto* classType = obj->getClass().type();
    auto events = classType->getEvents(ClassType::SearchMode::includeParents);
    std::size_t added = 0;
    for (const auto& ev: events)
    {
      if (listenEvent(objectName, ev->getName(), obj, ev.get()))
      {
        ++added;
      }
    }

    const auto fullName = std::string(obj->getLocalName());
    if (added > 0)
    {
      app_.appendInfo("Listening to " + std::to_string(added) + " events on '" + objectName + "' (" + fullName + ").");
    }
    else if (events.empty())
    {
      app_.appendInfo("'" + objectName + "' appeared as " + fullName + ", and its class has no events.");
    }
    else
    {
      app_.appendInfo("Already listening to all events on '" + objectName + "'.");
    }
  }
}

}  // namespace sen::components::term
