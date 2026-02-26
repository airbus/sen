// === replayer.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "replayer.h"

#include "replay.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/obj/object_source.h"
#include "sen/db/input.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/sen/components/replayer/replayer.stl.h"

// std
#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace sen::components::replayer
{

ReplayerImpl::ReplayerImpl(std::string name,
                           bool autoPlay,
                           std::shared_ptr<ObjectSource> controlBus,
                           kernel::RunApi& api)
  : ReplayerBase(std::move(name), autoPlay), controlBus_(std::move(controlBus)), api_(api)
{
}

ReplayerImpl::~ReplayerImpl()
{
  try
  {
    doCloseAll();
  }
  catch (...)
  {
    // swallow exceptions here
  }
}

void ReplayerImpl::openImpl(const std::string& name, const std::string& path)
{
  // complain if repeated name
  if (findByName(name) != replays_.end())
  {
    std::string err;
    err.append("there is already a replay named ");
    err.append(name);
    throwRuntimeError(err);
  }

  // complain if already open
  if (findByPath(path) != replays_.end())
  {
    std::string err;
    err.append("there is already a replay named ");
    err.append(name);
    err.append(" using the same archive");
    throwRuntimeError(err);
  }

  // create and publish the replay
  auto input = std::make_unique<db::Input>(path, api_.getTypes());
  replays_.push_back(std::make_shared<ReplayImpl>(name, path, std::move(input), api_));
  controlBus_->add(replays_.back());

  if (getAutoPlay())
  {
    replays_.back()->play();
  }
}

void ReplayerImpl::closeImpl(const std::string& name)
{
  auto itr = findByName(name);

  // complain if not found
  if (itr == replays_.end())
  {
    std::string err;
    err.append("there's no replay named ");
    err.append(name);
    err.append(" being managed by the replayer ");
    err.append(getName());
    throwRuntimeError(err);
  }

  controlBus_->remove(*itr);
  replays_.erase(itr);
}

void ReplayerImpl::closeAllImpl() { doCloseAll(); }

void ReplayerImpl::doCloseAll()
{
  controlBus_->remove(replays_);
  replays_.clear();
}

typename ReplayerImpl::ReplayList::iterator ReplayerImpl::findByName(const std::string& name)
{
  return std::find_if(replays_.begin(), replays_.end(), [&](const auto& replay) { return replay->getName() == name; });
}

typename ReplayerImpl::ReplayList::iterator ReplayerImpl::findByPath(const std::string& path)
{
  return std::find_if(
    replays_.begin(), replays_.end(), [&](const auto& replay) { return replay->getArchivePath() == path; });
}

}  // namespace sen::components::replayer
