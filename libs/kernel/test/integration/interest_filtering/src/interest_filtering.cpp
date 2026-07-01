// === interest_filtering.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// generated code
#include "stl/interest_filtering.stl.h"

// test_helpers
#include "test_helpers/helpers.h"
#include "test_helpers/test_helpers.stl.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/duration.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/object_source.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"

// std
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <tuple>
#include <utility>

namespace interest_filtering
{

/// Object under test whose properties can be changed calling editObject(). Its heartbeat property can be used to check
/// that the object is alive.
class TestObjectImpl final: public TestObjectBase
{
public:
  explicit TestObjectImpl(std::string name): TestObjectBase(std::move(name)), gen_(rd_()), dis_(0.0, 1.0) {}

public:
  void update(sen::kernel::RunApi& runApi) override
  {
    std::ignore = runApi;
    setNextHeartbeat(dis_(gen_));
  }

protected:
  void editObjectImpl() override
  {
    // set initial values of properties
    setNextFloatProp(100.0);
    setNextBoolProp(true);
    setNextEnumProp(TestEnum::third);
    setNextStructProp(TestStruct {"testString", 100U, {}});
  }

private:
  std::random_device rd_;
  std::mt19937 gen_;
  std::uniform_int_distribution<> dis_;
};

// This publisher does the following:
// - action1: publishes one TestObject on the bus
// - action2: edits properties in TestObject (via editObject())
class TestObjectPublisher final: public sen::test::PublisherImpl
{
public:
  using PublisherImpl::PublisherImpl;

protected:
  void action1() override
  {
    bus_ = getApi()->getSource("session.bus");
    object_ = std::make_shared<TestObjectImpl>("testObject");
    bus_->add(object_);

    PublisherImpl::action1();
  }

  void action2() override
  {
    object_->editObject();
    PublisherImpl::action2();
  }

private:
  std::shared_ptr<sen::ObjectSource> bus_;
  std::shared_ptr<TestObjectImpl> object_;
};

SEN_EXPORT_CLASS(TestObjectPublisher)

constexpr sen::Duration oneSecond {1E9};

// This listener does the following:
// - check1: detects the testObject with a subscription to all TestObjects
// - check2: still detects the testObject after the object properties are changed (no removals detected)
// In both cases it detects that the object is still alive using the heartbeat property of the object
class ListenerTestObjectAll final: public sen::test::ListenerImpl
{
public:
  using ListenerImpl::ListenerImpl;

public:
  void update(sen::kernel::RunApi& runApi) override
  {
    ListenerImpl::update(runApi);
    time_ = runApi.getTime();

    // advance if 1 second passed since the startTime_ (without object deletions)
    if (time_ - *startTime_ > oneSecond)
    {
      doAdvance_ = true;
    }
  }

protected:
  void check1() override
  {
    sub_ = getApi()->selectAllFrom<TestObjectInterface>(
      "session.bus",
      [this](const auto& addedObjects)
      {
        // check that only one object is detected
        SEN_ASSERT(std::distance(addedObjects.begin(), addedObjects.end()) == 1U);
        object_ = *addedObjects.begin();
        doAdvance_ = true;
        hbGuard_ =
          object_->onHeartbeatChanged({this,
                                       [this]()
                                       {
                                         // maybe advance state when the object is alive (check 10 updates)
                                         if (doAdvance_)
                                         {
                                           if (getName() == "listener3")
                                           {
                                             getLogger()->info("checking {} alive.... {}", getName(), numOfHbUpdates_);
                                           }
                                           checkAliveAndAdvanceTest(10U);
                                         }
                                       }});
      });
  }

  void check2() override
  {
    // wait 1 second to check that no object was deleted, then check that the object is still alive (checking 10
    // updates)
    if (!startTime_)
    {
      startTime_ = time_;
    }

    // if a deletion is detected, fail the test
    std::ignore = sub_->list.onRemoved(
      [this](const auto& deletedObjects)
      {
        std::ignore = deletedObjects;
        getLogger()->info("listener all received deletion");
        SEN_ASSERT(false);
      });
  }

private:
  void checkAliveAndAdvanceTest(const uint32_t numberOfUpdatesToCheck)
  {
    if (++numOfHbUpdates_ == numberOfUpdatesToCheck)
    {
      if (const auto currentState = getState(); currentState == sen::test::ConnectionState::step1)
      {
        if (getName() == "listener3")
        {
          getLogger()->info("{} -> check1", getName());
        }
        ListenerImpl::check1();
      }
      else if (currentState == sen::test::ConnectionState::step2)
      {
        if (getName() == "listener3")
        {
          getLogger()->info("{} -> check2", getName());
        }
        ListenerImpl::check2();
      }

      numOfHbUpdates_ = 0;
      doAdvance_ = false;
    }
  }

private:
  TestObjectInterface* object_;
  std::shared_ptr<sen::Subscription<TestObjectInterface>> sub_;
  sen::ConnectionGuard hbGuard_;
  uint32_t numOfHbUpdates_;
  bool doAdvance_ = false;
  sen::TimeStamp time_;
  std::optional<sen::TimeStamp> startTime_;
};

SEN_EXPORT_CLASS(ListenerTestObjectAll)

// This listener does the following:
// - check1: detects the testObject with a subscription to all TestObjects
// - check2: still detects the testObject after the object properties are changed (no removals detected)
// In both cases it detects that the object is still alive using the heartbeat property of the object
class ListenerTestObjectQueryFloat final: public sen::test::ListenerImpl
{
public:
  using ListenerImpl::ListenerImpl;

public:
  void update(sen::kernel::RunApi& runApi) override
  {
    ListenerImpl::update(runApi);
    time_ = runApi.getTime();

    // advance if 1 second passed since the startTime_ (without object deletions)
    if (time_ - *startTime_ > oneSecond)
    {
      doAdvance_ = true;
    }
  }

protected:
  void check1() override
  {
    bus_ = getApi()->getSource("session.bus");

    std::ignore = testObjectList_.onAdded(
      [this](const auto& addedObjects)
      {
        // check that only one object is detected
        SEN_ASSERT(std::distance(addedObjects.begin(), addedObjects.end()) == 1U);
        object_ = *addedObjects.begin();
        doAdvance_ = true;
        hbGuard_ = object_->onHeartbeatChanged({this,
                                                [this]()
                                                {
                                                  // maybe advance state when the object is alive (check 10 updates)
                                                  if (doAdvance_)
                                                  {
                                                    getLogger()->info("receiving updates....");
                                                    if (++numOfHbUpdates_ == 5U)
                                                    {
                                                      ListenerImpl::check1();
                                                      doAdvance_ = false;
                                                    }
                                                  }
                                                }});
      });

    bus_->addSubscriber(
      sen::Interest::make("SELECT interest_filtering.TestObject FROM session.bus WHERE floatProp < 10",
                          getApi()->getTypes()),
      &testObjectList_,
      true);
  }

  void check2() override
  {
    // if a deletion is detected, fail the test
    std::ignore = testObjectList_.onRemoved(
      [this](const auto& deletedObjects)
      {
        getLogger()->info("{} removed", getName());
        // test that the object is deleted after the changes
        SEN_ASSERT(object_->asObject().getId() == (*deletedObjects.begin())->asObject().getId());
        ListenerImpl::check2();
      });
  }

private:
  std::shared_ptr<sen::ObjectSource> bus_;
  sen::ObjectList<TestObjectInterface> testObjectList_;
  TestObjectInterface* object_;
  sen::ConnectionGuard hbGuard_;
  uint32_t numOfHbUpdates_;
  bool doAdvance_ = false;
  sen::TimeStamp time_;
  std::optional<sen::TimeStamp> startTime_;
};

SEN_EXPORT_CLASS(ListenerTestObjectQueryFloat)

}  // namespace interest_filtering
