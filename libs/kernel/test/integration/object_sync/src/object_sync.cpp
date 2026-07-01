// === object_sync.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "stl/object_sync.stl.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/object_source.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"

// spdlog
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

// std
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace object_sync
{

namespace
{

constexpr std::size_t generatorSeed = 2155648U;
constexpr uint32_t numOfChecks = 10U;
constexpr uint8_t staticPropValue = 15U;
constexpr auto staticNoConfigPropValue = TestEnum::second;

[[nodiscard]] std::string generateString(std::mt19937& gen, const int length = 10)
{
  static constexpr std::string_view charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

  std::string result;
  result.reserve(length);
  std::uniform_int_distribution<size_t> dist(0, charset.size() - 1);

  std::generate_n(std::back_inserter(result), length, [&]() { return charset[dist(gen)]; });

  return result;
}

[[nodiscard]] TestStruct generateStruct(std::mt19937& gen)
{
  std::uniform_int_distribution<uint32_t> distInteger;
  std::uniform_real_distribution distFloat(0.0f, 1000.0f);
  std::uniform_int_distribution distStrLen(1U, 6U);

  return {distInteger(gen), generateString(gen, static_cast<int>(distStrLen(gen))), distFloat(gen)};
}

}  // namespace

/// Object used when testing class member synchronization
class TestObjectImpl final: public TestObjectBase
{
public:
  SEN_NOCOPY_NOMOVE(TestObjectImpl)

public:
  TestObjectImpl(const std::string& name, const u8 staticProp)
    : TestObjectBase(name, staticProp)
    , logger_(spdlog::stdout_color_mt(name))
    , gen_(generatorSeed)
    , localMethodGen_(generatorSeed)
  {
    // set static no config property
    setNextStaticNoConfigProp(staticNoConfigPropValue);
  }

  ~TestObjectImpl() override = default;

public:
  void update(sen::kernel::RunApi& runApi) override
  {
    std::ignore = runApi;

    if (!doUpdate_)
    {
      return;
    }

    // keeps track of the number of updates
    setNextUpdateId(++updateCounter_);

    // update best effort prop
    gen_.seed(generatorSeed);
    gen_.discard(updateCounter_);
    setNextBestEffortProp(std::uniform_real_distribution()(gen_));

    // update confirmed prop
    gen_.seed(generatorSeed);
    gen_.discard(updateCounter_);
    setNextConfirmedProp(generateStruct(gen_));

    // update multicast prop
    gen_.seed(generatorSeed);
    gen_.discard(updateCounter_);
    setNextMulticastProp(std::uniform_real_distribution()(gen_));

    // best effort event
    gen_.seed(generatorSeed);
    gen_.discard(updateCounter_);
    bestEffortEvent(updateCounter_, generateStruct(gen_));

    // confirmed event
    gen_.seed(generatorSeed);
    gen_.discard(updateCounter_);
    confirmedEvent(updateCounter_, generateString(gen_));

    // update multicast prop
    gen_.seed(generatorSeed);
    gen_.discard(updateCounter_);
    multicastEvent(updateCounter_, std::uniform_real_distribution()(gen_));
  }

protected:
  [[nodiscard]] u32 constMethodImpl(const TestEnum& arg) const override { return static_cast<u32>(arg); }
  [[nodiscard]] u8 confirmedMethodImpl(u64 arg) override { return static_cast<u8>(arg); }
  [[nodiscard]] bool bestEffortMethodImpl(const std::string& arg) override
  {
    std::ignore = arg;
    return true;
  }
  [[nodiscard]] u16 localMethod() override { return localMethodDist_(gen_); }
  void doUpdateImpl() override { doUpdate_ = true; }

private:
  std::shared_ptr<spdlog::logger> logger_;
  uint32_t updateCounter_ = 0U;
  std::mt19937 gen_;
  bool doUpdate_ = false;

  // method distributions
  std::mt19937 localMethodGen_;
  std::uniform_int_distribution<uint16_t> localMethodDist_;
};

/// Publishes/Unpublishes the TestObject
class PublisherImpl final: public PublisherBase
{
public:
  SEN_NOCOPY_NOMOVE(PublisherImpl)

public:
  PublisherImpl(std::string name, const sen::VarMap& args)
    : PublisherBase(name, args), logger_(spdlog::stdout_color_mt(name))
  {
  }

  ~PublisherImpl() override = default;

public:
  void registered(sen::kernel::RegistrationApi& api) override
  {
    listenerStates_.reserve(getNumOfListeners());

    // detect listeners (used for test shutdown)
    listenerSub_ = api.selectAllFrom<ListenerInterface>(
      "session.bus",
      [this, &api](const auto& addedObjects)
      {
        detectedListeners_ += std::distance(addedObjects.begin(), addedObjects.end());
        for (auto* listener: addedObjects)
        {
          listenerStates_[listener->asObject().getId()] = listener->getState();
          guards_.emplace_back(
            listener->onStateChanged({this,
                                      [this, &api, listener]()
                                      {
                                        std::ignore = api;
                                        const auto& id = listener->asObject().getId();
                                        const auto& name = listener->asObject().getName();
                                        const auto& state = listener->getState();

                                        logger_->info("{} received on state changed from {} to {}",
                                                      getName(),
                                                      name,
                                                      sen::StringConversionTraits<ListenerState>::toString(state));

                                        listenerStates_[id] = state;

                                        // remove the test object from the bus if all listeners are in sync
                                        if (allListenersWithState(ListenerState::inSync))
                                        {
                                          listenerStates_.clear();

                                          logger_->info("{} removing {}", getName(), object_->getName());
                                          bus_->remove(object_);
                                        }

                                        // shutdown the process kernel if all listeners are finished
                                        if (allListenersWithState(ListenerState::finished))
                                        {
                                          logger_->info("{} commanding kernel stop", getName());
                                          api.requestKernelStop();
                                        }

                                        if (allListenersWithState(ListenerState::ready))
                                        {
                                          // start updating the object with all listeners are ready
                                          object_->doUpdate();
                                          logger_->info("listeners ready");
                                        }
                                      }}));
        }

        // publish the test object when all expected listeners have been detected
        if (detectedListeners_ == getNumOfListeners())
        {
          // publish the test object
          bus_ = api.getSource("session.bus");
          object_ = std::make_shared<TestObjectImpl>("testObject", staticPropValue);
          logger_->info("publishing test object");
          bus_->add(object_);
        }
      });
  }

private:
  [[nodiscard]] bool allListenersWithState(const ListenerState state)
  {
    return listenerStates_.size() == getNumOfListeners() &&
           std::all_of(listenerStates_.begin(),
                       listenerStates_.end(),
                       [state](const auto& pair) { return pair.second == state; });
  }

private:
  std::shared_ptr<spdlog::logger> logger_;
  std::shared_ptr<sen::ObjectSource> bus_;
  std::shared_ptr<TestObjectImpl> object_;
  std::shared_ptr<sen::Subscription<ListenerInterface>> listenerSub_;
  std::unordered_map<sen::ObjectId, ListenerState> listenerStates_;
  std::vector<sen::ConnectionGuard> guards_;
  uint32_t detectedListeners_ = 0U;
};

SEN_EXPORT_CLASS(PublisherImpl)

/// Contains all data from TestObject received on the listeners
struct ListenedData
{
  // static props
  uint8_t staticProp;
  TestEnum staticNoConfigProp;

  // dynamic props
  std::vector<float64_t> bestEffortPropUpdates;
  std::vector<TestStruct> confirmedPropUpdates;
  std::vector<OptF32> multicastPropUpdates;
  std::vector<WritablePropType> writablePropUpdates;

  // events
  std::unordered_map<uint32_t, TestStruct> bestEffortEventData;
  std::unordered_map<uint32_t, std::string> confirmedEventData;
  std::unordered_map<uint32_t, OptF32> multicastEventData;
};

/// Detects changes in the TestObject members (from the same, or a different component or process)
class ListenerImpl: public ListenerBase
{
public:
  SEN_NOCOPY_NOMOVE(ListenerImpl)

public:
  ListenerImpl(std::string name, const sen::VarMap& args)
    : ListenerBase(name, args), logger_(spdlog::stdout_color_mt(name))
  {
  }

  ~ListenerImpl() override = default;

public:
  void registered(sen::kernel::RegistrationApi& api) override
  {
    // detect all other listeners
    listenerSub_ = api.selectAllFrom<ListenerInterface>("session.bus");

    // listen to test objects
    bus_ = api.getSource("session.bus");
    bus_->addSubscriber(sen::Interest::make(getQuery(), api.getTypes()), &objList_, true);

    std::ignore = objList_.onAdded(
      [this](const auto& addedObjects)
      {
        if (addedObjects.begin() != addedObjects.end())
        {
          testObject_ = *addedObjects.begin();

          logger_->info("{} detected {}", getName(), testObject_->asObject().getName());

          // store static data
          data_.staticProp = testObject_->getStaticProp();
          data_.staticNoConfigProp = testObject_->getStaticNoConfigProp();
          doSync();

          // property update callbacks
          objGuards_.emplace_back(testObject_->onBestEffortPropChanged(
            {this,
             [this]()
             {
               doSync(testObject_->getUpdateId());

               if (data_.bestEffortPropUpdates.size() < numOfChecks)
               {
                 data_.bestEffortPropUpdates.push_back(testObject_->getBestEffortProp());
               }
             }}));
          objGuards_.emplace_back(testObject_->onConfirmedPropChanged(
            {this,
             [this]()
             {
               doSync(testObject_->getUpdateId());
               if (data_.confirmedPropUpdates.size() < numOfChecks)
               {
                 data_.confirmedPropUpdates.push_back(testObject_->getConfirmedProp());
               }
             }}));
          objGuards_.emplace_back(testObject_->onMulticastPropChanged(
            {this,
             [this]()
             {
               doSync(testObject_->getUpdateId());
               if (data_.multicastPropUpdates.size() < numOfChecks)
               {
                 data_.multicastPropUpdates.push_back(testObject_->getMulticastProp());
               }
             }}));
          objGuards_.emplace_back(
            testObject_->onWritablePropChanged({this,
                                                [this]()
                                                {
                                                  doSync(testObject_->getUpdateId());
                                                  if (data_.writablePropUpdates.size() < numOfChecks)
                                                  {
                                                    data_.writablePropUpdates.push_back(testObject_->getWritableProp());
                                                  }
                                                }}));

          objGuards_.emplace_back(
            testObject_->onBestEffortEvent({this,
                                            [this](const uint32_t id, const TestStruct& arg)
                                            {
                                              if (data_.bestEffortEventData.size() < numOfChecks)
                                              {
                                                if (!data_.bestEffortEventData.insert({id, arg}).second)
                                                {
                                                  // Key was repeated; result.first points to the existing
                                                  // element
                                                  repeatedEventsReceived_ = true;
                                                }
                                              }
                                            }}));
          objGuards_.emplace_back(
            testObject_->onConfirmedEvent({this,
                                           [this](const uint32_t id, const std::string& arg)
                                           {
                                             if (data_.confirmedEventData.size() < numOfChecks)
                                             {
                                               if (!data_.confirmedEventData.insert({id, arg}).second)
                                               {
                                                 // Key was repeated; result.first points to the existing
                                                 // element
                                                 repeatedEventsReceived_ = true;
                                               }
                                             }
                                           }}));
          objGuards_.emplace_back(
            testObject_->onMulticastEvent({this,
                                           [this](const uint32_t id, const OptF32& arg)
                                           {
                                             if (data_.multicastEventData.size() < numOfChecks)
                                             {
                                               if (!data_.multicastEventData.insert({id, arg}).second)
                                               {
                                                 // Key was repeated; result.first points to the existing
                                                 // element
                                                 repeatedEventsReceived_ = true;
                                               }
                                             }
                                           }}));

          setNextState(ListenerState::ready);
        }
      });

    std::ignore = objList_.onRemoved(
      [this](const auto& removedObjects)
      {
        if (removedObjects.begin() != removedObjects.end())
        {
          // clear callbacks associated to the object
          testObject_ = nullptr;
          doChecks();
          setNextState(ListenerState::finished);
          logger_->info("{} moved to finished state", getName());
        }
      });

    // detect when the listener has finished and stop the kernel if all other listeners are finished
    listenersGuard_ =
      onStateChanged({this,
                      [this, &api]()
                      {
                        // if all listeners are finished, stop the process
                        if (std::all_of(listenerSub_->list.getObjects().begin(),
                                        listenerSub_->list.getObjects().end(),
                                        [](const auto* elem) { return elem->getState() == ListenerState::finished; }))
                        {
                          logger_->info("stopping listener process", getName());
                          api.requestKernelStop();
                        }
                      }});
  }

protected:
  std::shared_ptr<spdlog::logger>& getLogger() { return logger_; }
  [[nodiscard]] const ListenedData& getData() const noexcept { return data_; }
  [[nodiscard]] TestObjectInterface* getTestObject() const noexcept { return testObject_; }
  [[nodiscard]] uint32_t getFirstUpdateId() const noexcept { return firstUpdateId_; }
  [[nodiscard]] bool getRepeatedEventsReceived() const noexcept { return repeatedEventsReceived_; }

protected:
  /// Synchronizes received updates and change the state to onSync when the required updates are received
  virtual void doSync(uint32_t updateId = 0)  // NOLINT [google-default-arguments]
  {
    if (firstUpdateId_ == 0)
    {
      firstUpdateId_ = updateId;
    }
  }

private:
  /// Asserts the correctness of the property updates received
  virtual void doChecks() {}

private:
  std::shared_ptr<sen::ObjectSource> bus_;
  sen::ObjectList<TestObjectInterface> objList_;
  TestObjectInterface* testObject_ = nullptr;
  std::shared_ptr<sen::Subscription<ListenerInterface>> listenerSub_;
  std::vector<sen::ConnectionGuard> objGuards_;
  sen::ConnectionGuard listenersGuard_;
  std::shared_ptr<spdlog::logger> logger_;
  ListenedData data_ {};
  uint32_t firstUpdateId_ = 0;  // first update ID received by the listener
  bool repeatedEventsReceived_ =
    false;  // true if the events where received more than once in each participant (previous bug)
  std::vector<ListenerInterface*> listeners_;
};

/// Listener that checks if static props are synchronized correctly
class ListenerStaticProps final: public ListenerImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerStaticProps)

public:
  using ListenerImpl::ListenerImpl;
  ~ListenerStaticProps() override = default;

private:  // implements ListenerImpl
  void doSync(uint32_t updateId) override
  {
    std::ignore = updateId;

    if (getData().staticProp != 0U)
    {
      setNextState(ListenerState::inSync);
    }
  }

  void doChecks() override
  {
    const auto& data = getData();

    SEN_ASSERT(data.staticProp == staticPropValue);
    SEN_ASSERT(data.staticNoConfigProp == staticNoConfigPropValue);
  }
};

SEN_EXPORT_CLASS(ListenerStaticProps)

/// Listener that checks if static props are synchronized correctly
class ListenerBestEffortProps final: public ListenerImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerBestEffortProps)

public:
  using ListenerImpl::ListenerImpl;
  ~ListenerBestEffortProps() override = default;

private:  // implements ListenerImpl
  void doSync(uint32_t updateId) override
  {
    ListenerImpl::doSync(updateId);

    if (getData().bestEffortPropUpdates.size() == numOfChecks)
    {
      setNextState(ListenerState::inSync);
    }
  }

  void doChecks() override
  {
    const auto& data = getData();

    SEN_ASSERT(data.bestEffortPropUpdates.size() == numOfChecks);

    // generator for the updates
    for (uint32_t i = 0; i < numOfChecks; ++i)
    {
      gen_.seed(generatorSeed);
      gen_.discard(getFirstUpdateId() + i);
      SEN_ASSERT(data.bestEffortPropUpdates[i] - std::uniform_real_distribution()(gen_) < 1e-6);
    }
  }

private:
  std::mt19937 gen_ {generatorSeed};
};

SEN_EXPORT_CLASS(ListenerBestEffortProps)

/// Listener that checks if static props are synchronized correctly
class ListenerConfirmedProps final: public ListenerImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerConfirmedProps)

public:
  using ListenerImpl::ListenerImpl;
  ~ListenerConfirmedProps() override = default;

private:  // implements ListenerImpl
  void doSync(uint32_t updateId) override
  {
    ListenerImpl::doSync(updateId);

    if (getData().confirmedPropUpdates.size() == numOfChecks)
    {
      setNextState(ListenerState::inSync);
    }
  }

  void doChecks() override
  {
    const auto& data = getData();
    SEN_ASSERT(data.confirmedPropUpdates.size() == numOfChecks);

    // generator for the updates
    for (uint32_t i = 0; i < numOfChecks; ++i)
    {
      std::mt19937 gen(generatorSeed);
      gen.discard(getFirstUpdateId() + i);
      SEN_ASSERT(data.confirmedPropUpdates[i] == generateStruct(gen));
    }
  }
};

SEN_EXPORT_CLASS(ListenerConfirmedProps)

/// Listener that checks if multicast props are synchronized correctly
class ListenerMulticastProps final: public ListenerImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerMulticastProps)

public:
  using ListenerImpl::ListenerImpl;
  ~ListenerMulticastProps() override = default;

private:  // implements ListenerImpl
  void doSync(uint32_t updateId) override
  {
    ListenerImpl::doSync(updateId);

    if (getData().multicastPropUpdates.size() == numOfChecks)
    {
      setNextState(ListenerState::inSync);
    }
  }

  void doChecks() override
  {
    const auto& data = getData();
    SEN_ASSERT(data.multicastPropUpdates.size() == numOfChecks);

    // generator for the updates
    for (uint32_t i = 0; i < numOfChecks; ++i)
    {
      std::mt19937 gen(generatorSeed);
      gen.discard(getFirstUpdateId() + i);
      SEN_ASSERT(data.multicastPropUpdates[i].asOptional().has_value());
      if (data.multicastPropUpdates[i].asOptional().has_value())
      {
        SEN_ASSERT(data.multicastPropUpdates[i].asOptional().value() ==
                   static_cast<float32_t>(std::uniform_real_distribution()(gen)));
      }
    }
  }
};

SEN_EXPORT_CLASS(ListenerMulticastProps)

/// Listener that checks if writable props are synchronized . We just send the update ID in the writable prop directly
class ListenerWritableProps final: public ListenerImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerWritableProps)

public:
  ListenerWritableProps(std::string name, const sen::VarMap& args)
    : ListenerImpl(std::move(name), args), gen_(generatorSeed)
  {
  }
  ~ListenerWritableProps() override = default;

public:
  void update(sen::kernel::RunApi& runApi) override
  {
    std::ignore = runApi;

    // set the writable property
    if (auto* obj = getTestObject(); obj != nullptr)
    {
      obj->setNextWritableProp({counter_++, updateDist_(gen_)});
    }
  }

private:  // implements ListenerImpl
  void doSync(uint32_t updateId) override
  {
    std::ignore = updateId;

    if (getData().writablePropUpdates.size() == numOfChecks)
    {
      setNextState(ListenerState::inSync);
    }
  }

  void doChecks() override
  {
    const auto& updates = getData().writablePropUpdates;

    for (const auto& [id, value]: updates)
    {
      gen_.seed(generatorSeed);
      updateDist_.reset();
      gen_.discard(id);
      SEN_ASSERT(value == updateDist_(gen_));
    }
  }

private:
  uint32_t counter_ = 0U;
  std::mt19937_64 gen_;
  std::uniform_int_distribution<uint64_t> updateDist_;
};

SEN_EXPORT_CLASS(ListenerWritableProps)

/// Listener that checks if best effort events are transmitted correctly
class ListenerBestEffortEvent final: public ListenerImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerBestEffortEvent)

public:
  ListenerBestEffortEvent(std::string name, const sen::VarMap& args)
    : ListenerImpl(std::move(name), args), gen_(generatorSeed)
  {
  }
  ~ListenerBestEffortEvent() override = default;

private:  // implements ListenerImpl
  void doSync(uint32_t updateId) override
  {
    std::ignore = updateId;
    if (getData().bestEffortEventData.size() == numOfChecks)
    {
      setNextState(ListenerState::inSync);
    }
  }

  void doChecks() override
  {
    const auto& data = getData();
    SEN_ASSERT(data.bestEffortEventData.size() == numOfChecks);

    // generator for the updates
    for (const auto& [id, value]: data.bestEffortEventData)
    {
      gen_.seed(generatorSeed);
      gen_.discard(id);
      SEN_ASSERT(value == generateStruct(gen_));
    }
    // check that the event was not received multiple times
    SEN_ASSERT(!getRepeatedEventsReceived());
  }

private:
  std::mt19937 gen_;
};

SEN_EXPORT_CLASS(ListenerBestEffortEvent)

/// Listener that checks if confirmed events are transmitted correctly
class ListenerConfirmedEvent final: public ListenerImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerConfirmedEvent)

public:
  ListenerConfirmedEvent(std::string name, const sen::VarMap& args)
    : ListenerImpl(std::move(name), args), gen_(generatorSeed)
  {
  }
  ~ListenerConfirmedEvent() override = default;

private:  // implements ListenerImpl
  void doSync(uint32_t updateId) override
  {
    std::ignore = updateId;
    if (getData().confirmedEventData.size() == numOfChecks)
    {
      setNextState(ListenerState::inSync);
    }
  }

  void doChecks() override
  {
    const auto& data = getData();
    SEN_ASSERT(data.confirmedEventData.size() == numOfChecks);

    // generator for the updates
    for (const auto& [id, value]: data.confirmedEventData)
    {
      gen_.seed(generatorSeed);
      gen_.discard(id);
      SEN_ASSERT(value == generateString(gen_));
    }

    // check that the event was not received multiple times
    SEN_ASSERT(!getRepeatedEventsReceived());
  }

private:
  std::mt19937 gen_;
};

SEN_EXPORT_CLASS(ListenerConfirmedEvent)

/// Listener that checks if multicast events are transmitted correctly
class ListenerMulticastEvent final: public ListenerImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerMulticastEvent)

public:
  ListenerMulticastEvent(std::string name, const sen::VarMap& args)
    : ListenerImpl(std::move(name), args), gen_(generatorSeed)
  {
  }
  ~ListenerMulticastEvent() override = default;

private:  // implements ListenerImpl
  void doSync(uint32_t updateId) override
  {
    std::ignore = updateId;
    if (getData().multicastEventData.size() == numOfChecks)
    {
      setNextState(ListenerState::inSync);
    }
  }

  void doChecks() override
  {
    const auto& data = getData();
    SEN_ASSERT(data.multicastEventData.size() == numOfChecks);

    // generator for the updates
    for (const auto& [id, value]: data.multicastEventData)
    {
      gen_.seed(generatorSeed);
      gen_.discard(id);
      SEN_ASSERT(value.asOptional().has_value());
      if (value.asOptional().has_value())
      {
        SEN_ASSERT(value.asOptional().value() == static_cast<float32_t>(std::uniform_real_distribution()(gen_)));
      }
    }

    // check that the event was not received multiple times
    SEN_ASSERT(!getRepeatedEventsReceived());
  }

private:
  std::mt19937 gen_;
};

SEN_EXPORT_CLASS(ListenerMulticastEvent)

/// Listener that checks if confirmed events are transmitted correctly
class ListenerLocalMethod final: public ListenerImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerLocalMethod)

public:
  ListenerLocalMethod(std::string name, const sen::VarMap& args)
    : ListenerImpl(std::move(name), args), gen_(generatorSeed)
  {
    returnValues_.reserve(numOfChecks);
  }
  ~ListenerLocalMethod() override = default;

public:
  void update(sen::kernel::RunApi& runApi) override
  {
    std::ignore = runApi;
    if (auto* testObject = getTestObject(); testObject != nullptr)
    {
      returnValues_.push_back(testObject->localMethod());

      if (returnValues_.size() == numOfChecks)
      {
        setNextState(ListenerState::inSync);
      }
    }
  }

private:  // implements ListenerImpl
  void doChecks() override
  {
    // generator for the updates
    for (const auto value: returnValues_)
    {
      SEN_ASSERT(value == distribution_(gen_));
    }
  }

private:
  std::mt19937 gen_;
  std::uniform_int_distribution<uint16_t> distribution_;
  std::vector<uint16_t> returnValues_;
};

SEN_EXPORT_CLASS(ListenerLocalMethod)

/// Listener that checks if confirmed return correctly when called
class ListenerConstMethod final: public ListenerImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerConstMethod)

public:
  ListenerConstMethod(std::string name, const sen::VarMap& args)
    : ListenerImpl(std::move(name), args), gen_(generatorSeed), distribution_(0U, 2U)
  {
    returnValues_.reserve(numOfChecks);
  }
  ~ListenerConstMethod() override = default;

public:
  void update(sen::kernel::RunApi& runApi) override
  {
    std::ignore = runApi;
    if (const auto* testObject = getTestObject(); testObject != nullptr)
    {
      testObject->constMethod(static_cast<TestEnum>(distribution_(gen_)),
                              {this,
                               [this](const auto& response)
                               {
                                 if (response)
                                 {
                                   returnValues_.push_back(response.getValue());

                                   if (returnValues_.size() == numOfChecks)
                                   {
                                     setNextState(ListenerState::inSync);
                                   }
                                 }
                               }});
    }
  }

private:  // implements ListenerImpl
  void doChecks() override
  {
    // check that we have collected responses within range (expected values)
    SEN_ASSERT(std::all_of(returnValues_.begin(), returnValues_.end(), [](uint32_t val) { return val <= 2U; }));
  }

private:
  std::mt19937 gen_;
  std::uniform_int_distribution<uint8_t> distribution_;
  std::vector<uint32_t> returnValues_;
};

SEN_EXPORT_CLASS(ListenerConstMethod)

/// Listener that checks if confirmed methods return correctly when called
class ListenerConfirmedMethod final: public ListenerImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerConfirmedMethod)

public:
  ListenerConfirmedMethod(std::string name, const sen::VarMap& args)
    : ListenerImpl(std::move(name), args), gen_(generatorSeed), distribution_(0U, 2U)
  {
    returnValues_.reserve(numOfChecks);
  }
  ~ListenerConfirmedMethod() override = default;

public:
  void update(sen::kernel::RunApi& runApi) override
  {
    std::ignore = runApi;
    if (const auto* testObject = getTestObject(); testObject != nullptr)
    {
      testObject->constMethod(static_cast<TestEnum>(distribution_(gen_)),
                              {this,
                               [this](const auto& response)
                               {
                                 if (response)
                                 {
                                   returnValues_.push_back(response.getValue());

                                   if (returnValues_.size() == numOfChecks)
                                   {
                                     setNextState(ListenerState::inSync);
                                   }
                                 }
                               }});
    }
  }

private:  // implements ListenerImpl
  void doChecks() override
  {
    // check that we have collected responses within range (expected values)
    SEN_ASSERT(std::all_of(returnValues_.begin(), returnValues_.end(), [](uint32_t val) { return val <= 2U; }));
  }

private:
  std::mt19937 gen_;
  std::uniform_int_distribution<uint8_t> distribution_;
  std::vector<uint32_t> returnValues_;
};

SEN_EXPORT_CLASS(ListenerConfirmedMethod)

}  // namespace object_sync
