// === scope.cpp =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "scope.h"

// local
#include "unicode.h"

// std
#include <array>
#include <cstddef>
#include <utility>

namespace sen::components::term
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

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

  if (target == "-")
  {
    return navigateBack();
  }

  if (target == "..")
  {
    return navigateUp();
  }

  if (target.front() == '@')
  {
    auto name = target.substr(1);
    if (name.empty())
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
    if (slashPos != std::string_view::npos)
    {
      groupPart = busPart.substr(slashPos + 1);
      busPart = busPart.substr(0, slashPos);
    }

    if (sessionPart.empty() || busPart.empty())
    {
      return false;
    }

    savePrevious();
    kind_ = groupPart.empty() ? Kind::bus : Kind::group;
    session_ = sessionPart;
    bus_ = busPart;
    groupPath_ = groupPart;
    queryName_.clear();
    rebuildPath();
    return true;
  }

  if (kind_ == Kind::root)
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
    savePrevious();
    kind_ = Kind::group;
    if (groupPath_.empty())
    {
      groupPath_ = target;
    }
    else
    {
      groupPath_ += '/';
      groupPath_ += target;
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
      auto lastSlash = groupPath_.rfind('/');
      if (lastSlash == std::string::npos)
      {
        // Was one level deep; go back to bus.
        kind_ = Kind::bus;
        groupPath_.clear();
      }
      else
      {
        groupPath_ = groupPath_.substr(0, lastSlash);
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
  return objGroupPath.size() >= groupPath_.size() && objGroupPath.substr(0, groupPath_.size()) == groupPath_;
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

  // At group scope, strip the groupPath_ prefix
  if (kind_ == Kind::group && !groupPath_.empty() && result.size() >= groupPath_.size() &&
      result.compare(0, groupPath_.size(), groupPath_) == 0)
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
        path_ += '/';
        path_ += groupPath_;
      }
    }
  }
}

}  // namespace sen::components::term
