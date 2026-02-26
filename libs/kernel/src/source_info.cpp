// === source_info.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/source_info.h"

#include "sen/core/obj/object_source.h"

// std
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sen::kernel
{

//--------------------------------------------------------------------------------------------------------------
// SourceInfo
//--------------------------------------------------------------------------------------------------------------

SourceInfo::SourceInfo(std::string name): name_(std::move(name)) {}

const std::string& SourceInfo::getName() const noexcept { return name_; }

std::vector<std::string> SourceInfo::getDetectedSources() const
{
  Lock lock(sourcesMutex_);
  return currentSources_;
}

void SourceInfo::onSourceDetected(const std::function<void(const std::string&)>& callback)
{
  Lock lock(sourcesMutex_);
  detectedCallback_ = callback;

  if (detectedCallback_)
  {
    for (const auto& elem: currentSources_)
    {
      detectedCallback_(elem);
    }
  }
}

void SourceInfo::onSourceUndetected(const std::function<void(const std::string&)>& callback)
{
  Lock lock(sourcesMutex_);
  undetectedCallback_ = callback;
}

void SourceInfo::addSource(const std::string& name)
{
  Lock lock(sourcesMutex_);

  if (std::find(currentSources_.begin(), currentSources_.end(), name) == currentSources_.end())
  {
    // add to additions
    if (std::find(pendingAdditions_.begin(), pendingAdditions_.end(), name) == pendingAdditions_.end())
    {
      pendingAdditions_.push_back(name);
    }

    // remove from removals
    remove(name, pendingRemovals_);
  }
}

void SourceInfo::drainInputs()
{
  Lock lock(sourcesMutex_);

  std::vector<std::string> additions;
  std::swap(additions, pendingAdditions_);

  std::vector<std::string> removals;
  std::swap(removals, pendingRemovals_);

  // add additions
  for (const auto& elem: additions)
  {
    currentSources_.push_back(elem);
  }

  // remove removals
  for (const auto& elem: removals)
  {
    remove(elem, currentSources_);
  }

  // call back for removals
  if (undetectedCallback_ && !removals.empty())
  {
    for (const auto& elem: removals)
    {
      undetectedCallback_(elem);
    }
  }

  // call back for additions
  if (detectedCallback_ && !additions.empty())
  {
    for (const auto& elem: additions)
    {
      detectedCallback_(elem);
    }
  }
}

void SourceInfo::lock() { sourcesMutex_.lock(); }

void SourceInfo::unlock() { sourcesMutex_.unlock(); }

void SourceInfo::removeSource(const std::string& name)
{
  Lock lock(sourcesMutex_);

  auto itr = std::find(currentSources_.begin(), currentSources_.end(), name);
  if (itr != currentSources_.end())
  {
    // add to removals
    if (std::find(pendingRemovals_.begin(), pendingRemovals_.end(), name) == pendingRemovals_.end())
    {
      pendingRemovals_.push_back(name);
    }

    // remove from additions
    remove(name, pendingAdditions_);
  }

  // removing something that was never added?
}

void SourceInfo::remove(const std::string& elem, std::vector<std::string>& vec) const
{
  vec.erase(std::remove(vec.begin(), vec.end(), elem), vec.end());
}

//--------------------------------------------------------------------------------------------------------------
// SessionsDiscoverer
//--------------------------------------------------------------------------------------------------------------

SessionsDiscoverer::SessionsDiscoverer(sen::kernel::impl::Runner* owner): SourceInfo(""), owner_(owner) {}

void SessionsDiscoverer::drainInputs()
{
  lock();
  SourceInfo::drainInputs();

  validChildren_.reserve(children_.size());
  for (const auto& elem: children_)
  {
    auto validChild = elem.lock();
    if (validChild)
    {
      validChild->drainInputs();
      validChildren_.push_back(elem);
    }
  }

  std::swap(children_, validChildren_);
  validChildren_.clear();

  unlock();
}

void SessionsDiscoverer::sessionAvailable(const std::string& name) { addSource(name); }

void SessionsDiscoverer::sessionUnavailable(const std::string& name) { removeSource(name); }

//--------------------------------------------------------------------------------------------------------------
// SessionInfoProvider
//--------------------------------------------------------------------------------------------------------------

SessionInfoProvider::SessionInfoProvider(std::string name, std::shared_ptr<impl::Session> session)
  : SourceInfo(std::move(name)), session_(std::move(session))
{
}

}  // namespace sen::kernel
