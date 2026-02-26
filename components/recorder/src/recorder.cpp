// === recorder.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "recorder.h"

// component
#include "util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/object_mux.h"
#include "sen/db/output.h"

// generated code
#include "stl/sen/components/recorder/configuration.stl.h"
#include "stl/sen/components/recorder/recorder.stl.h"
#include "stl/sen/db/basic_types.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

// sen
#include "sen/kernel/component_api.h"

// std
#include <cstddef>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace recorder
{

RecorderImpl::RecorderImpl(const std::string& name, const sen::VarMap& args)
  : RecorderBase(name, args), mux_(std::make_unique<sen::ObjectMux>())
{
  setNextState(sen::components::recorder::RecorderState::stopped);
}

RecorderImpl::RecorderImpl(const sen::components::recorder::RecordingSettings& settings, sen::kernel::RunApi& api)
  : RecorderBase(settings.name, settings), mux_(std::make_unique<sen::ObjectMux>()), api_ {&api}
{
  setNextState(sen::components::recorder::RecorderState::stopped);
}

void RecorderImpl::update(sen::kernel::RunApi& runApi)
{
  if (SEN_UNLIKELY(api_ == nullptr))
  {
    api_ = &runApi;
    // auto start if needed
    if (getSettings().autoStart)
    {
      start();
    }
  }

  if (getState() != sen::components::recorder::RecorderState::recording)
  {
    return;
  }

  const auto period = getSettings().keyframePeriod;

  if (period.get() != 0U && runApi.getTime() - lastKeyframeTime_ > period)
  {
    sendKeyframe(runApi.getTime());
  }
}

void RecorderImpl::registered(sen::kernel::RegistrationApi& api)
{
  std::ignore = api;

  // if api exists at the time of registering, autostart the component if specified.
  if (SEN_LIKELY(api_ != nullptr))
  {
    if (getSettings().autoStart)
    {
      start();
    }
  }
}

void RecorderImpl::startImpl()
{
  if (getState() != sen::components::recorder::RecorderState::stopped)
  {
    sen::components::recorder::getLogger()->info("cannot start the recorder {} because it is not stopped", getName());
    return;
  }

  const auto& settings = getSettings();

  sen::db::OutSettings outSettings {};
  outSettings.folder = settings.folder;
  outSettings.indexKeyframes = settings.indexKeyframes;
  outSettings.name = settings.name;

  out_ = std::make_shared<sen::db::Output>(std::move(outSettings),
                                           [this]()
                                           {
                                             sen::components::recorder::getLogger()->error("error detected");
                                             setNextState(sen::components::recorder::RecorderState::error);
                                           });

  // build the list of interests
  std::vector<std::shared_ptr<sen::Interest>> interests;
  interests.reserve(settings.selections.size());
  for (const auto& query: settings.selections)
  {
    interests.push_back(sen::Interest::make(query, api_->getTypes()));
    if (!interests.back()->getBusCondition().has_value())
    {
      std::string err;
      err.append("selection does not define a source: ");
      err.append(query);
      sen::throwRuntimeError(err);
    }
  }

  // find the unique sources
  std::set<std::string, std::less<>> uniqueSources;
  for (const auto& interest: interests)
  {
    uniqueSources.insert(asString(interest->getBusCondition().value()));
  }

  // find the sources and store a reference
  sources_.reserve(uniqueSources.size());
  for (const auto& elem: uniqueSources)
  {
    sources_.push_back(api_->getSource(elem));
  }

  // subscribe to the sources
  for (const auto& interest: interests)
  {
    // combine the source into the mux
    api_->getSource(asString(interest->getBusCondition().value()))->addSubscriber(interest, mux_.get(), true);
  }

  // react to new additions and removals before setting callbacks as recorder may be instanced mid-execution
  additionsAndRemovalsEnabled_ = true;

  // listen to additions and removals
  trackedObjects_ = std::make_unique<sen::ObjectList<Object>>();
  std::ignore = trackedObjects_->onAdded(
    [this](const auto& iterators)
    {
      for (auto itr = iterators.untypedBegin; itr != iterators.untypedEnd; ++itr)
      {
        objectAdded((*itr).get());
      }
    });
  std::ignore = trackedObjects_->onRemoved(
    [this](const auto& iterators)
    {
      for (auto itr = iterators.untypedBegin; itr != iterators.untypedEnd; ++itr)
      {
        objectRemoved((*itr).get());
      }
    });

  // hook the list to hold the tracked objects
  mux_->addListener(trackedObjects_.get(), true);

  // now we are recording
  setNextState(sen::components::recorder::RecorderState::recording);

  sen::components::recorder::getLogger()->debug("{} started", getName());
}

void RecorderImpl::stopImpl()
{
  if (getState() == sen::components::recorder::RecorderState::recording)
  {
    additionsAndRemovalsEnabled_ = false;
    guards_.clear();
    out_.reset();
    mux_.reset();
    trackedObjects_.reset();
    sources_.clear();

    sen::components::recorder::getLogger()->debug("{} stopped", getName());
  }
}

sen::db::OutStats RecorderImpl::fetchStatsImpl()
{
  if (out_)
  {
    return out_->fetchStats();
  }
  return {};
}

void RecorderImpl::objectAdded(Object* object)
{
  if (!additionsAndRemovalsEnabled_)
  {
    return;
  }

  subscribeToEvents(object);
  subscribeToPropertyChanges(object);
  sendCreation(api_->getTime(), object);

  sen::components::recorder::getLogger()->debug("{} added object {}", getName(), object->getName());
}

void RecorderImpl::subscribeToEvents(Object* object)
{
  auto& guards = guards_[object->getId()];

  // react to events
  auto allEvents = object->getClass()->getEvents(sen::ClassType::SearchMode::includeParents);
  for (const auto& elem: allEvents)
  {
    const auto* event = elem.get();

    auto guard = object->onEventUntyped(
      event,
      sen::EventCallback<sen::VarList>(this,
                                       [this, object, event](const auto& info, const auto& args) mutable
                                       { eventProduced(object, event, info, args); }));
    // keep the guard
    guards.push_back(std::move(guard));
  }
}

void RecorderImpl::subscribeToPropertyChanges(Object* object)
{
  auto& guards = guards_[object->getId()];

  auto allProps = object->getClass()->getProperties(sen::ClassType::SearchMode::includeParents);
  for (const auto& elem: allProps)
  {
    const auto* prop = elem.get();

    // do not track static properties
    if (prop->getCategory() == sen::PropertyCategory::staticRO ||
        prop->getCategory() == sen::PropertyCategory::staticRW)
    {
      continue;
    }

    auto guard = object->onPropertyChangedUntyped(
      prop,
      sen::EventCallback<sen::VarList>(this,
                                       [this, object, prop](const auto& info, const auto& args)
                                       {
                                         std::ignore = args;
                                         propChanged(object, prop, info);
                                       }));

    // keep the guard
    guards.push_back(std::move(guard));
  }
}

void RecorderImpl::objectRemoved(Object* object)
{
  if (!additionsAndRemovalsEnabled_)
  {
    return;
  }

  // remove all connections to this object
  guards_.erase(object->getId());

  sendDeletion(api_->getTime(), object);

  sen::components::recorder::getLogger()->debug("{} removed object {}", getName(), object->getName());
}

void RecorderImpl::eventProduced(const Object* instance,
                                 const sen::Event* event,
                                 const sen::EventInfo& info,
                                 const sen::VarList& args) const
{
  if (out_ == nullptr)
  {
    return;
  }

  const auto& metaArgs = event->getArgs();

  sen::kernel::Buffer buffer;
  if (!metaArgs.empty())
  {
    sen::ResizableBufferWriter writer(buffer);
    sen::OutputStream out(writer);

    for (std::size_t i = 0; i < args.size(); ++i)
    {
      sen::impl::writeToStream(args[i], out, *metaArgs[i].type);
    }
  }
  out_->event(info.creationTime, instance->getId(), event->getId(), std::move(buffer));
}

void RecorderImpl::propChanged(const Object* instance, const sen::Property* prop, const sen::EventInfo& info) const
{
  if (out_ == nullptr)
  {
    return;
  }

  sen::kernel::Buffer buffer;
  {
    sen::ResizableBufferWriter writer(buffer);
    sen::OutputStream out(writer);

    auto value = instance->getPropertyUntyped(prop);
    sen::impl::writeToStream(value, out, *prop->getType());
  }

  out_->propertyChange(info.creationTime, instance->getId(), prop->getId(), std::move(buffer));
}

void RecorderImpl::sendKeyframe(const sen::TimeStamp& time)
{
  if (out_ == nullptr)
  {
    return;
  }

  const auto& untyped = trackedObjects_->getUntypedObjects();

  sen::db::ObjectInfoList objects;
  objects.reserve(untyped.size());
  for (const auto& elem: untyped)
  {
    objects.emplace_back(getObjectInfo(elem.get()));
  }

  out_->keyframe(time, objects);
  lastKeyframeTime_ = time;

  sen::components::recorder::getLogger()->debug("{} keyframe sent", getName());
}

void RecorderImpl::sendCreation(const sen::TimeStamp& time, const Object* object) const
{
  if (out_ == nullptr)
  {
    return;
  }

  out_->creation(time, getObjectInfo(object), getSettings().indexObjects);
}

void RecorderImpl::sendDeletion(const sen::TimeStamp& time, const Object* object) const
{
  if (out_ == nullptr)
  {
    return;
  }

  out_->deletion(time, object->getId());
}

sen::db::ObjectInfo RecorderImpl::getObjectInfo(const Object* instance) const
{
  sen::db::ObjectInfo objectInfo {};
  objectInfo.instance = instance;

  if (auto nameComponents = ::sen::impl::split(instance->getLocalName(), '.'); nameComponents.size() >= 4)
  {
    objectInfo.session = nameComponents.at(1U);
    objectInfo.bus = nameComponents.at(2U);
  }

  return objectInfo;
}

SEN_EXPORT_CLASS(RecorderImpl)

}  // namespace recorder
