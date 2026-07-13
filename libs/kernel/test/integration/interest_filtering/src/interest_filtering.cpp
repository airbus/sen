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
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

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
    object_ = std::make_shared<TestObjectImpl>(getName() + "_testObject");
    bus_->add(object_);

    getLogger()->error("OBJECT ID {}", object_->getId().get());

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

// Time gap big enough for the listeners to receive updates
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
  void registered(sen::kernel::RegistrationApi& api) override
  {
    ListenerImpl::registered(api);

    sub_ = getApi()->selectAllFrom<TestObjectInterface>(
      "session.bus",
      [this](const auto& addedObjects)
      {
        guards_.reserve(std::distance(addedObjects.begin(), addedObjects.end()));
        for (auto* object: addedObjects)
        {
          guards_.emplace_back(object->onHeartbeatChanged({this,
                                                           [this]()
                                                           {
                                                             if (++numOfHbUpdates_ == 10U)
                                                             {
                                                               SEN_ASSERT(sub_->list.getObjects().size() == 1);
                                                               ListenerImpl::check1();
                                                             }
                                                           }}));
        }
      });
  }

  void update(sen::kernel::RunApi& runApi) override
  {
    ListenerImpl::update(runApi);

    time_ = runApi.getTime();

    // wait for one second without removals before check2
    if (check2Start_ && time_ - *check2Start_ > oneSecond)
    {
      SEN_ASSERT(sub_->list.getObjects().size() == 1);
      check2Start_.reset();
      ListenerImpl::check2();
    }
  }

protected:
  void check1() override
  {
    // no code needed here
  }

  void check2() override { check2Start_ = time_; }

private:
  std::shared_ptr<sen::Subscription<TestObjectInterface>> sub_;
  uint32_t numOfHbUpdates_ = 0U;
  sen::TimeStamp time_;
  std::optional<sen::TimeStamp> check2Start_;
  std::vector<sen::ConnectionGuard> guards_;
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
  void registered(sen::kernel::RegistrationApi& api) override
  {
    ListenerImpl::registered(api);

    bus_ = api.getSource("session.bus");

    std::ignore = testObjectList_.onAdded(
      [this](const auto& addedObjects)
      {
        guards_.reserve(std::distance(addedObjects.begin(), addedObjects.end()));
        for (auto* object: addedObjects)
        {
          guards_.emplace_back(object->onHeartbeatChanged({this,
                                                           [this]()
                                                           {
                                                             if (++numOfHbUpdates_ == 10U)
                                                             {
                                                               SEN_ASSERT(testObjectList_.getObjects().size() == 1);
                                                               ListenerImpl::check1();
                                                             }
                                                           }}));
        }
      });

    std::ignore = testObjectList_.onRemoved(
      [this](const auto& deletedObjects)
      {
        getLogger()->error("{} RECEIVED DELETION", getName());
        SEN_ASSERT(std::distance(deletedObjects.begin(), deletedObjects.end()) == 1U);
        SEN_ASSERT(testObjectList_.getObjects().empty());
        ListenerImpl::check2();
      });

    bus_->addSubscriber(
      sen::Interest::make("SELECT interest_filtering.TestObject FROM session.bus WHERE floatProp < 10",
                          getApi()->getTypes()),
      &testObjectList_,
      true);
  }

  void update(sen::kernel::RunApi& runApi) override
  {
    ListenerImpl::update(runApi);

    time_ = runApi.getTime();
  }

protected:
  void check1() override
  {
    // no code needed
  }

  void check2() override
  {
    // no code needed
  }

private:
  std::shared_ptr<sen::ObjectSource> bus_;
  sen::ObjectList<TestObjectInterface> testObjectList_;
  uint32_t numOfHbUpdates_ = 0U;
  sen::TimeStamp time_;
  std::vector<sen::ConnectionGuard> guards_;
};

SEN_EXPORT_CLASS(ListenerTestObjectQueryFloat)

// This listener does the following:
// - check1: detects the test object with 5 different subscriptions with different interests each. All interests
// should find the object. Checks that the proxy pointed to by all interests is unique.
// - check2: after object properties change. Checks that only one of the subscriptions find the object.
class MultipleSubscriptionListener final: public sen::test::ListenerImpl
{
public:
  using ListenerImpl::ListenerImpl;

public:
  void update(sen::kernel::RunApi& runApi) override
  {
    ListenerImpl::update(runApi);

    const auto nullProxies = getNullProxies();

    // perform check1 once the object has been detected in all 5 subscriptions
    if (getState() == sen::test::ConnectionState::step1 && nullProxies.empty())
    {
      // check that all subscriptions point to the same object
      SEN_ASSERT(
        std::all_of(objects_.begin(), objects_.end(), [first = objects_[0]](const auto& obj) { return obj == first; }));

      ListenerImpl::check1();
    }

    // check2 once the only subscription that detects the object is the first query (no where condition)
    if (getState() == sen::test::ConnectionState::step2 && nullProxies.size() == 4 &&
        std::find(nullProxies.begin(), nullProxies.end(), 0) == nullProxies.end())
    {
      ListenerImpl::check2();
    }
  }

protected:
  void check1() override
  {
    bus_ = getApi()->getSource("session.bus");

    for (size_t i = 0U; i < lists_.size(); ++i)
    {
      std::ignore = lists_.at(i).onAdded([&obj = objects_.at(i)](const auto& iterators) { obj = *iterators.begin(); });

      std::ignore = lists_.at(i).onRemoved(
        [&obj = objects_.at(i)](const auto& iterators)
        {
          if (obj == *iterators.begin())
          {
            obj = nullptr;
          }
        });

      bus_->addSubscriber(sen::Interest::make(queries.at(i), getApi()->getTypes()), &lists_.at(i), true);
    }
  }

  void check2() override
  {
    // disable automatic step 2
  }

private:
  /// Returns the indexes of the TestObjectInterface pointers received in the subscriptions that are nullptr (the
  /// object is not currently among the detections for that subscription)
  std::vector<size_t> getNullProxies() const
  {
    std::vector<size_t> indexes;
    indexes.reserve(objects_.size());
    for (size_t i = 0U; i < objects_.size(); ++i)
    {
      if (objects_.at(i) == nullptr)
      {
        indexes.push_back(i);
      }
    }
    return indexes;
  }

private:
  static constexpr std::array<std::string_view, 5U> queries {
    "SELECT interest_filtering.TestObject FROM session.bus",
    "SELECT interest_filtering.TestObject FROM session.bus WHERE floatProp < 10.0",
    "SELECT interest_filtering.TestObject FROM session.bus WHERE boolProp = false",
    R"(SELECT interest_filtering.TestObject FROM session.bus WHERE enumProp IN ("first", "second"))",
    "SELECT interest_filtering.TestObject FROM session.bus WHERE structProp.member2 = 0"};

  std::shared_ptr<sen::ObjectSource> bus_;
  std::array<sen::ObjectList<TestObjectInterface>, 5U> lists_;
  std::array<TestObjectInterface*, 5U> objects_ {};
};

SEN_EXPORT_CLASS(MultipleSubscriptionListener)

}  // namespace interest_filtering
