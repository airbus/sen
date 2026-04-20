// === command_engine_watch.cpp ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "command_engine.h"

// local
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

/// Unwrap alias/optional layers and check if the underlying type is composite (struct/seq/variant).
bool isCompositeType(const Type* type)
{
  const Type* t = type;
  while (auto* a = t->asAliasType())
  {
    t = a->getAliasedType().type();
  }
  if (auto* o = t->asOptionalType(); o != nullptr)
  {
    const Type* inner = o->getType().type();
    while (auto* a = inner->asAliasType())
    {
      inner = a->getAliasedType().type();
    }
    t = inner;
  }
  return (t->asStructType() != nullptr) || (t->asSequenceType() != nullptr) || (t->asVariantType() != nullptr);
}

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

/// Split `<object>.<property>` on the last dot.
bool splitObjectProperty(std::string_view token, std::string_view& object, std::string_view& property)
{
  auto dot = token.rfind('.');
  if (dot == std::string_view::npos || dot == 0 || dot == token.size() - 1)
  {
    return false;
  }
  object = token.substr(0, dot);
  property = token.substr(dot + 1);
  return true;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Watch commands
//--------------------------------------------------------------------------------------------------------------

/// Register a single property watch. Returns true on success.
bool CommandEngine::watchProperty(std::string_view objectName,
                                  std::string_view propertyName,
                                  const std::shared_ptr<Object>& target,
                                  const Property* property)
{
  const std::string key = std::string(objectName) + "." + std::string(propertyName);
  if (watches_.count(key) != 0U)
  {
    return false;  // already watching
  }

  // Determine inline vs composite rendering.
  const bool inlineValue = !isCompositeType(property->getType().type());

  std::weak_ptr<Object> weakObj = target;
  EventCallback<VarList> callback {api_.getWorkQueue(),
                                   [this, key, property, inlineValue, weakObj](const VarList& /*args*/)
                                   {
                                     auto obj = weakObj.lock();
                                     if (!obj)
                                     {
                                       return;
                                     }
                                     Var value = obj->getPropertyUntyped(property);
                                     ftxui::Element valueElement = formatValue(value, *property->getType());
                                     app_.updateWatch(key, std::move(valueElement), inlineValue);
                                   }};

  auto guard = target->onPropertyChangedUntyped(property, std::move(callback));

  // Seed with the current value.
  {
    Var initial = target->getPropertyUntyped(property);
    ftxui::Element valueElement = formatValue(initial, *property->getType());
    app_.updateWatch(key, std::move(valueElement), inlineValue);
  }

  Watch w;
  w.objectName = std::string(objectName);
  w.propertyName = std::string(propertyName);
  w.object = target;
  w.property = property;
  w.guard = std::move(guard);
  watches_.emplace(key, std::move(w));
  return true;
}

void CommandEngine::cmdWatch(std::string_view args)
{
  if (args.empty())
  {
    reportError("Usage", "watch <object>[.<property>]");
    return;
  }

  // Whole-object path -> watch all properties.
  auto wholeObject = completer_.findObject(args);
  if (wholeObject)
  {
    const auto* classType = wholeObject->getClass().type();
    auto properties = classType->getProperties(ClassType::SearchMode::includeParents);
    std::size_t added = 0;
    for (const auto& prop: properties)
    {
      if (watchProperty(args, prop->getName(), wholeObject, prop.get()))
      {
        ++added;
      }
    }
    if (added > 0)
    {
      app_.showWatchPane();
      app_.appendInfo("Watching " + std::to_string(added) + " properties on '" + std::string(args) + "'.");
    }
    else
    {
      app_.appendInfo("Already watching all properties on '" + std::string(args) + "'.");
    }
    return;
  }

  std::string_view objectName;
  std::string_view propertyName;
  if (splitObjectProperty(args, objectName, propertyName))
  {
    auto target = completer_.findObject(objectName);
    if (target)
    {
      const auto* classType = target->getClass().type();
      const auto* property = classType->searchPropertyByName(propertyName);
      if (property == nullptr)
      {
        reportError("Unknown Property",
                    "Property '" + std::string(propertyName) + "' not found on '" + std::string(objectName) + "'.");
        return;
      }
      if (!watchProperty(objectName, propertyName, target, property))
      {
        app_.appendInfo("Already watching '" + std::string(args) + "'.");
        return;
      }
      app_.showWatchPane();
      app_.appendInfo("Watching '" + std::string(args) + "'.");
      return;
    }
  }

  // Pending watch. Activates via onObjectAdded.
  {
    const std::string key = std::string(args);
    if (watches_.count(key) != 0U)
    {
      app_.appendInfo("Already watching '" + key + "'.");
      return;
    }

    Watch w;
    w.objectName = std::string(args);
    w.pending = true;
    watches_.emplace(key, std::move(w));
    app_.addPendingWatch(key);
    app_.showWatchPane();
    app_.appendInfo("Watching '" + key + "'.");
  }
}

void CommandEngine::cmdUnwatch(std::string_view args)
{
  if (args.empty())
  {
    reportError("Usage", "unwatch <object>[.<property>] | all");
    return;
  }

  if (args == "all")
  {
    if (watches_.empty())
    {
      app_.appendInfo("No active watches.");
      return;
    }
    const auto count = watches_.size();
    watches_.clear();
    app_.clearAllWatches();
    app_.appendInfo("Cleared " + std::to_string(count) + " watch" + (count == 1 ? "" : "es") + ".");
    return;
  }

  const std::string key {args};
  auto it = watches_.find(key);
  if (it != watches_.end())
  {
    watches_.erase(it);
    app_.removeWatch(key);
    app_.appendInfo("Stopped watching '" + key + "'.");
    return;
  }

  const std::string prefix = std::string(args) + ".";
  std::size_t removed = 0;
  for (auto wit = watches_.begin(); wit != watches_.end();)
  {
    if (wit->first.compare(0, prefix.size(), prefix) == 0)
    {
      app_.removeWatch(wit->first);
      wit = watches_.erase(wit);
      ++removed;
    }
    else
    {
      ++wit;
    }
  }
  if (removed > 0)
  {
    app_.appendInfo("Stopped " + std::to_string(removed) + " watch" + (removed == 1 ? "" : "es") + " on '" +
                    std::string(args) + "'.");
    return;
  }

  reportError("Not Watched", "No active watch matching '" + std::string(args) + "'.");
}

void CommandEngine::cmdWatches(std::string_view /*args*/)
{
  if (watches_.empty())
  {
    app_.appendInfo("No active watches.  Use 'watch <object>.<property>' to start one.");
    return;
  }

  ftxui::Elements rows;
  rows.push_back(ftxui::text("Active watches:") | ftxui::bold);
  for (const auto& [key, watch]: watches_)
  {
    rows.push_back(ftxui::hbox({ftxui::text("  "), ftxui::text(key)}));
  }
  app_.appendElement(ftxui::vbox(std::move(rows)));
}

//--------------------------------------------------------------------------------------------------------------
// Listen commands
//--------------------------------------------------------------------------------------------------------------

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
  app_.appendInfo("Listening to '" + key + "'.");
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
    rows.push_back(ftxui::hbox({ftxui::text("  "), ftxui::text(key)}));
  }
  app_.appendElement(ftxui::vbox(std::move(rows)));
}

//--------------------------------------------------------------------------------------------------------------
// Object lifecycle callbacks (watch/listen reconnection)
//--------------------------------------------------------------------------------------------------------------

void CommandEngine::onObjectRemoved(const std::shared_ptr<Object>& obj)
{
  for (auto& [key, watch]: watches_)
  {
    auto locked = watch.object.lock();
    if (locked == obj)
    {
      watch.guard = ConnectionGuard {};
      app_.setWatchConnected(key, false);
    }
  }

  for (auto& [key, listener]: listeners_)
  {
    auto locked = listener.object.lock();
    if (locked == obj)
    {
      listener.guard = ConnectionGuard {};
    }
  }
}

void CommandEngine::onObjectAdded(const std::shared_ptr<Object>& obj)
{
  std::vector<std::string> pendingAllKeys;

  // Manual unpack instead of `auto& [key, watch]` because the lambda below captures `key`
  // by value, and C++17 does not permit capturing structured-binding names.
  for (auto& entry: watches_)
  {
    const auto& key = entry.first;
    auto& watch = entry.second;
    if (watch.object.lock() != nullptr)
    {
      continue;
    }

    if (watch.pending && watch.propertyName.empty())
    {
      auto resolvedFull = completer_.findObject(watch.objectName);
      if (resolvedFull == obj)
      {
        pendingAllKeys.push_back(key);
        continue;
      }
      std::string_view objPart;
      std::string_view propPart;
      if (splitObjectProperty(watch.objectName, objPart, propPart))
      {
        auto resolvedObj = completer_.findObject(objPart);
        if (resolvedObj == obj)
        {
          watch.objectName = std::string(objPart);
          watch.propertyName = std::string(propPart);
          // Fall through to single-property reconnect below.
        }
        else
        {
          continue;
        }
      }
      else
      {
        continue;
      }
    }

    auto resolved = completer_.findObject(watch.objectName);
    if (resolved != obj)
    {
      continue;
    }

    const auto* classType = obj->getClass().type();
    const auto* property = classType->searchPropertyByName(watch.propertyName);
    if (property == nullptr)
    {
      continue;
    }

    const bool inlineValue = !isCompositeType(property->getType().type());

    std::weak_ptr<Object> weakObj = obj;
    EventCallback<VarList> callback {api_.getWorkQueue(),
                                     [this, key, property, inlineValue, weakObj](const VarList& /*args*/)
                                     {
                                       auto o = weakObj.lock();
                                       if (!o)
                                       {
                                         return;
                                       }
                                       Var value = o->getPropertyUntyped(property);
                                       app_.updateWatch(key, formatValue(value, *property->getType()), inlineValue);
                                     }};

    watch.guard = obj->onPropertyChangedUntyped(property, std::move(callback));
    watch.object = obj;
    watch.property = property;
    watch.pending = false;
    app_.setWatchConnected(key, true);

    Var initial = obj->getPropertyUntyped(property);
    app_.updateWatch(key, formatValue(initial, *property->getType()), inlineValue);
  }

  for (const auto& key: pendingAllKeys)
  {
    auto it = watches_.find(key);
    if (it == watches_.end())
    {
      continue;
    }
    const auto objectName = it->second.objectName;
    watches_.erase(it);
    app_.removeWatch(key);

    const auto* classType = obj->getClass().type();
    auto properties = classType->getProperties(ClassType::SearchMode::includeParents);
    for (const auto& prop: properties)
    {
      watchProperty(objectName, prop->getName(), obj, prop.get());
    }
  }

  std::vector<std::string> pendingListenAllKeys;
  for (auto& [key, listener]: listeners_)
  {
    if (listener.object.lock() != nullptr)
    {
      continue;
    }

    if (listener.eventName.empty())
    {
      auto resolvedFull = completer_.findObject(listener.objectName);
      if (resolvedFull == obj)
      {
        pendingListenAllKeys.push_back(key);
        continue;
      }
      std::string_view objPart;
      std::string_view evPart;
      if (splitObjectProperty(listener.objectName, objPart, evPart))
      {
        auto resolvedObj = completer_.findObject(objPart);
        if (resolvedObj == obj)
        {
          listener.objectName = std::string(objPart);
          listener.eventName = std::string(evPart);
          // Fall through to single-event reconnect below.
        }
        else
        {
          continue;
        }
      }
      else
      {
        continue;
      }
    }

    auto resolved = completer_.findObject(listener.objectName);
    if (resolved != obj)
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
    for (const auto& ev: events)
    {
      listenEvent(objectName, ev->getName(), obj, ev.get());
    }
  }
}

}  // namespace sen::components::term
