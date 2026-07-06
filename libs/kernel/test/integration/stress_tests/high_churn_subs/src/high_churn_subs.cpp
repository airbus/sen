// === high_churn_subs.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// generated code
#include "stl/high_churn_subs.stl.h"

// test_helpers
#include "test_helpers/helpers.h"
#include "test_helpers/test_helpers.stl.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/component_api.h"

// std
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace high_churn_subs
{

class TestObjectImpl final: public TestObjectBase
{
public:
  explicit TestObjectImpl(std::string name): TestObjectBase(std::move(name)), gen_(rd_()), dis_(0.0, 100.0) {}

public:
  void update(sen::kernel::RunApi& runApi) override
  {
    std::ignore = runApi;
    setNextProperty(dis_(gen_));
  }

private:
  std::random_device rd_;
  std::mt19937 gen_;
  std::uniform_real_distribution<> dis_;
};

class Producer: public sen::test::PublisherImpl
{
public:
  using PublisherImpl::PublisherImpl;

public:
  void registered(sen::kernel::RegistrationApi& api) override
  {
    PublisherImpl::registered(api);

    object_ = std::make_shared<TestObjectImpl>(getName() + "_object");
    bus_ = api.getSource("session.bus");
    bus_->add(object_);
  }

private:
  std::shared_ptr<sen::ObjectSource> bus_;
  std::shared_ptr<TestObjectImpl> object_;
};

SEN_EXPORT_CLASS(Producer)

class Consumer: public sen::test::ListenerImpl
{
public:
  Consumer(std::string name, const sen::VarMap& args)
    : ListenerImpl(std::move(name), args), gen_(std::random_device {}())
  {
  }

public:
  void registered(sen::kernel::RegistrationApi& api) override
  {
    ListenerImpl::registered(api);

    bus_ = api.getSource("session.bus");

    constexpr std::array<std::string_view, 10> queries {
      "SELECT interest_filtering.TestObject FROM session.bus",
      "SELECT interest_filtering.TestObject FROM session.bus WHERE property < 10.0",
      "SELECT interest_filtering.TestObject FROM session.bus WHERE property > 20.0",
      "SELECT interest_filtering.TestObject FROM session.bus WHERE property < 30.0",
      "SELECT interest_filtering.TestObject FROM session.bus WHERE property > 40.0",
      "SELECT interest_filtering.TestObject FROM session.bus WHERE property < 50.0",
      "SELECT interest_filtering.TestObject FROM session.bus WHERE property > 60.0",
      "SELECT interest_filtering.TestObject FROM session.bus WHERE property < 70.0",
      "SELECT interest_filtering.TestObject FROM session.bus WHERE property > 80.0",
      "SELECT interest_filtering.TestObject FROM session.bus WHERE property < 70.0"};

    // build an interest for each of the queries
    interests_.reserve(queries.size());
    for (const auto query: queries)
    {
      interests_.emplace_back(sen::Interest::make(query, api.getTypes()));
    }
  }

  void update(sen::kernel::RunApi& runApi) override
  {
    ListenerImpl::update(runApi);

    // we add/remove subscriptions in batches of up to three elements
    constexpr size_t batchSize = 3U;

    // terminate the test after 20 seconds
    if (getState() != sen::test::ConnectionState::starting &&
        runApi.getTime() - runApi.getStartTime() > std::chrono::seconds(20U))
    {
      setNextState(sen::test::ConnectionState::finished);
      return;
    }

    // skip 20% of frames to create a random pace
    if (diceRoll(1, 100) > 80)
    {
      return;
    }

    // add subscriptions 50% of the time and remove subscriptions 50% of the time
    if (diceRoll(0, 1) == 1)
    {
      for (size_t i = 0; i < diceRoll(1, batchSize); ++i)
      {
        // get random interest
        std::shared_ptr<sen::Interest> randomInterest;
        std::sample(interests_.begin(), interests_.end(), &randomInterest, 1U, gen_);

        // random object list
        auto& randomList = lists_.at(diceRoll(0, lists_.size() - 1));

        // create and store the active subscriber
        bus_->addSubscriber(randomInterest, &randomList, true);
        activeSubs_.insert(&randomList);

        getLogger()->info("{} creating sub with interest {}. Number of active subs: {}.",
                          getName(),
                          randomInterest->getQueryString(),
                          activeSubs_.size());
      }
    }
    else
    {
      for (size_t i = 0; i < std::min(diceRoll(1, batchSize), activeSubs_.size()); ++i)
      {
        auto* list = *activeSubs_.begin();
        activeSubs_.erase(activeSubs_.begin());
        bus_->removeSubscriber(list, true);
      }

      getLogger()->info("{} erasing subs. Number of active subs: {}.", getName(), activeSubs_.size());
    }
  }

protected:
  void check1() override
  {
    // avoid premature termination of the test
  }

private:
  size_t diceRoll(size_t min, size_t max)
  {
    std::uniform_int_distribution<size_t> dist(min, max);
    return dist(gen_);
  }

private:
  std::shared_ptr<sen::ObjectSource> bus_;
  std::array<sen::ObjectList<TestObjectInterface>, 10U> lists_;
  std::vector<std::shared_ptr<sen::Interest>> interests_;
  std::unordered_set<sen::ObjectList<TestObjectInterface>*> activeSubs_;
  std::mt19937 gen_;
};

SEN_EXPORT_CLASS(Consumer)

}  // namespace high_churn_subs
