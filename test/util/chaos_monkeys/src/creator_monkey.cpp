// === creator_monkey.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "creator_monkey.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/numbers.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/kernel/component_api.h"

// spdlog
#include "stl/creator_monkeys/chaos_monkeys.stl.h"

#include <spdlog/spdlog.h>

// std
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace creator_monkeys
{

constexpr uint8_t noneTaskDuration = 20U;
constexpr uint8_t testMethodTaskDuration = 40U;
constexpr uint8_t testMethod2TaskDuration = 60U;
constexpr uint8_t testEventTaskDuration = 100U;

//--------------------------------------------------------------------------------------------------------------
// MonkeyTestClassImpl
//--------------------------------------------------------------------------------------------------------------
void MonkeyTestClassImpl::registered(sen::kernel::RegistrationApi& api)
{
  std::ignore = api;

  setNextCurrentTask({Activity::none, sen::TimeStamp {0}});
  setNextFinishedTasks(0);
  setNextTestedEvents(0);

  // set up callback to update counter of finished tasks
  auto cbTaskChange = [us = shared_from_this(), this]() { setNextFinishedTasks(getFinishedTasks() + 1); };
  onCurrentTaskChanged({this, std::move(cbTaskChange)}).keep();

  // set up callback to update counter of tested events
  auto cbEmitEvent = [us = shared_from_this(), this](const std::string& /*str*/, const sen::TimeStamp /*time*/)
  { setNextTestedEvents(getTestedEvents() + 1); };
  onTestEvent({this, std::move(cbEmitEvent)}).keep();
}

void MonkeyTestClassImpl::update(sen::kernel::RunApi& runApi) { doTask(getCurrentTask(), runApi.getTime()); }

i32 MonkeyTestClassImpl::testMethodImpl(const i32 arg1, const std::string& arg2)
{
  std::ignore = arg2;

  return arg1;
}

void MonkeyTestClassImpl::emitTestEventImpl() { testEvent("monkey class test event since ", getCurrentTask().since); }

void MonkeyTestClassImpl::doTask(const Task task, const sen::TimeStamp time)
{
  switch (task.currentActivity)
  {
    case Activity::none:
      if (time - task.since > noneTaskDuration)
      {
        testMethodImpl(-10, "hello");
        setNextCurrentTask({Activity::testMethod, time});
      }
      break;

    case Activity::testMethod:
      if (time - task.since > testMethodTaskDuration)
      {
        emitTestEventImpl();
        setNextCurrentTask({Activity::testEvent, time});
      }
      break;

    case Activity::testMethod2:
      [[fallthrough]];

    case Activity::testEvent:
      if (time - task.since > testEventTaskDuration)
      {
        setNextCurrentTask({Activity::none, time});
      }
      break;

    default:
      sen::throwRuntimeError("invalid activity");
  }
}

SEN_EXPORT_CLASS(MonkeyTestClassImpl)

//--------------------------------------------------------------------------------------------------------------
// MonkeyTestClass2Impl
//--------------------------------------------------------------------------------------------------------------
void MonkeyTestClass2Impl::registered(sen::kernel::RegistrationApi& api)
{
  std::ignore = api;

  setNextCurrentTask({Activity::none, sen::TimeStamp {0}});
  setNextFinishedTasks(0);
  setNextTestedEvents(0);

  // set up callback to update counter of finished tasks
  auto cbTaskChange = [&]() { setNextFinishedTasks(getFinishedTasks() + 1); };
  onCurrentTaskChanged({this, std::move(cbTaskChange)}).keep();

  // set up callback to update counter of tested events
  auto cbEmitEvent = [&](const std::string& /*str*/, const sen::TimeStamp /*time*/)
  { setNextTestedEvents(getTestedEvents() + 1); };
  onTestEvent({this, std::move(cbEmitEvent)}).keep();
}

void MonkeyTestClass2Impl::update(sen::kernel::RunApi& runApi) { doTask(getCurrentTask(), runApi.getTime()); }

f64 MonkeyTestClass2Impl::testMethodImpl(const f64 arg1) { return arg1; }

void MonkeyTestClass2Impl::testMethod2Impl(const u16 arg1) { std::ignore = arg1; }

void MonkeyTestClass2Impl::emitTestEventImpl()
{
  testEvent("monkey class 2 test event since ", getCurrentTask().since);
}

void MonkeyTestClass2Impl::doTask(const Task task, const sen::TimeStamp time)
{
  switch (task.currentActivity)
  {
    case Activity::none:
      if (time - task.since > noneTaskDuration)
      {
        testMethodImpl(3.14);
        setNextCurrentTask({Activity::testMethod, time});
      }
      break;

    case Activity::testMethod:
      if (time - task.since > testMethodTaskDuration)
      {
        testMethod2Impl(8U);
        setNextCurrentTask({Activity::testMethod2, time});
      }
      break;

    case Activity::testMethod2:
      if (time - task.since > testMethod2TaskDuration)
      {
        emitTestEventImpl();
        setNextCurrentTask({Activity::testEvent, time});
      }
      break;

    case Activity::testEvent:
      if (time - task.since > testEventTaskDuration)
      {
        setNextCurrentTask({Activity::none, time});
      }
      break;

    default:
      sen::throwRuntimeError("invalid activity");
  }
}

SEN_EXPORT_CLASS(MonkeyTestClass2Impl)

//--------------------------------------------------------------------------------------------------------------
// CreatorMonkeyImpl
//--------------------------------------------------------------------------------------------------------------
void CreatorMonkeyImpl::registered(sen::kernel::RegistrationApi& api)
{
  instancesPrefix_ = getName() + ".obj";
  bus_ = api.getSource(getTargetBus());

  for (const auto& className: getObjectsToCreate())
  {
    registerObject(api, className);
    SPDLOG_INFO("[Creator Monkeys] Creator Monkey registered class {} on bus {}", className, getTargetBus());
  }
}

void CreatorMonkeyImpl::registerObject(sen::kernel::RegistrationApi& api, const std::string className)
{
  auto type = api.getTypes().get(className);
  if (!type)
  {
    std::string err;
    err.append("could not find type '");
    err.append(className);
    err.append("'");
    sen::throwRuntimeError(err);
  }

  auto meta = sen::dynamicTypeHandleCast<const sen::ClassType>(type.value());
  if (!meta.has_value())
  {
    std::string err;
    err.append("type '");
    err.append(className);
    err.append("' is not a class");
    sen::throwRuntimeError(err);
  }

  if (!meta.value()->hasProxyMakers())
  {
    std::string err;
    err.append("class '");
    err.append(className);
    err.append("' cannot instantiate native objects");
    sen::throwRuntimeError(err);
  }
  meta_.push_back(std::move(meta).value());
}

void CreatorMonkeyImpl::update(sen::kernel::RunApi& runApi)
{
  std::ignore = runApi;

  if (frameCount_ == 0)
  {
    startTime_ = runApi.getTime();
  }

  if (const auto execTime = runApi.getTime().sinceEpoch().toSeconds() - startTime_.sinceEpoch().toSeconds();
      execTime > getSecondsToLive() && getSecondsToLive() > 0)
  {
    SPDLOG_INFO("[Creator Monkeys] Lifetime ({}s) reached, stopping kernel execution.", getSecondsToLive());
    runApi.requestKernelStop(0);

    // update start time to avoid logging spam
    startTime_ = runApi.getTime();
  }

  if (frameCount_ % getActionFrameInterval() == 0)
  {
    std::mt19937 gen(rd_());
    std::uniform_int_distribution<int64_t> distrib(0, getMaxObjectCount());

    const auto targetCount = distrib(gen);
    const auto currentCount = static_cast<int64_t>(objects_.size());

    std::uniform_int_distribution<uint64_t> distrib2(0, meta_.size() - 1);
    auto flipCoin = [&]() { return distrib2(gen); };

    if (currentCount > targetCount)
    {
      deleteObject(std::llabs(targetCount - currentCount));
      SPDLOG_INFO("[Creator Monkeys] Deleted {} objects in update cycle", std::llabs(targetCount - currentCount));
    }
    else
    {
      const auto idx = flipCoin();
      addObject(runApi, targetCount - currentCount, meta_[idx].type());
      SPDLOG_INFO("[Creator Monkeys] Added {} objects of type {} in update cycle",
                  targetCount - currentCount,
                  meta_[idx]->getQualifiedName());
    }

    setNextObjectCount(static_cast<uint32_t>(objects_.size()));
  }
  frameCount_++;
}

void CreatorMonkeyImpl::addObject(sen::kernel::RunApi& api, const std::size_t count, const sen::ClassType* type)
{
  std::vector<decltype(objects_)::value_type> toAdd;
  toAdd.reserve(count);

  sen::VarMap map;

  for (std::size_t i = 0; i < count; ++i)
  {
    map["creationTime"] = api.getTime();
    toAdd.push_back(api.getTypes().makeInstance(type, instancesPrefix_ + std::to_string(nextObject_), map));
    nextObject_++;
    objects_.insert(toAdd.back());
  }

  // update created object count
  setNextCreatedObjectsCount(getCreatedObjectsCount() + toAdd.size());
  bus_->add(toAdd);
}

void CreatorMonkeyImpl::deleteObject(const std::size_t count)
{
  std::vector<decltype(objects_)::value_type> toRemove;
  toRemove.reserve(count);

  for (std::size_t i = 0; i < count && toRemove.size() < objects_.size(); ++i)
  {
    toRemove.push_back(*objects_.begin());
    objects_.erase(objects_.begin());
  }

  bus_->remove(toRemove);
  setNextDeletedObjectsCount(getDeletedObjectsCount() + toRemove.size());
}

SEN_EXPORT_CLASS(CreatorMonkeyImpl)

}  // namespace creator_monkeys
