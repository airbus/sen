// === scope.cpp =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "scope.h"

// component
#include "unicode.h"

// std
#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <utility>

namespace sen::components::term
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

/// Group nesting is typed and displayed with '/', and stored with '.' so that it compares equal
/// against the dotted group path taken from an object's local name. An empty segment would match
/// nothing at all, so a target carrying one is rejected rather than silently listing nothing.
bool normaliseGroupPart(std::string_view typed, std::string& stored)
{
  stored.assign(typed);
  std::replace(stored.begin(), stored.end(), '/', '.');
  return !stored.empty() && stored.front() != '.' && stored.back() != '.' && stored.find("..") == std::string::npos;
}

/// Find dot positions in a string_view without allocating.
/// Returns the number of dots found (up to maxDots). Positions stored in `dots`.
std::size_t findDots(std::string_view s, std::array<std::size_t, 8>& dots)
{
  std::size_t count = 0;
  for (std::size_t i = 0; i < s.size() && count < dots.size(); ++i)
  {
    if (s[i] == '.')
    {
      dots[count++] = i;
    }
  }
  return count;
}

/// Extract the Nth dot-separated segment (0-indexed). Returns empty if out of range.
std::string_view nthSegment(std::string_view s,
                            const std::array<std::size_t, 8>& dots,
                            std::size_t dotCount,
                            std::size_t index)
{
  std::size_t start = (index == 0) ? 0 : dots[index - 1] + 1;
  std::size_t end = (index < dotCount) ? dots[index] : s.size();
  if (start > s.size())
  {
    return {};
  }
  return s.substr(start, end - start);
}

/// Return everything from the Nth segment onward (0-indexed).
std::string_view fromSegment(std::string_view s, const std::array<std::size_t, 8>& dots, std::size_t index)
{
  if (index == 0)
  {
    return s;
  }
  if (index <= 8 && dots[index - 1] + 1 <= s.size())
  {
    return s.substr(dots[index - 1] + 1);
  }
  return {};
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Navigation
//--------------------------------------------------------------------------------------------------------------

bool Scope::navigate(std::string_view target)
{
  if (target.empty())
  {
    return false;
  }

  if (target == "/")
  {
    savePrevious();
    kind_ = Kind::root;
    session_.clear();
    bus_.clear();
    groupPath_.clear();
    queryName_.clear();
    rebuildPath();
    return true;
  }

  // The prompt shows the scope with a leading separator, so the path it prints is a target. A target
  // that begins with one is absolute: it names a session and a bus, never something under the scope
  // the user happens to be in.
  const bool absolute = target.size() > 1U && target.front() == '/';
  if (absolute)
  {
    target.remove_prefix(1);
  }

  // The relative forms are not paths, so a leading separator on one is a typo, not a meaning.
  if (target == "-")
  {
    return absolute ? false : navigateBack();
  }

  if (target == "..")
  {
    return absolute ? false : navigateUp();
  }

  if (target.front() == '@')
  {
    auto name = target.substr(1);
    if (name.empty() || absolute)
    {
      return false;
    }

    savePrevious();
    kind_ = Kind::query;
    queryName_ = name;
    session_.clear();
    bus_.clear();
    groupPath_.clear();
    rebuildPath();
    return true;
  }

  auto dotPos = target.find('.');
  if (dotPos != std::string_view::npos)
  {
    auto sessionPart = target.substr(0, dotPos);
    auto busPart = target.substr(dotPos + 1);

    auto slashPos = busPart.find('/');
    std::string_view groupPart;
    const bool hasGroupPart = slashPos != std::string_view::npos;
    if (hasGroupPart)
    {
      groupPart = busPart.substr(slashPos + 1);
      busPart = busPart.substr(0, slashPos);
    }

    // Only the group part may carry the separator. Checking it here and not after this branch is the
    // whole rule rather than half of it: `local/a.b` used to land on session "local/a", where nothing
    // matches and `ls` is silently empty -- the failure this change exists to remove.
    if (sessionPart.empty() || busPart.empty() || sessionPart.find('/') != std::string_view::npos ||
        busPart.find('/') != std::string_view::npos)
    {
      return false;
    }

    // Normalise whenever a separator was present, so `local.main/` is refused for the same reason
    // `a/` is rather than silently dropped.
    std::string group;
    if (hasGroupPart && !normaliseGroupPart(groupPart, group))
    {
      return false;
    }

    savePrevious();
    kind_ = group.empty() ? Kind::bus : Kind::group;
    session_ = sessionPart;
    bus_ = busPart;
    groupPath_ = std::move(group);
    queryName_.clear();
    rebuildPath();
    return true;
  }

  // Only a group target may carry the separator, so "local/a" is not a session named "local/a".
  if ((kind_ == Kind::root || kind_ == Kind::session || absolute) && target.find('/') != std::string_view::npos)
  {
    return false;
  }

  // An absolute target is resolved against the root wherever the user is standing, so an undotted one
  // names a session. Tying that to the current scope instead refused `/local` from every scope but the
  // root -- including from `/local` itself, whose own prompt prints exactly that string.
  if (kind_ == Kind::root || absolute)
  {
    savePrevious();
    kind_ = Kind::session;
    session_ = target;
    bus_.clear();
    groupPath_.clear();
    queryName_.clear();
    rebuildPath();
    return true;
  }

  if (kind_ == Kind::session)
  {
    savePrevious();
    kind_ = Kind::bus;
    bus_ = target;
    groupPath_.clear();
    queryName_.clear();
    rebuildPath();
    return true;
  }

  if (kind_ == Kind::bus || kind_ == Kind::group)
  {
    std::string segment;
    if (!normaliseGroupPart(target, segment))
    {
      return false;
    }

    savePrevious();
    kind_ = Kind::group;
    if (groupPath_.empty())
    {
      groupPath_ = std::move(segment);
    }
    else
    {
      groupPath_ += '.';
      groupPath_ += segment;
    }
    queryName_.clear();
    rebuildPath();
    return true;
  }

  return false;
}

bool Scope::navigateUp()
{
  switch (kind_)
  {
    case Kind::root:
      return false;

    case Kind::query:
      savePrevious();
      kind_ = Kind::root;
      queryName_.clear();
      break;

    case Kind::session:
      savePrevious();
      kind_ = Kind::root;
      session_.clear();
      break;

    case Kind::bus:
      savePrevious();
      kind_ = Kind::session;
      bus_.clear();
      break;

    case Kind::group:
    {
      savePrevious();
      auto lastSeparator = groupPath_.rfind('.');
      if (lastSeparator == std::string::npos)
      {
        // Was one level deep; go back to bus.
        kind_ = Kind::bus;
        groupPath_.clear();
      }
      else
      {
        groupPath_ = groupPath_.substr(0, lastSeparator);
      }
      break;
    }
  }

  rebuildPath();
  return true;
}

bool Scope::navigateBack()
{
  if (!hasPrevious_)
  {
    return false;
  }

  std::swap(kind_, prevKind_);
  std::swap(session_, prevSession_);
  std::swap(bus_, prevBus_);
  std::swap(groupPath_, prevGroupPath_);
  std::swap(queryName_, prevQueryName_);

  rebuildPath();
  return true;
}

//--------------------------------------------------------------------------------------------------------------
// Accessors
//--------------------------------------------------------------------------------------------------------------

Scope::Kind Scope::getKind() const noexcept { return kind_; }
std::string_view Scope::getSession() const noexcept { return session_; }
std::string_view Scope::getBus() const noexcept { return bus_; }
std::string_view Scope::getGroupPath() const noexcept { return groupPath_; }
std::string_view Scope::getQueryName() const noexcept { return queryName_; }
std::string_view Scope::getPath() const noexcept { return path_; }

std::string Scope::getBusAddress() const
{
  if (session_.empty() || bus_.empty())
  {
    return {};
  }
  return session_ + "." + bus_;
}

std::string Scope::makePrompt() const
{
  std::string result = "sen:";
  result += path_;
  result += unicode::promptSymbol;
  result += ' ';
  return result;
}

namespace
{

/// True when `candidate` is `prefix` itself or a path nested under it. A plain prefix test would let
/// group "a" match group "ab", so the character after the prefix has to be a separator.
bool isGroupPrefix(std::string_view prefix, std::string_view candidate)
{
  if (candidate.size() < prefix.size() || candidate.compare(0, prefix.size(), prefix) != 0)
  {
    return false;
  }
  return candidate.size() == prefix.size() || candidate[prefix.size()] == '.';
}

}  // namespace

bool Scope::contains(std::string_view objectLocalName) const
{
  // Object local name format: component.session.bus[.rest...]
  // Require at least 3 dots (4 segments) for a valid object name.
  std::array<std::size_t, 8> dots {};
  auto dotCount = findDots(objectLocalName, dots);
  if (dotCount < 3)
  {
    return false;
  }

  if (kind_ == Kind::root || kind_ == Kind::query)
  {
    return true;
  }

  if (nthSegment(objectLocalName, dots, dotCount, 1) != session_)
  {
    return false;
  }

  if (kind_ == Kind::session)
  {
    return true;
  }

  if (nthSegment(objectLocalName, dots, dotCount, 2) != bus_)
  {
    return false;
  }

  if (kind_ == Kind::bus)
  {
    return true;
  }

  // Group scope: check that the path after session.bus starts with groupPath_
  auto afterBus = fromSegment(objectLocalName, dots, 3);
  // Strip the last segment (the object name itself) to get the group path
  auto lastDot = afterBus.rfind('.');
  auto objGroupPath = (lastDot != std::string_view::npos) ? afterBus.substr(0, lastDot) : std::string_view {};
  return isGroupPrefix(groupPath_, objGroupPath);
}

std::string Scope::relativeName(std::string_view objectLocalName) const
{
  // Object local name format: component.session.bus[.rest...]
  std::array<std::size_t, 8> dots {};
  auto dotCount = findDots(objectLocalName, dots);
  if (dotCount < 3)
  {
    return std::string(objectLocalName);
  }

  // Start segment: skip component prefix + scope depth
  //   root/query: 1 (show from session onward)
  //   session:    2 (show from bus onward)
  //   bus/group:  3 (show object path only)
  std::size_t startSegment = 1;
  if (kind_ == Kind::session)
  {
    startSegment = 2;
  }
  else if (kind_ == Kind::bus || kind_ == Kind::group)
  {
    startSegment = 3;
  }

  auto result = std::string(fromSegment(objectLocalName, dots, startSegment));

  // At group scope, strip the group prefix. The boundary check matters: without it, group "a"
  // strips one character off an object in group "ab" and yields "b.x.obj".
  if (kind_ == Kind::group && !groupPath_.empty() && isGroupPrefix(groupPath_, result))
  {
    result = result.substr(groupPath_.size());
    if (!result.empty() && result.front() == '.')
    {
      result = result.substr(1);
    }
  }

  return result;
}

//--------------------------------------------------------------------------------------------------------------
// Internal
//--------------------------------------------------------------------------------------------------------------

void Scope::savePrevious()
{
  prevKind_ = kind_;
  prevSession_ = session_;
  prevBus_ = bus_;
  prevGroupPath_ = groupPath_;
  prevQueryName_ = queryName_;
  hasPrevious_ = true;
}

void Scope::rebuildPath()
{
  if (kind_ == Kind::query)
  {
    path_ = "@" + queryName_;
    return;
  }

  path_ = "/";

  if (!session_.empty())
  {
    path_ += session_;
    if (!bus_.empty())
    {
      path_ += '.';
      path_ += bus_;
      if (!groupPath_.empty())
      {
        std::string displayed = groupPath_;
        std::replace(displayed.begin(), displayed.end(), '.', '/');
        path_ += '/';
        path_ += displayed;
      }
    }
  }
}

}  // namespace sen::components::term
