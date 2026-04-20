// === completer.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "completer.h"

// local
#include "command_engine.h"
#include "log_router.h"
#include "object_store.h"
#include "scope.h"
#include "suggester.h"
#include "unicode.h"

// sen
#include "sen/core/base/checked_conversions.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type_registry.h"

// std
#include <algorithm>
#include <cstddef>
#include <set>

namespace sen::components::term
{

using sen::std_util::checkedConversion;

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

/// Split input into whitespace-delimited tokens.
std::vector<std::string_view> tokenize(std::string_view input)
{
  std::vector<std::string_view> tokens;
  std::size_t i = 0;
  while (i < input.size())
  {
    while (i < input.size() && input[i] == ' ')
    {
      ++i;
    }
    if (i >= input.size())
    {
      break;
    }
    auto start = i;
    while (i < input.size() && input[i] != ' ')
    {
      ++i;
    }
    tokens.push_back(input.substr(start, i - start));
  }
  return tokens;
}

constexpr std::string_view logLevels[] = {"trace", "debug", "info", "warn", "error", "critical", "off"};

/// Extract the first path segment of `rel` (up to the first dot), or the whole string if no dot.
std::string_view firstSegment(std::string_view rel)
{
  auto dot = rel.find('.');
  return (dot == std::string_view::npos) ? rel : rel.substr(0, dot);
}

int scopeDepthFromKind(Scope::Kind kind)
{
  switch (kind)
  {
    case Scope::Kind::root:
    case Scope::Kind::query:
      return 0;
    case Scope::Kind::session:
      return 1;
    case Scope::Kind::bus:
      return 2;
    case Scope::Kind::group:
      return 3;
  }
  return 0;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Cache management
//--------------------------------------------------------------------------------------------------------------

void Completer::markScopeDirty() { scopeDirty_ = true; }
void Completer::markListsDirty() { listsDirty_ = true; }

void Completer::setTypeRegistry(const CustomTypeRegistry* registry) { types_ = registry; }
void Completer::setTuiMode(bool tuiMode) noexcept { tuiMode_ = tuiMode; }

void Completer::update(const Scope& scope, const ObjectStore& store, LogRouter& logRouter)
{
  // Object maps are only rebuilt when the scope itself changed. Object add/remove events
  // patch them incrementally via onObjectAdded/onObjectRemoved, so we never pay O(N) per cycle.
  if (scopeDirty_)
  {
    scopeDirty_ = false;
    scopeDepth_ = scopeDepthFromKind(scope.getKind());

    // Rebuild the object map + children counts from scratch against the new scope.
    std::unordered_map<std::string, std::shared_ptr<Object>> objByName;
    std::unordered_map<std::string, std::size_t> childCounts;
    std::vector<std::string> childNames;

    const auto& objects = store.getObjects();
    objByName.reserve(objects.size());

    for (const auto& obj: objects)
    {
      auto localName = obj->getLocalName();
      if (!scope.contains(localName))
      {
        continue;
      }
      auto rel = scope.relativeName(localName);
      if (rel.empty())
      {
        continue;
      }

      auto segment = std::string(firstSegment(rel));
      if (auto [itr, inserted] = childCounts.try_emplace(std::move(segment), 1U); !inserted)
      {
        ++itr->second;
      }

      objByName.try_emplace(std::move(rel), obj);
    }

    childNames.reserve(childCounts.size());
    for (const auto& [name, _count]: childCounts)
    {
      childNames.push_back(name);
    }
    std::sort(childNames.begin(), childNames.end());

    objectsByName_ = std::move(objByName);
    childCounts_ = std::move(childCounts);
    childNames_ = std::move(childNames);
    objectsInScope_ = objectsByName_.size();
  }

  // Refresh sources/queries/loggers only when something changed.
  auto gen = store.getGeneration();
  if (gen != lastStoreGeneration_ || listsDirty_)
  {
    lastStoreGeneration_ = gen;
    listsDirty_ = false;

    openSources_ = store.getOpenSources();
    availableSources_ = store.getAvailableSources();

    auto queries = store.getQueries();
    std::vector<std::string> qnames;
    qnames.reserve(queries.size());
    for (auto& q: queries)
    {
      qnames.push_back(std::move(q.name));
    }
    queryNames_ = std::move(qnames);
  }

  if (logRouter.hasNewLoggers())
  {
    auto loggers = logRouter.listLoggers();
    std::vector<std::string> lnames;
    lnames.reserve(loggers.size());
    for (auto& info: loggers)
    {
      lnames.push_back(std::move(info.name));
    }
    loggerNames_ = std::move(lnames);
  }
}

void Completer::onObjectAdded(const Scope& scope, std::shared_ptr<Object> obj)
{
  // If a scope change is pending, skip the patch: the upcoming update() will rebuild from the
  // authoritative object list and will see this object. Patching stale maps now just creates
  // work to be thrown away.
  if (scopeDirty_)
  {
    return;
  }

  auto localName = obj->getLocalName();
  if (!scope.contains(localName))
  {
    return;
  }
  auto rel = scope.relativeName(localName);
  if (rel.empty())
  {
    return;
  }

  auto segmentStr = std::string(firstSegment(rel));

  // Move the shared_ptr into the map: we took it by value and we're the final owner of this copy.
  auto [itr, inserted] = objectsByName_.try_emplace(std::move(rel), std::move(obj));
  if (!inserted)
  {
    // Already present (another add for the same name). Don't double-count children.
    return;
  }
  ++objectsInScope_;

  auto& count = childCounts_[segmentStr];
  ++count;
  if (count == 1)
  {
    // Transitioned 0->1: insert the new segment into childNames_ at the right position.
    auto pos = std::lower_bound(childNames_.begin(), childNames_.end(), segmentStr);
    childNames_.insert(pos, std::move(segmentStr));
  }
}

void Completer::onObjectRemoved(const Scope& scope, const std::shared_ptr<Object>& obj)
{
  if (scopeDirty_)
  {
    return;
  }

  auto localName = obj->getLocalName();
  if (!scope.contains(localName))
  {
    return;
  }
  auto rel = scope.relativeName(localName);
  if (rel.empty())
  {
    return;
  }

  auto segment = std::string(firstSegment(rel));

  auto erased = objectsByName_.erase(rel);
  if (erased == 0)
  {
    // Never saw this one (arrived while scope was dirty, etc.). Nothing to undo.
    return;
  }
  --objectsInScope_;

  auto cItr = childCounts_.find(segment);
  if (cItr == childCounts_.end())
  {
    return;  // invariant violated, but stay defensive
  }
  if (--cItr->second == 0)
  {
    childCounts_.erase(cItr);
    auto nItr = std::lower_bound(childNames_.begin(), childNames_.end(), segment);
    if (nItr != childNames_.end() && *nItr == segment)
    {
      childNames_.erase(nItr);
    }
  }
}

std::shared_ptr<Object> Completer::findObject(std::string_view relativeName) const
{
  auto it = objectsByName_.find(std::string(relativeName));
  if (it != objectsByName_.end())
  {
    return it->second;
  }
  return nullptr;
}

std::vector<std::string> Completer::findObjectSuggestions(std::string_view query, std::size_t maxSuggestions) const
{
  if (query.empty() || maxSuggestions == 0)
  {
    return {};
  }

  // We don't want to allocate a full name list when only a handful of suggestions are needed,
  // so we pull the keys one by one and rank them inline. A small heap keeps the top-k.
  struct Scored
  {
    std::size_t distance;
    std::string name;
  };
  std::vector<Scored> ranked;
  ranked.reserve(maxSuggestions + 1);

  const std::size_t threshold = suggestionThreshold(query.size());

  auto tryInsert = [&](std::size_t d, std::string_view name)
  {
    // Keep only the top maxSuggestions by distance; drop the worst when full.
    if (ranked.size() < maxSuggestions)
    {
      ranked.push_back({d, std::string(name)});
      return;
    }
    auto worst = std::max_element(
      ranked.begin(), ranked.end(), [](const Scored& a, const Scored& b) { return a.distance < b.distance; });
    if (d < worst->distance)
    {
      *worst = {d, std::string(name)};
    }
  };

  for (const auto& [name, _obj]: objectsByName_)
  {
    auto d = scoreSuggestion(query, name);
    if (d > threshold)
    {
      continue;
    }
    tryInsert(d, name);
  }

  std::sort(ranked.begin(), ranked.end(), [](const Scored& a, const Scored& b) { return a.distance < b.distance; });

  std::vector<std::string> out;
  out.reserve(ranked.size());
  for (auto& r: ranked)
  {
    out.push_back(std::move(r.name));
  }
  return out;
}

std::size_t Completer::objectsInScopeCount() const { return objectsInScope_; }

//--------------------------------------------------------------------------------------------------------------
// Completion
//--------------------------------------------------------------------------------------------------------------

CompletionResult Completer::complete(std::string_view input, int cursorPos) const
{
  auto upToCursor = input.substr(0, checkedConversion<std::size_t>(cursorPos));
  auto tokens = tokenize(upToCursor);

  // Determine the prefix being completed and its position
  bool endsWithSpace = !upToCursor.empty() && upToCursor.back() == ' ';
  std::string_view prefix;
  int replaceFrom = cursorPos;

  if (endsWithSpace || tokens.empty())
  {
    prefix = {};
    replaceFrom = cursorPos;
  }
  else
  {
    prefix = tokens.back();
    replaceFrom = cursorPos - checkedConversion<int>(prefix.size());
  }

  std::size_t completedTokens = endsWithSpace ? tokens.size() : (tokens.empty() ? 0 : tokens.size() - 1);

  CompletionResult result;
  result.replaceFrom = replaceFrom;
  result.replaceTo = cursorPos;

  if (completedTokens == 0)
  {
    auto dotSplit = splitObjectMethod(prefix);
    if (dotSplit.has_value())
    {
      auto [objPath, methodPrefix] = *dotSplit;
      auto it = objectsByName_.find(std::string(objPath));
      if (it != objectsByName_.end())
      {
        // Object match: offer methods and child objects sharing the prefix.
        result.candidates = completeMethodName(objPath, methodPrefix);
        auto pathCandidates = completeObjectOrCommand(prefix);
        for (auto& c: pathCandidates)
        {
          if (c.kind != CompletionKind::command)
          {
            result.candidates.push_back(std::move(c));
          }
        }
      }
      else
      {
        result.candidates = completeObjectOrCommand(prefix);
      }
    }
    else
    {
      result.candidates = completeObjectOrCommand(prefix);
    }
  }
  else
  {
    auto cmd = tokens[0];

    if (cmd == "cd" || cmd == "ls")
    {
      if (completedTokens <= 1)
      {
        result.candidates = completeCdArg(prefix);
      }
    }
    else if (cmd == "open")
    {
      if (completedTokens <= 1)
      {
        result.candidates = completeOpenArg(prefix);
      }
    }
    else if (cmd == "close")
    {
      if (completedTokens <= 1)
      {
        result.candidates = completeCloseArg(prefix);
      }
    }
    else if (cmd == "query")
    {
      if (completedTokens == 1)
      {
        // First arg: "rm" subcommand or existing query names (for reference).
        result.candidates = completeQueryRmArg(prefix);
        if (std::string_view("rm").compare(0, prefix.size(), prefix) == 0)
        {
          result.candidates.push_back({"rm", "remove a query", {}, CompletionKind::value});
        }
      }
      else if (completedTokens >= 2 && tokens[1] == "rm")
      {
        // query rm <name>
        result.candidates = completeQueryRmArg(prefix);
      }
      else if (completedTokens == 2)
      {
        // query <name> -> suggest SELECT keyword
        if (std::string_view("SELECT").compare(0, prefix.size(), prefix) == 0)
        {
          result.candidates.push_back({"SELECT", "begin query", {}, CompletionKind::value});
        }
      }
      else if (completedTokens >= 3)
      {
        // Positional: SELECT <Type> FROM <bus> WHERE ...
        // Find which keyword position we're at by scanning tokens.
        bool seenSelect = false;
        bool seenFrom = false;
        bool seenWhere = false;
        for (std::size_t t = 2; t < tokens.size() - (endsWithSpace ? 0U : 1U); ++t)
        {
          if (tokens[t] == "SELECT")
          {
            seenSelect = true;
          }
          else if (tokens[t] == "FROM")
          {
            seenFrom = true;
          }
          else if (tokens[t] == "WHERE")
          {
            seenWhere = true;
          }
        }

        if (seenSelect && !seenFrom && !seenWhere)
        {
          // After SELECT: type names + "*"
          if (prefix.empty() || prefix[0] == '*')
          {
            result.candidates.push_back({"*", "any type", {}, CompletionKind::value});
          }
          if (types_ != nullptr)
          {
            auto allTypes = types_->getAll();
            for (const auto& [name, type]: allTypes)
            {
              if (type->asClassType() != nullptr && name.size() >= prefix.size() &&
                  name.compare(0, prefix.size(), prefix) == 0)
              {
                result.candidates.push_back({name, "class", {}, CompletionKind::value});
              }
            }
          }
          // Also suggest FROM if type was already entered
          if (std::string_view("FROM").compare(0, prefix.size(), prefix) == 0)
          {
            result.candidates.push_back({"FROM", "", {}, CompletionKind::value});
          }
        }
        else if (seenFrom && !seenWhere)
        {
          // After FROM: bus addresses (open + available + *) so users can query buses
          // they haven't opened yet. Sen will open them on demand when the query runs.
          if (prefix.empty() || prefix[0] == '*')
          {
            result.candidates.push_back({"*", "any bus", {}, CompletionKind::value});
          }
          std::set<std::string> seen;
          for (const auto& src: openSources_)
          {
            if (src.size() >= prefix.size() && src.compare(0, prefix.size(), prefix) == 0 && seen.insert(src).second)
            {
              result.candidates.push_back({src, "open", {}, CompletionKind::value});
            }
          }
          for (const auto& src: availableSources_)
          {
            if (src.size() >= prefix.size() && src.compare(0, prefix.size(), prefix) == 0 && seen.insert(src).second)
            {
              result.candidates.push_back({src, "available", {}, CompletionKind::value});
            }
          }
          if (std::string_view("WHERE").compare(0, prefix.size(), prefix) == 0)
          {
            result.candidates.push_back({"WHERE", "", {}, CompletionKind::value});
          }
        }
      }
    }
    else if (cmd == "help")
    {
      if (completedTokens <= 1)
      {
        result.candidates = completeCommand(prefix);
      }
    }
    else if (cmd == "theme")
    {
      if (completedTokens <= 1)
      {
        const auto& enumType = *MetaTypeTrait<ThemeStyle>::meta();
        for (const auto& e: enumType.getEnums())
        {
          if (e.name.size() >= prefix.size() && e.name.compare(0, prefix.size(), prefix) == 0)
          {
            result.candidates.push_back({std::string(e.name), "", {}, CompletionKind::value});
          }
        }
      }
    }
    else if (cmd == "log")
    {
      result.candidates = completeLogArg(prefix, tokens);
    }
    else if (cmd == "inspect" || cmd == "types")
    {
      if (completedTokens <= 1)
      {
        auto candidates = completeObjectOrCommand(prefix);
        candidates.erase(std::remove_if(candidates.begin(),
                                        candidates.end(),
                                        [](const Completion& c) { return c.kind == CompletionKind::command; }),
                         candidates.end());
        if (types_ != nullptr)
        {
          auto allTypes = types_->getAll();
          for (const auto& [name, type]: allTypes)
          {
            if (name.size() >= prefix.size() && name.compare(0, prefix.size(), prefix) == 0)
            {
              std::string kind;
              if (type->asClassType() != nullptr)
              {
                kind = "class";
              }
              else if (type->asStructType() != nullptr)
              {
                kind = "struct";
              }
              else if (type->asEnumType() != nullptr)
              {
                kind = "enum";
              }
              else if (type->asSequenceType() != nullptr)
              {
                kind = "sequence";
              }
              else if (type->asVariantType() != nullptr)
              {
                kind = "variant";
              }
              else
              {
                kind = "type";
              }
              candidates.push_back({name, kind, {}, CompletionKind::value});
            }
          }
        }
        result.candidates = std::move(candidates);
      }
    }
    else if (cmd == "units")
    {
      if (completedTokens <= 1)
      {
        static const std::vector<std::string> categoryNames = {
          "length",
          "mass",
          "time",
          "angle",
          "temperature",
          "density",
          "pressure",
          "area",
          "force",
          "frequency",
          "velocity",
          "angularVelocity",
          "acceleration",
          "angularAcceleration",
        };
        for (const auto& cat: categoryNames)
        {
          if (cat.size() >= prefix.size() && cat.compare(0, prefix.size(), prefix) == 0)
          {
            result.candidates.push_back({cat, "", {}, CompletionKind::value});
          }
        }
      }
    }
    else if (cmd == "listen" || cmd == "unlisten")
    {
      if (completedTokens <= 1)
      {
        result.candidates = completeListenArg(prefix);
        if (cmd == "unlisten" && std::string_view("all").compare(0, prefix.size(), prefix) == 0)
        {
          result.candidates.push_back({"all", "stop all listeners", {}, CompletionKind::value});
        }
      }
    }
    else if ((cmd == "watch" || cmd == "unwatch") && tuiMode_)
    {
      if (completedTokens <= 1)
      {
        result.candidates = completeWatchArg(prefix);
        if (cmd == "unwatch" && std::string_view("all").compare(0, prefix.size(), prefix) == 0)
        {
          result.candidates.push_back({"all", "stop all watches", {}, CompletionKind::value});
        }
      }
    }
  }

  // Remove duplicate candidates (same text)
  {
    std::set<std::string> seen;
    auto it = std::remove_if(result.candidates.begin(),
                             result.candidates.end(),
                             [&seen](const Completion& c) { return !seen.insert(c.text).second; });
    result.candidates.erase(it, result.candidates.end());
  }

  return result;
}

//--------------------------------------------------------------------------------------------------------------
// Completion strategies
//--------------------------------------------------------------------------------------------------------------

std::vector<Completion> Completer::completeCommand(std::string_view prefix) const
{
  std::vector<Completion> result;
  for (const auto& desc: CommandEngine::getCommandDescriptors())
  {
    if (desc.tuiOnly && !tuiMode_)
    {
      continue;
    }
    if (desc.name.size() >= prefix.size() && desc.name.compare(0, prefix.size(), prefix) == 0)
    {
      result.push_back({std::string(desc.name), std::string(desc.completionHint), {}, CompletionKind::command});
    }
  }
  return result;
}

std::vector<Completion> Completer::completeObjectOrCommand(std::string_view prefix) const
{
  // Only show command completions when there's no dot in the prefix
  std::vector<Completion> result;
  if (prefix.find('.') == std::string_view::npos)
  {
    result = completeCommand(prefix);
  }

  // Check for exact object match; offer methods instead of just the name.
  auto exactIt = objectsByName_.find(std::string(prefix));
  if (exactIt != objectsByName_.end())
  {
    return completeMethodName(prefix, {});
  }

  // Build path-segment completions from object names.
  // Like filesystem completion: "local." -> "local.demo", "local.kernel" (next segment only).
  // If a candidate is an exact object name, annotate with class; otherwise annotate as [path].
  std::set<std::string> seen;
  for (const auto& [name, obj]: objectsByName_)
  {
    if (name.size() < prefix.size() || name.compare(0, prefix.size(), prefix) != 0)
    {
      continue;
    }

    auto remainder = std::string_view(name).substr(prefix.size());
    auto dot = remainder.find('.');
    bool isLeaf = (dot == std::string_view::npos);
    std::string segment;
    if (isLeaf)
    {
      segment = name;
    }
    else
    {
      segment = std::string(prefix) + std::string(remainder.substr(0, dot));
    }

    if (!seen.insert(segment).second)
    {
      continue;
    }

    if (isLeaf)
    {
      auto objIt = objectsByName_.find(segment);
      auto className = (objIt != objectsByName_.end() && objIt->second)
                         ? std::string(objIt->second->getClass()->getName())
                         : std::string("?");
      result.push_back(Completion {segment, className, {}, CompletionKind::object});
    }
    else
    {
      // Label path segments based on their depth in the hierarchy
      // scopeDepth_ + number of dots in segment = absolute depth
      auto dotCount = std::count(segment.begin(), segment.end(), '.');
      int absoluteDepth = scopeDepth_ + checkedConversion<int>(dotCount) + 1;
      std::string label;
      if (absoluteDepth == 1)
      {
        label = "session";
      }
      else if (absoluteDepth == 2)
      {
        label = "bus";
      }
      else
      {
        label = "group";
      }
      result.push_back(Completion {segment, label, {}, CompletionKind::path});
    }
  }

  std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.text < b.text; });
  return result;
}

std::vector<Completion> Completer::completeMethodName(std::string_view objectName, std::string_view methodPrefix) const
{
  auto it = objectsByName_.find(std::string(objectName));
  if (it == objectsByName_.end() || !it->second)
  {
    return {};
  }

  const auto& methodCompletions = getMethodCompletions(it->second->getClass());
  std::string objPrefix = std::string(objectName) + ".";

  std::vector<Completion> result;
  for (const auto& mc: methodCompletions)
  {
    if (mc.text.size() >= methodPrefix.size() && mc.text.compare(0, methodPrefix.size(), methodPrefix) == 0)
    {
      Completion forwarded {objPrefix + mc.text, mc.display, mc.detail, mc.kind};
      forwarded.argCount = mc.argCount;
      result.push_back(std::move(forwarded));
    }
  }
  return result;
}

std::vector<Completion> Completer::completePropertyName(std::string_view objectName,
                                                        std::string_view propertyPrefix) const
{
  auto it = objectsByName_.find(std::string(objectName));
  if (it == objectsByName_.end() || !it->second)
  {
    return {};
  }
  const auto* classType = it->second->getClass().type();
  const auto properties = classType->getProperties(ClassType::SearchMode::includeParents);
  std::string objPrefix = std::string(objectName) + ".";

  std::vector<Completion> result;
  for (const auto& prop: properties)
  {
    auto propName = prop->getName();
    if (propName.size() < propertyPrefix.size() || propName.compare(0, propertyPrefix.size(), propertyPrefix) != 0)
    {
      continue;
    }
    auto typeName = std::string(prop->getType()->getName());
    auto propDesc = std::string(prop->getDescription());
    std::string detail =
      propDesc.empty() ? (objPrefix + std::string(propName) + " : " + typeName) : (propDesc + " (" + typeName + ")");
    result.push_back(
      Completion {objPrefix + std::string(propName), ": " + typeName, std::move(detail), CompletionKind::value});
  }
  return result;
}

std::vector<Completion> Completer::completeEventName(std::string_view objectName, std::string_view eventPrefix) const
{
  auto it = objectsByName_.find(std::string(objectName));
  if (it == objectsByName_.end() || !it->second)
  {
    return {};
  }
  const auto* classType = it->second->getClass().type();
  const auto events = classType->getEvents(ClassType::SearchMode::includeParents);
  std::string objPrefix = std::string(objectName) + ".";

  std::vector<Completion> result;
  for (const auto& ev: events)
  {
    auto evName = ev->getName();
    if (evName.size() < eventPrefix.size() || evName.compare(0, eventPrefix.size(), eventPrefix) != 0)
    {
      continue;
    }
    auto desc = std::string(ev->getDescription());
    result.push_back(Completion {objPrefix + std::string(evName), "event", std::move(desc), CompletionKind::value});
  }
  return result;
}

std::vector<Completion> Completer::completeListenArg(std::string_view prefix) const
{
  auto split = splitObjectMethod(prefix);
  if (split.has_value())
  {
    auto [objPath, evPrefix] = *split;
    if (objectsByName_.count(std::string(objPath)) != 0U)
    {
      // Object match: offer events and child objects sharing the prefix.
      auto candidates = completeEventName(objPath, evPrefix);
      auto pathCandidates = completeObjectOrCommand(prefix);
      for (auto& c: pathCandidates)
      {
        if (c.kind != CompletionKind::command)
        {
          candidates.push_back(std::move(c));
        }
      }
      return candidates;
    }
  }

  // Fall back to object-path traversal (no commands).
  auto candidates = completeObjectOrCommand(prefix);
  candidates.erase(
    std::remove_if(
      candidates.begin(), candidates.end(), [](const Completion& c) { return c.kind == CompletionKind::command; }),
    candidates.end());
  return candidates;
}

std::vector<Completion> Completer::completeWatchArg(std::string_view prefix) const
{
  // watch / unwatch only accept `<object>.<property>` targets, so completion must never surface
  // built-in commands or methods, only object-path segments leading to a property leaf.
  //
  // Three cases:
  //   1. Prefix is `<known-object>.<propPrefix>`: drill into that object's properties.
  //   2. Prefix exactly names a known object: also return properties (treat it like case 1
  //      with an empty prop prefix, the dot can be typed afterwards). Without this, users who
  //      Tab-accepted the object in a previous round would get a puzzling "no completions".
  //   3. Anything else: return object-path segments so the user can keep navigating. We
  //      deliberately reuse the segment code from completeObjectOrCommand but filter out any
  //      command entries it might include.
  auto split = splitObjectMethod(prefix);
  if (split.has_value())
  {
    auto [objPath, propPrefix] = *split;
    if (objectsByName_.count(std::string(objPath)) != 0U)
    {
      // Object match: offer both properties and child objects sharing the prefix.
      auto candidates = completePropertyName(objPath, propPrefix);
      auto pathCandidates = completeObjectOrCommand(prefix);
      for (auto& c: pathCandidates)
      {
        if (c.kind != CompletionKind::command)
        {
          candidates.push_back(std::move(c));
        }
      }
      return candidates;
    }
  }

  if (objectsByName_.count(std::string(prefix)) != 0U)
  {
    return completePropertyName(prefix, {});
  }

  // Fall back to object-path traversal only. Strip out any command candidates that
  // completeObjectOrCommand added; they're nonsense for a watch argument.
  auto candidates = completeObjectOrCommand(prefix);
  candidates.erase(
    std::remove_if(
      candidates.begin(), candidates.end(), [](const Completion& c) { return c.kind == CompletionKind::command; }),
    candidates.end());
  return candidates;
}

const std::vector<Completion>& Completer::getMethodCompletions(ConstTypeHandle<ClassType> classType) const
{
  auto it = classMethodCache_.find(classType);
  if (it != classMethodCache_.end())
  {
    return it->second;
  }

  // Build method completions for this class.
  // getMethods() returns standalone methods (declared with fn in STL).
  // Property getter/setter methods are only accessible via Property::getGetterMethod()/getSetterMethod().
  std::vector<Completion> completions;

  auto addMethod = [&completions](const Method& method)
  {
    auto retName = std::string(method.getReturnType()->getName());
    bool isVoid = (retName == "void");
    auto desc = std::string(method.getDescription());

    const std::string arrow = std::string(unicode::arrowRight) + " ";
    std::string display = isVoid ? "" : (arrow + retName);
    std::string detail;
    if (!isVoid)
    {
      detail = arrow + retName;
    }
    if (!desc.empty())
    {
      detail += detail.empty() ? desc : (" " + std::string(unicode::middleDot) + " " + desc);
    }
    Completion c {std::string(method.getName()), std::move(display), std::move(detail), CompletionKind::method};
    c.argCount = method.getArgs().size();
    completions.push_back(std::move(c));
  };

  // Standalone methods (declared with fn in STL)
  auto methods = classType->getMethods(ClassType::SearchMode::includeParents);
  for (const auto& method: methods)
  {
    addMethod(*method);
  }

  // Property getter/setter methods
  auto properties = classType->getProperties(ClassType::SearchMode::includeParents);
  for (const auto& prop: properties)
  {
    auto propName = std::string(prop->getName());
    auto typeName = std::string(prop->getType()->getName());

    auto propDesc = std::string(prop->getDescription());
    const std::string arrow = std::string(" ") + unicode::arrowRight + " ";
    std::string getterDetail = propDesc.empty() ? ("getter for " + propName + arrow + typeName) : propDesc;
    Completion getter {std::string(prop->getGetterMethod().getName()),
                       "get " + propName + arrow + typeName,
                       std::move(getterDetail),
                       CompletionKind::method};
    getter.argCount = 0U;  // property getters are always zero-arg
    completions.push_back(std::move(getter));

    auto category = prop->getCategory();
    if (category == PropertyCategory::dynamicRW)
    {
      Completion setter {std::string(prop->getSetterMethod().getName()),
                         "set " + propName + " : " + typeName,
                         "setter for " + propName + " : " + typeName,
                         CompletionKind::method};
      setter.argCount = 1U;  // property setters always take the new value
      completions.push_back(std::move(setter));
    }
  }

  // Virtual "print" method: displays all properties of the object
  if (!properties.empty())
  {
    Completion print {"print", "", "display all properties", CompletionKind::method};
    print.argCount = 0U;
    completions.push_back(std::move(print));
  }

  std::sort(completions.begin(), completions.end(), [](const auto& a, const auto& b) { return a.text < b.text; });

  auto [inserted, _] = classMethodCache_.emplace(classType, std::move(completions));
  return inserted->second;
}

std::vector<Completion> Completer::completeCdArg(std::string_view prefix) const
{
  std::vector<Completion> result;

  auto matches = [&](std::string_view text)
  { return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0; };

  if (matches(".."))
  {
    result.push_back({"..", ""});
  }
  if (matches("/"))
  {
    result.push_back({"/", ""});
  }
  if (matches("-"))
  {
    result.push_back({"-", ""});
  }

  for (const auto& child: childNames_)
  {
    if (matches(child))
    {
      result.push_back({child, "", {}, CompletionKind::path});
    }
  }

  for (const auto& q: queryNames_)
  {
    auto target = "@" + q;
    if (matches(target))
    {
      result.push_back({std::move(target), ""});
    }
  }

  // Offer available sources not already in the result (avoids duplicates with childNames).
  // Undotted prefix: session names. Dotted prefix: bus addresses.
  bool prefixHasDot = (prefix.find('.') != std::string_view::npos);
  for (const auto& src: availableSources_)
  {
    bool srcHasDot = (src.find('.') != std::string::npos);
    if (srcHasDot != prefixHasDot || !matches(src))
    {
      continue;
    }
    bool duplicate = false;
    for (const auto& existing: result)
    {
      if (existing.text == src)
      {
        duplicate = true;
        break;
      }
    }
    if (!duplicate)
    {
      result.push_back({src, "", {}, CompletionKind::path});
    }
  }

  std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.text < b.text; });
  return result;
}

std::vector<Completion> Completer::completeOpenArg(std::string_view prefix) const
{
  return filterByPrefix(availableSources_, prefix, "source");
}

std::vector<Completion> Completer::completeCloseArg(std::string_view prefix) const
{
  return filterByPrefix(openSources_, prefix, "source");
}

std::vector<Completion> Completer::completeQueryRmArg(std::string_view prefix) const
{
  return filterByPrefix(queryNames_, prefix, "query");
}

std::vector<Completion> Completer::completeLogArg(std::string_view prefix, Span<const std::string_view> tokens) const
{
  std::size_t completedTokens = prefix.empty() ? tokens.size() : tokens.size() - 1;

  if (completedTokens == 1)
  {
    if (std::string_view("level").compare(0, prefix.size(), prefix) == 0)
    {
      return {{"level", "set verbosity"}};
    }
    return {};
  }

  if (completedTokens == 2 && tokens[1] == "level")
  {
    std::vector<Completion> result;
    for (auto lvl: logLevels)
    {
      if (lvl.size() >= prefix.size() && lvl.compare(0, prefix.size(), prefix) == 0)
      {
        result.push_back({std::string(lvl), "level"});
      }
    }
    for (const auto& name: loggerNames_)
    {
      if (name.size() >= prefix.size() && name.compare(0, prefix.size(), prefix) == 0)
      {
        result.push_back({name, "logger"});
      }
    }
    std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.text < b.text; });
    return result;
  }

  if (completedTokens == 3 && tokens[1] == "level")
  {
    std::vector<Completion> result;
    for (auto lvl: logLevels)
    {
      if (lvl.size() >= prefix.size() && lvl.compare(0, prefix.size(), prefix) == 0)
      {
        result.push_back({std::string(lvl), "level"});
      }
    }
    return result;
  }

  return {};
}

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

std::optional<std::pair<std::string_view, std::string_view>> Completer::splitObjectMethod(std::string_view token)
{
  // Split on the last dot. Method names never contain dots, but object paths can
  // (e.g., "local.demo.slowLogger.getRate" -> object="local.demo.slowLogger", method="getRate")
  auto dot = token.rfind('.');
  if (dot == std::string_view::npos || dot == 0)
  {
    return std::nullopt;
  }
  return std::pair {token.substr(0, dot), token.substr(dot + 1)};
}

std::vector<Completion> Completer::filterByPrefix(Span<const std::string> items,
                                                  std::string_view prefix,
                                                  std::string_view annotation)
{
  std::vector<Completion> result;
  for (const auto& item: items)
  {
    if (item.size() >= prefix.size() && item.compare(0, prefix.size(), prefix) == 0)
    {
      result.push_back({item, std::string(annotation)});
    }
  }
  std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.text < b.text; });
  return result;
}

std::string Completer::commonPrefix(Span<const Completion> candidates)
{
  if (candidates.empty())
  {
    return {};
  }

  auto prefix = candidates[0].text;
  for (std::size_t i = 1; i < candidates.size(); ++i)
  {
    const auto& text = candidates[i].text;
    std::size_t len = std::min(prefix.size(), text.size());
    std::size_t j = 0;
    while (j < len && prefix[j] == text[j])
    {
      ++j;
    }
    prefix.resize(j);
    if (prefix.empty())
    {
      break;
    }
  }
  return prefix;
}

}  // namespace sen::components::term
