// === completer.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "completer.h"

// component
#include "command_engine.h"
#include "log_router.h"
#include "object_store.h"
#include "parse_utils.h"
#include "scope.h"
#include "suggester.h"
#include "type_peel.h"
#include "unicode.h"
#include "util.h"

// sen
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/span.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/meta/unit.h"
#include "sen/core/obj/object.h"

// generated code
#include "stl/term.stl.h"

// std
#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

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
    std::set<std::string> childNames;

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

    for (const auto& [name, _count]: childCounts)
    {
      childNames.insert(name);
    }

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

  // Remembered, not read. The logger list is rebuilt when `log level <Tab>` asks for it: `listLoggers` goes
  // through `applyToAllLoggers`, which holds spdlog's process-global logger-map mutex for the whole walk.
  // Rebuilding it here would make every logger creation and every `registry::get` in the kernel contend
  // with term thirty times a second, for a list with one consumer.
  logRouter_ = &logRouter;
}

void Completer::refreshLoggerNames() const
{
  if (logRouter_ == nullptr)
  {
    return;
  }

  auto loggers = logRouter_->listLoggers();
  std::vector<std::string> lnames;
  lnames.reserve(loggers.size());
  for (auto& info: loggers)
  {
    lnames.push_back(std::move(info.name));
  }
  loggerNames_ = std::move(lnames);
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
    // Transitioned 0->1: the set keeps the order, at log N rather than a memmove of half the sequence.
    childNames_.insert(std::move(segmentStr));
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
    auto nItr = childNames_.find(std::string(segment));
    if (nItr != childNames_.end())
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

std::vector<Completion> Completer::completeFirstToken(std::string_view prefix) const
{
  auto dotSplit = splitObjectMethod(prefix);
  if (!dotSplit.has_value())
  {
    return completeObjectOrCommand(prefix);
  }

  auto [objPath, methodPrefix] = *dotSplit;
  auto itr = objectsByName_.find(std::string(objPath));
  if (itr == objectsByName_.end())
  {
    return completeObjectOrCommand(prefix);
  }

  // The token names an object, so offer its methods and any child object sharing the prefix. Commands
  // are dropped: a dot has already ruled them out.
  auto candidates = completeMethodName(objPath, methodPrefix);
  for (auto& candidate: completeObjectOrCommand(prefix))
  {
    if (candidate.kind != CompletionKind::command)
    {
      candidates.push_back(std::move(candidate));
    }
  }
  return candidates;
}

std::vector<Completion> Completer::completeQuerySelectArg(std::string_view prefix) const
{
  std::vector<Completion> candidates;
  if (prefix.empty() || prefix[0] == '*')
  {
    candidates.push_back({"*", "any type", {}, CompletionKind::value});
  }
  if (types_ != nullptr)
  {
    for (const auto& [name, type]: types_->getAll())
    {
      if (type->asClassType() != nullptr && startsWith(name, prefix))
      {
        candidates.push_back({name, "class", {}, CompletionKind::value});
      }
    }
  }
  // FROM as well, for a line where the type is already typed.
  if (startsWith("FROM", prefix))
  {
    candidates.push_back({"FROM", "", {}, CompletionKind::value});
  }
  return candidates;
}

std::vector<Completion> Completer::completeQueryFromArg(std::string_view prefix) const
{
  std::vector<Completion> candidates;
  if (prefix.empty() || prefix[0] == '*')
  {
    candidates.push_back({"*", "any bus", {}, CompletionKind::value});
  }
  std::set<std::string> seen;
  for (const auto& src: openSources_)
  {
    if (startsWith(src, prefix) && seen.insert(src).second)
    {
      candidates.push_back({src, "open", {}, CompletionKind::value});
    }
  }
  for (const auto& src: availableSources_)
  {
    if (startsWith(src, prefix) && seen.insert(src).second)
    {
      candidates.push_back({src, "available", {}, CompletionKind::value});
    }
  }
  if (startsWith("WHERE", prefix))
  {
    candidates.push_back({"WHERE", "", {}, CompletionKind::value});
  }
  return candidates;
}

std::vector<Completion> Completer::completeQueryArg(std::string_view prefix,
                                                    Span<const std::string_view> tokens,
                                                    std::size_t completedTokens,
                                                    bool endsWithSpace) const
{
  if (completedTokens == 1)
  {
    // First argument: the `rm` subcommand, or an existing query name for reference.
    auto candidates = completeQueryRmArg(prefix);
    if (startsWith("rm", prefix))
    {
      candidates.push_back({"rm", "remove a query", {}, CompletionKind::value});
    }
    return candidates;
  }
  if (completedTokens >= 2 && tokens[1] == "rm")
  {
    return completeQueryRmArg(prefix);
  }
  if (completedTokens == 2)
  {
    // `query <name>`: the selection starts with SELECT.
    if (startsWith("SELECT", prefix))
    {
      return {{"SELECT", "begin query", {}, CompletionKind::value}};
    }
    return {};
  }
  if (completedTokens < 3)
  {
    return {};
  }

  // Positional: SELECT <Type> FROM <bus> WHERE ... Which keyword has been passed decides what comes
  // next, so scan the tokens already complete.
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

  if (seenFrom && !seenWhere)
  {
    return completeQueryFromArg(prefix);
  }
  if (seenSelect && !seenFrom && !seenWhere)
  {
    return completeQuerySelectArg(prefix);
  }
  return {};
}

std::vector<Completion> Completer::completeThemeArg(std::string_view prefix)
{
  std::vector<Completion> candidates;
  const auto& enumType = *MetaTypeTrait<ThemeStyle>::meta();
  for (const auto& e: enumType.getEnums())
  {
    if (startsWith(e.name, prefix))
    {
      candidates.push_back({std::string(e.name), "", {}, CompletionKind::value});
    }
  }
  return candidates;
}

std::vector<Completion> Completer::completeInspectArg(std::string_view prefix) const
{
  auto candidates = completeObjectOrCommand(prefix);
  candidates.erase(
    std::remove_if(
      candidates.begin(), candidates.end(), [](const Completion& c) { return c.kind == CompletionKind::command; }),
    candidates.end());
  if (types_ == nullptr)
  {
    return candidates;
  }
  for (const auto& [name, type]: types_->getAll())
  {
    if (startsWith(name, prefix))
    {
      candidates.push_back({name, std::string(typeKindName(*type)), {}, CompletionKind::value});
    }
  }
  return candidates;
}

std::vector<Completion> Completer::completeUnitsArg(std::string_view prefix)
{
  std::vector<Completion> candidates;
  for (auto category: allUnitCategories)
  {
    auto cat = std::string(Unit::getCategoryString(category));
    if (startsWith(cat, prefix))
    {
      candidates.push_back({cat, "", {}, CompletionKind::value});
    }
  }
  return candidates;
}

CompletionResult Completer::complete(std::string_view input, int cursorPos) const
{
  auto upToCursor = input.substr(0, checkedConversion<std::size_t>(cursorPos));
  auto tokens = tokenize(upToCursor);

  // What is being completed is the token under the cursor, unless the line ends on a space, in which
  // case it is a new empty token.
  const bool endsWithSpace = !upToCursor.empty() && upToCursor.back() == ' ';
  std::string_view prefix;
  int replaceFrom = cursorPos;
  if (!endsWithSpace && !tokens.empty())
  {
    prefix = tokens.back();
    replaceFrom = cursorPos - checkedConversion<int>(prefix.size());
  }
  const std::size_t completedTokens = endsWithSpace ? tokens.size() : (tokens.empty() ? 0 : tokens.size() - 1);

  CompletionResult result;
  result.replaceFrom = replaceFrom;
  result.replaceTo = cursorPos;

  if (completedTokens == 0)
  {
    result.candidates = completeFirstToken(prefix);
  }
  else
  {
    const auto cmd = tokens[0];
    const bool firstArg = completedTokens <= 1;
    if ((cmd == "cd" || cmd == "ls") && firstArg)
    {
      result.candidates = completeCdArg(prefix);
    }
    else if (cmd == "open" && firstArg)
    {
      result.candidates = completeOpenArg(prefix);
    }
    else if (cmd == "close" && firstArg)
    {
      result.candidates = completeCloseArg(prefix);
    }
    else if (cmd == "query")
    {
      result.candidates = completeQueryArg(prefix, tokens, completedTokens, endsWithSpace);
    }
    else if (cmd == "help" && firstArg)
    {
      result.candidates = completeCommand(prefix);
    }
    else if (cmd == "theme" && firstArg)
    {
      result.candidates = completeThemeArg(prefix);
    }
    else if (cmd == "log")
    {
      result.candidates = completeLogArg(prefix, tokens);
    }
    else if ((cmd == "inspect" || cmd == "types") && firstArg)
    {
      result.candidates = completeInspectArg(prefix);
    }
    else if (cmd == "units" && firstArg)
    {
      result.candidates = completeUnitsArg(prefix);
    }
    else if ((cmd == "listen" || cmd == "unlisten") && firstArg)
    {
      result.candidates = completeListenArg(prefix);
      if (cmd == "unlisten" && startsWith("all", prefix))
      {
        result.candidates.push_back({"all", "stop all listeners", {}, CompletionKind::value});
      }
    }
  }

  // Two strategies can offer the same text, an object that is also a type name for instance.
  std::set<std::string> seen;
  auto duplicate = std::remove_if(result.candidates.begin(),
                                  result.candidates.end(),
                                  [&seen](const Completion& c) { return !seen.insert(c.text).second; });
  result.candidates.erase(duplicate, result.candidates.end());

  // One bound for every strategy, at the one place they all come through. Unbounded, Tab on an empty prefix
  // builds a candidate per object, three strings each, sorts them, and the renderer then re-scans the whole
  // list every frame for its column width although it draws about ten rows. On a bus with a hundred
  // thousand objects that is tens of megabytes and a visible freeze on the most ordinary gesture there is.
  constexpr std::size_t maxCandidates = 200;
  if (result.candidates.size() > maxCandidates)
  {
    result.candidates.resize(maxCandidates);
    result.truncated = true;
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
    if (startsWith(desc.name, prefix))
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
    if (!startsWith(name, prefix))
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
    if (startsWith(mc.text, methodPrefix))
    {
      Completion forwarded {objPrefix + mc.text, mc.display, mc.detail, mc.kind};
      forwarded.argCount = mc.argCount;
      result.push_back(std::move(forwarded));
    }
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
    if (!startsWith(evName, eventPrefix))
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
    bool isVoid = isVoidTypeName(retName);
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
    const std::string arrow = std::string(" ").append(unicode::arrowRight).append(" ");
    std::string getterDetail =
      propDesc.empty() ? std::string("getter for ").append(propName).append(arrow).append(typeName) : propDesc;
    Completion getter {std::string(prop->getGetterMethod().getName()),
                       std::string("get ").append(propName).append(arrow).append(typeName),
                       std::move(getterDetail),
                       CompletionKind::method};
    getter.argCount = 0U;  // property getters are always zero-arg
    completions.push_back(std::move(getter));

    auto category = prop->getCategory();
    if (category == PropertyCategory::dynamicRW)
    {
      Completion setter {std::string(prop->getSetterMethod().getName()),
                         std::string("set ").append(propName).append(" : ").append(typeName),
                         std::string("setter for ").append(propName).append(" : ").append(typeName),
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

  auto matches = [&](std::string_view text) { return startsWith(text, prefix); };

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
    // Never offer a target `cd` will read as something else. A segment carrying the scope separator
    // can only come from an object whose name contains one, which core permits; `cd` would normalise
    // it to a group path that matches nothing, and completion is the one place the user does not type
    // the string themselves. The object stays reachable by its dotted relative name, which is what
    // `listen` and method calls use.
    if (child.find('/') != std::string::npos)
    {
      continue;
    }
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
    if (startsWith("level", prefix))
    {
      return {{"level", "set verbosity"}};
    }
    return {};
  }

  if (completedTokens == 2 && tokens[1] == "level")
  {
    // Here, once, rather than on every cycle: this is the only place the list is read.
    refreshLoggerNames();

    std::vector<Completion> result;
    for (auto lvl: logLevels)
    {
      if (startsWith(lvl, prefix))
      {
        result.push_back({std::string(lvl), "level"});
      }
    }
    for (const auto& name: loggerNames_)
    {
      if (startsWith(name, prefix))
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
      if (startsWith(lvl, prefix))
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
  // `accept`: the user types "obj." and presses Tab to ask for the method list, which arrives here as an
  // empty method half. Execution asks the same function the other question.
  std::string_view object;
  std::string_view member;
  if (!splitObjectMember(token, TrailingSeparator::accept, object, member))
  {
    return std::nullopt;
  }
  return std::pair {object, member};
}

std::vector<Completion> Completer::filterByPrefix(Span<const std::string> items,
                                                  std::string_view prefix,
                                                  std::string_view annotation)
{
  std::vector<Completion> result;
  for (const auto& item: items)
  {
    if (startsWith(item, prefix))
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

    // Cut back to a codepoint boundary. Two candidates that share the lead byte of a multi-byte codepoint
    // and differ in the continuation byte, é and è for instance, give a prefix ending in a bare 0xC3.
    // writeCompletion would put that on the line and ftxui would drop it from the screen, leaving a byte in
    // the buffer that is not on the display and that Enter sends.
    prefix = truncateUtf8(prefix, j);
    if (prefix.empty())
    {
      break;
    }
  }
  return prefix;
}

}  // namespace sen::components::term
