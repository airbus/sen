// === recorder.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "recorder.h"

// component
#include "data_point.h"
#include "database.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/object_mux.h"
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

namespace sen::components::influx
{

Recorder::Recorder(std::unique_ptr<Database> database,
                   const std::vector<std::string>& selections,
                   kernel::RunApi& runApi)
  : runApi_(runApi), database_(std::move(database)), mux_(std::make_unique<ObjectMux>())
{
  // build the list of interests
  std::vector<std::shared_ptr<Interest>> interests;
  interests.reserve(selections.size());
  for (const auto& query: selections)
  {
    interests.push_back(Interest::make(query, runApi_.getTypes()));
    if (!interests.back()->getBusCondition().has_value())
    {
      std::string err;
      err.append("selection does not define a source: ");
      err.append(query);
      throwRuntimeError(err);
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
    sources_.push_back(runApi_.getSource(elem));
  }

  // subscribe to the sources
  for (const auto& interest: interests)
  {
    // combine the source into the mux
    runApi_.getSource(asString(interest->getBusCondition().value()))->addSubscriber(interest, mux_.get(), true);
  }

  // listen to additions and removals
  trackedObjects_ = std::make_unique<ObjectList<Object>>();
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
      };
    });

  // hook the list to hold the tracked objects
  mux_->addListener(trackedObjects_.get(), true);
}

Recorder::~Recorder()
{
  guards_.clear();
  mux_.reset();
  trackedObjects_.reset();
  sources_.clear();
}

void Recorder::objectAdded(Object* object)
{
  subscribeToEvents(object);
  subscribeToPropertyChanges(object);
  sendCreation(runApi_.getTime(), object);
}

void Recorder::subscribeToEvents(Object* object)
{
  auto& guards = guards_[object->getId()];

  // react to events
  auto allEvents = object->getClass()->getEvents(ClassType::SearchMode::includeParents);
  for (const auto& elem: allEvents)
  {
    const auto* event = elem.get();

    auto guard =
      object->onEventUntyped(event,
                             EventCallback<VarList>(runApi_.getWorkQueue(),
                                                    [this, object, event](const auto& info, const auto& args) mutable
                                                    { eventProduced(object, event, info, args); }));
    // keep the guard
    guards.push_back(std::move(guard));
  }
}

void Recorder::subscribeToPropertyChanges(Object* object)
{
  auto& guards = guards_[object->getId()];

  auto allProps = object->getClass()->getProperties(ClassType::SearchMode::includeParents);
  for (const auto& elem: allProps)
  {
    const auto* prop = elem.get();

    // do not track static properties
    if (prop->getCategory() == PropertyCategory::staticRO || prop->getCategory() == PropertyCategory::staticRW)
    {
      continue;
    }

    auto guard =
      object->onPropertyChangedUntyped(prop,
                                       EventCallback<VarList>(runApi_.getWorkQueue(),
                                                              [this, object, prop](const auto& info, const auto& args)
                                                              {
                                                                std::ignore = args;
                                                                propChanged(object, prop, info);
                                                              }));

    // keep the guard
    guards.push_back(std::move(guard));
  }
}

void Recorder::objectRemoved(Object* object)
{
  // remove all connections to this object
  guards_.erase(object->getId());
  sendDeletion(runApi_.getTime(), object);
}

void Recorder::eventProduced(const Object* instance, const Event* event, const EventInfo& info, const VarList& args)
{
  std::string measurement;
  measurement.append(instance->getClass()->getQualifiedName());
  measurement.append(".");
  measurement.append(event->getName());

  // create a point for the event
  DataPoint point(std::move(measurement), info.creationTime);
  point.addTag("object", instance->getName());
  point.addTag("payload_type", "event");

  // create fields for the arguments
  const auto& metaArgs = event->getArgs();
  for (std::size_t i = 0U; i < metaArgs.size(); ++i)
  {
    varToFields(point, metaArgs[i].type.type(), args[i], info.creationTime, metaArgs[i].name);
  }
  database_->write(std::move(point));
}

void Recorder::propChanged(const Object* instance, const Property* prop, const EventInfo& info)
{
  std::string measurement;
  measurement.append(instance->getClass()->getQualifiedName());
  measurement.append(".");
  measurement.append(prop->getName());

  // create a point for the property
  DataPoint point(std::move(measurement), info.creationTime);
  point.addTag("object", instance->getName());
  point.addTag("payload_type", "property_change");

  // create fields for the value
  varToFields(point, prop->getType().type(), instance->getPropertyUntyped(prop), info.creationTime, "value");
  database_->write(std::move(point));
}

void Recorder::sendCreation(const TimeStamp& time, const Object* object)
{
  // create a point for the creation
  DataPoint point("created", time);
  point.addTag("object", object->getName());
  point.addTag("payload_type", "creation");

  auto allProperties = object->getClass()->getProperties(ClassType::SearchMode::includeParents);
  for (const auto& prop: allProperties)
  {
    varToFields(
      point, prop->getType().type(), object->getPropertyUntyped(prop.get()), time, std::string(prop->getName()));
  }
  database_->write(std::move(point));
}

void Recorder::sendDeletion(const TimeStamp& time, const Object* object) const
{
  DataPoint point("deleted", time);
  point.addTag("object", object->getName());
  point.addTag("payload_type", "deletion");
}

}  // namespace sen::components::influx
