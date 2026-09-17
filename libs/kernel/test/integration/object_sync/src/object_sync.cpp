// === object_sync.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "stl/object_sync.stl.h"

// test_helpers
#include "test_helpers/helpers.h"

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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
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

[[nodiscard]] TestEnum generateEnum(std::mt19937& gen)
{
  return static_cast<TestEnum>(std::uniform_int_distribution(0, 2)(gen));
}

template <typename ValueType, typename Func>
inline std::optional<size_t> getFirstUpdateIndex(ValueType value, Func&& randomGen)
{
  constexpr auto maxFirstUpdateIndex = 1000;
  for (size_t i = 0; i < maxFirstUpdateIndex; ++i)
  {
    std::mt19937 gen {generatorSeed};
    gen.discard(i);

    const auto generatedVal = randomGen(gen);
    if constexpr (std::is_floating_point_v<decltype(generatedVal)>)
    {
      if (std::abs(generatedVal - value) < 1e-6)
      {
        return i;  // found the matching position
      }
    }
    else
    {
      if (generatedVal == value)
      {
        return i;
      }
    }
  }

  return std::nullopt;  // value was not found within the limits
}

}  // namespace

/// Object used when testing class member synchronization
class TestObjectImpl final: public TestObjectBase
{
public:
  SEN_NOCOPY_NOMOVE(TestObjectImpl)

public:
  TestObjectImpl(const std::string& name, const u8 staticProp)
    : TestObjectBase(name, staticProp), logger_(spdlog::stdout_color_mt(name))
  {
    // set static no config property
    setNextStaticNoConfigProp(staticNoConfigPropValue);
  }

  ~TestObjectImpl() override = default;

public:
  void update(sen::kernel::RunApi& runApi) override
  {
    std::ignore = runApi;

    updateCounter_++;

    // update best effort prop
    resetGen();
    setNextBestEffortProp(std::uniform_real_distribution()(gen_));

    // update confirmed prop
    resetGen();
    setNextConfirmedProp(generateStruct(gen_));

    // update multicast prop
    resetGen();
    setNextMulticastProp(std::uniform_real_distribution()(gen_));

    // best effort event
    resetGen();
    bestEffortEvent(updateCounter_, generateStruct(gen_));

    // confirmed event
    resetGen();
    confirmedEvent(updateCounter_, generateString(gen_));

    // update multicast prop
    resetGen();
    multicastEvent(updateCounter_, std::uniform_real_distribution()(gen_));
  }

protected:
  [[nodiscard]] u32 constMethodImpl(const TestEnum& arg) const override { return static_cast<u32>(arg); }
  [[nodiscard]] u8 confirmedMethodImpl(u64 arg) override { return static_cast<u8>(arg); }
  [[nodiscard]] u64 bestEffortMethodImpl(const std::string& arg) override { return std::hash<std::string>()(arg); }
  void resetGen()
  {
    gen_.seed(generatorSeed);
    gen_.discard(updateCounter_);
  }
  [[nodiscard]] u16 localMethod() override { return std::uniform_int_distribution<uint16_t>()(localMethodGen_); }

private:
  std::shared_ptr<spdlog::logger> logger_;
  uint32_t updateCounter_ = 0U;
  std::mt19937 gen_ {generatorSeed};
  std::mt19937 localMethodGen_ {generatorSeed};
};

/// Publishes/Unpublishes the TestObject
class PublisherObjectSync final: public sen::test::PublisherImpl
{
public:
  SEN_NOCOPY_NOMOVE(PublisherObjectSync)

public:
  using PublisherImpl::PublisherImpl;
  ~PublisherObjectSync() override = default;

public:
  void registered(sen::kernel::RegistrationApi& api) override
  {
    PublisherImpl::registered(api);
    bus_ = api.getSource("session.bus");
  }

protected:
  void action1() override
  {
    object_ = std::make_shared<TestObjectImpl>("testObject", staticPropValue);
    bus_->add(object_);

    PublisherImpl::action1();
  }

  // NOTE: we use action3 here because we install the onRemoved callback in the listener's check2()
  void action3() override
  {
    bus_->remove(object_);

    PublisherImpl::action3();
  }

private:
  std::shared_ptr<sen::ObjectSource> bus_;
  std::shared_ptr<TestObjectImpl> object_;
};

SEN_EXPORT_CLASS(PublisherObjectSync)

/// Detects changes in the TestObject members (from the same, or a different component or process)
class ListenerObjectSyncImpl: public ListenerObjectSyncBase<sen::test::ListenerImpl>
{
public:
  SEN_NOCOPY_NOMOVE(ListenerObjectSyncImpl)

public:
  using ListenerObjectSyncBase::ListenerObjectSyncBase;
  ~ListenerObjectSyncImpl() override = default;

public:
  void registered(sen::kernel::RegistrationApi& api) override
  {
    ListenerObjectSyncBase::registered(api);
    bus_ = api.getSource("session.bus");
    bus_->addSubscriber(sen::Interest::make(getQuery(), api.getTypes()), &list_, true);
  }

protected:
  virtual void onTestObjectAdded(TestObjectInterface* obj)
  {
    std::ignore = obj;
    ListenerImpl::check1();
  }

  virtual void onTestObjectRemoved(TestObjectInterface* obj)
  {
    std::ignore = obj;
    ListenerImpl::check3();
  }

protected:  // implements ListenerImpl
  void check1() override
  {
    std::ignore = list_.onAdded(
      [this](const auto& addedObjects)
      {
        SEN_ASSERT(std::distance(addedObjects.begin(), addedObjects.end()) == 1);
        onTestObjectAdded(*addedObjects.begin());
      });
  }

  void check2() override
  {
    std::ignore = list_.onRemoved(
      [this](const auto& removedObjects)
      {
        SEN_ASSERT(std::distance(removedObjects.begin(), removedObjects.end()) == 1);
        onTestObjectRemoved(*removedObjects.begin());
      });

    ListenerImpl::check2();
  }

private:
  std::shared_ptr<sen::ObjectSource> bus_;
  sen::ObjectList<TestObjectInterface> list_;
};

/// Listener that checks if static props are synchronized correctly
class ListenerStaticProps final: public ListenerObjectSyncImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerStaticProps)

public:
  using ListenerObjectSyncImpl::ListenerObjectSyncImpl;
  ~ListenerStaticProps() override = default;

protected:  // implements ListenerObjectSyncImpl
  void onTestObjectAdded(TestObjectInterface* obj) override
  {
    SEN_ASSERT(obj->getStaticProp() == staticPropValue);
    SEN_ASSERT(obj->getStaticNoConfigProp() == staticNoConfigPropValue);
    ListenerObjectSyncImpl::onTestObjectAdded(obj);
  }
};

SEN_EXPORT_CLASS(ListenerStaticProps)

/// Listener that checks if static props are synchronized correctly
class ListenerBestEffortProps final: public ListenerObjectSyncImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerBestEffortProps)

public:
  using ListenerObjectSyncImpl::ListenerObjectSyncImpl;
  ~ListenerBestEffortProps() override = default;

protected:
  void onTestObjectAdded(TestObjectInterface* obj) override
  {
    bestEffortPropUpdates_.reserve(numOfChecks);
    guard_ = obj->onBestEffortPropChanged(
      {this,
       [this, obj]()
       {
         if (!firstUpdateIndex_)
         {
           firstUpdateIndex_ = getFirstUpdateIndex(
             obj->getBestEffortProp(), [](std::mt19937& gen) { return std::uniform_real_distribution()(gen); });
         }

         bestEffortPropUpdates_.push_back(obj->getBestEffortProp());
         if (bestEffortPropUpdates_.size() == numOfChecks)
         {
           for (size_t i = 0; i < bestEffortPropUpdates_.size(); ++i)
           {
             // TODO: do we need to check this inside the loop?
             std::mt19937 gen {generatorSeed};
             gen.discard(*firstUpdateIndex_ + i);
             SEN_ASSERT(bestEffortPropUpdates_[i] - std::uniform_real_distribution()(gen) < 1e-6);
           }

           ListenerObjectSyncImpl::onTestObjectAdded(obj);
         }
       }});
  }

  void onTestObjectRemoved(TestObjectInterface* obj) override
  {
    guard_ = {};
    ListenerObjectSyncImpl::onTestObjectRemoved(obj);
  }

private:
  std::vector<float64_t> bestEffortPropUpdates_;
  sen::ConnectionGuard guard_;
  std::optional<size_t> firstUpdateIndex_ = std::nullopt;
};

SEN_EXPORT_CLASS(ListenerBestEffortProps)

/// Listener that checks if static props are synchronized correctly
class ListenerConfirmedProps final: public ListenerObjectSyncImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerConfirmedProps)

public:
  using ListenerObjectSyncImpl::ListenerObjectSyncImpl;
  ~ListenerConfirmedProps() override = default;

protected:  // implements ListenerObjectSyncImpl
  void onTestObjectAdded(TestObjectInterface* obj) override
  {
    confirmedPropUpdates_.reserve(numOfChecks);
    guard_ = obj->onConfirmedPropChanged({this,
                                          [this, obj]()
                                          {
                                            if (!firstUpdateIndex_)
                                            {
                                              firstUpdateIndex_ = getFirstUpdateIndex(obj->getConfirmedProp(),
                                                                                      [](std::mt19937& gen)
                                                                                      { return generateStruct(gen); });
                                            }

                                            confirmedPropUpdates_.push_back(obj->getConfirmedProp());

                                            if (confirmedPropUpdates_.size() == numOfChecks)
                                            {
                                              for (size_t i = 0; i < confirmedPropUpdates_.size(); ++i)
                                              {
                                                // TODO: do we need to check this inside the loop?

                                                std::mt19937 gen {generatorSeed};
                                                gen.discard(*firstUpdateIndex_ + i);
                                                SEN_ASSERT(confirmedPropUpdates_[i] == generateStruct(gen));
                                              }

                                              ListenerObjectSyncImpl::onTestObjectAdded(obj);
                                            }
                                          }});
  }

private:
  std::vector<TestStruct> confirmedPropUpdates_;
  sen::ConnectionGuard guard_;
  std::optional<size_t> firstUpdateIndex_ = std::nullopt;
};

SEN_EXPORT_CLASS(ListenerConfirmedProps)

/// Listener that checks if multicast props are synchronized correctly
class ListenerMulticastProps final: public ListenerObjectSyncImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerMulticastProps)

public:
  using ListenerObjectSyncImpl::ListenerObjectSyncImpl;
  ~ListenerMulticastProps() override = default;

protected:  // implements ListenerObjectSyncImpl
  void onTestObjectAdded(TestObjectInterface* obj) override
  {
    multicastPropUpdates_.reserve(numOfChecks);
    guard_ = obj->onMulticastPropChanged({this,
                                          [this, obj]()
                                          {
                                            if (!firstUpdateIndex_)
                                            {
                                              firstUpdateIndex_ =
                                                getFirstUpdateIndex(static_cast<float64_t>(*obj->getMulticastProp()),
                                                                    [](std::mt19937& gen)
                                                                    { return std::uniform_real_distribution()(gen); });
                                            }

                                            multicastPropUpdates_.push_back(obj->getMulticastProp());

                                            if (multicastPropUpdates_.size() == numOfChecks)
                                            {
                                              for (size_t i = 0; i < multicastPropUpdates_.size(); ++i)
                                              {
                                                // TODO: do i need to create the gen inside the loop?
                                                std::mt19937 gen {generatorSeed};
                                                gen.discard(*firstUpdateIndex_ + i);
                                                SEN_ASSERT(abs(static_cast<float64_t>(*multicastPropUpdates_[i]) -
                                                               std::uniform_real_distribution()(gen)) < 1e-4);
                                              }

                                              ListenerObjectSyncImpl::onTestObjectAdded(obj);
                                            }
                                          }});
  }

  void onTestObjectRemoved(TestObjectInterface* obj) override
  {
    guard_ = {};
    ListenerObjectSyncImpl::onTestObjectRemoved(obj);
  }

private:
  std::vector<OptF32> multicastPropUpdates_;
  sen::ConnectionGuard guard_;
  std::optional<size_t> firstUpdateIndex_ = std::nullopt;
};

SEN_EXPORT_CLASS(ListenerMulticastProps)

/// Listener that checks if writable props are synchronized . We just send the update ID in the writable prop directly
class ListenerWritableProps final: public ListenerObjectSyncImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerWritableProps)

public:
  using ListenerObjectSyncImpl::ListenerObjectSyncImpl;
  ~ListenerWritableProps() override = default;

public:
  void update(sen::kernel::RunApi& runApi) override
  {
    ListenerObjectSyncImpl::update(runApi);

    if (testObject_ != nullptr)
    {
      testObject_->setNextWritableProp({counter_++, std::uniform_int_distribution<uint64_t>()(gen_)});
    }
  }

protected:
  void onTestObjectAdded(TestObjectInterface* obj) override
  {
    testObject_ = obj;
    writablePropUpdates_.reserve(numOfChecks);
    guard_ = obj->onWritablePropChanged({this,
                                         [this, obj]()
                                         {
                                           writablePropUpdates_.push_back(obj->getWritableProp());
                                           if (writablePropUpdates_.size() == numOfChecks)
                                           {
                                             for (const auto& [id, value]: writablePropUpdates_)
                                             {
                                               std::mt19937_64 gen {generatorSeed};
                                               gen.discard(id);
                                               SEN_ASSERT(value == std::uniform_int_distribution<uint64_t>()(gen));
                                             }
                                             ListenerObjectSyncImpl::onTestObjectAdded(obj);
                                           }
                                         }});
  }

  void onTestObjectRemoved(TestObjectInterface* obj) override
  {
    testObject_ = nullptr;
    guard_ = {};
    ListenerObjectSyncImpl::onTestObjectRemoved(obj);
  }

private:
  uint32_t counter_ = 0U;
  TestObjectInterface* testObject_ = nullptr;
  std::mt19937_64 gen_ {generatorSeed};
  std::vector<WritablePropType> writablePropUpdates_;
  sen::ConnectionGuard guard_;
};

SEN_EXPORT_CLASS(ListenerWritableProps)

/// Listener that checks if best effort events are transmitted correctly
class ListenerBestEffortEvent final: public ListenerObjectSyncImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerBestEffortEvent)

public:
  using ListenerObjectSyncImpl::ListenerObjectSyncImpl;
  ~ListenerBestEffortEvent() override = default;

protected:
  void onTestObjectAdded(TestObjectInterface* obj) override
  {
    bestEffortEventData_.reserve(numOfChecks);
    guard_ = obj->onBestEffortEvent({this,
                                     [this, obj](u32 id, const TestStruct& value)
                                     {
                                       bestEffortEventData_.emplace(id, value);
                                       if (bestEffortEventData_.size() == numOfChecks)
                                       {
                                         for (const auto& event: bestEffortEventData_)
                                         {
                                           std::mt19937 gen {generatorSeed};
                                           gen.discard(event.first);
                                           SEN_ASSERT(generateStruct(gen) == event.second);
                                         }
                                         ListenerObjectSyncImpl::onTestObjectAdded(obj);
                                       }
                                     }});
  }

  void onTestObjectRemoved(TestObjectInterface* obj) override
  {
    guard_ = {};
    ListenerObjectSyncImpl::onTestObjectRemoved(obj);
  }

private:
  std::unordered_map<uint32_t, TestStruct> bestEffortEventData_;
  sen::ConnectionGuard guard_;
};

SEN_EXPORT_CLASS(ListenerBestEffortEvent)

/// Listener that checks if confirmed events are transmitted correctly
class ListenerConfirmedEvent final: public ListenerObjectSyncImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerConfirmedEvent)

public:
  using ListenerObjectSyncImpl::ListenerObjectSyncImpl;
  ~ListenerConfirmedEvent() override = default;

protected:
  void onTestObjectAdded(TestObjectInterface* obj) override
  {
    confirmedEventData_.reserve(numOfChecks);
    guard_ = obj->onConfirmedEvent({this,
                                    [this, obj](u32 id, const std::string& value)
                                    {
                                      confirmedEventData_.emplace(id, value);
                                      if (confirmedEventData_.size() == numOfChecks)
                                      {
                                        for (const auto& event: confirmedEventData_)
                                        {
                                          std::mt19937 gen {generatorSeed};
                                          gen.discard(event.first);
                                          SEN_ASSERT(generateString(gen) == event.second);
                                        }

                                        ListenerObjectSyncImpl::onTestObjectAdded(obj);
                                      }
                                    }});
  }

  void onTestObjectRemoved(TestObjectInterface* obj) override
  {
    guard_ = {};
    ListenerObjectSyncImpl::onTestObjectRemoved(obj);
  }

private:
  std::unordered_map<uint32_t, std::string> confirmedEventData_;
  sen::ConnectionGuard guard_;
};

SEN_EXPORT_CLASS(ListenerConfirmedEvent)

/// Listener that checks if multicast events are transmitted correctly
class ListenerMulticastEvent final: public ListenerObjectSyncImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerMulticastEvent)

public:
  using ListenerObjectSyncImpl::ListenerObjectSyncImpl;
  ~ListenerMulticastEvent() override = default;

protected:
  void onTestObjectAdded(TestObjectInterface* obj) override
  {
    multicastEventData_.reserve(numOfChecks);
    guard_ = obj->onMulticastEvent(
      {this,
       [this, obj](u32 id, const OptF32& value)
       {
         multicastEventData_.emplace(id, value);
         if (multicastEventData_.size() == numOfChecks)
         {
           for (const auto& event: multicastEventData_)
           {
             std::mt19937 gen {generatorSeed};
             gen.discard(event.first);
             SEN_ASSERT(static_cast<float64_t>(*event.second) - std::uniform_real_distribution()(gen) < 1e-6);
           }

           ListenerObjectSyncImpl::onTestObjectAdded(obj);
         }
       }});
  }

  void onTestObjectRemoved(TestObjectInterface* obj) override
  {
    guard_ = {};
    ListenerObjectSyncImpl::onTestObjectRemoved(obj);
  }

private:
  std::unordered_map<uint32_t, OptF32> multicastEventData_;
  sen::ConnectionGuard guard_;
};

SEN_EXPORT_CLASS(ListenerMulticastEvent)

/// Listener that checks if confirmed events are transmitted correctly
class ListenerLocalMethod final: public ListenerObjectSyncImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerLocalMethod)

public:
  using ListenerObjectSyncImpl::ListenerObjectSyncImpl;
  ~ListenerLocalMethod() override = default;

public:
  void update(sen::kernel::RunApi& runApi) override
  {
    ListenerObjectSyncImpl::update(runApi);

    if (testObject_ != nullptr)
    {
      returnedValues_.push_back(testObject_->localMethod());
      if (returnedValues_.size() == numOfChecks)
      {
        std::mt19937 gen {generatorSeed};
        for (const auto value: returnedValues_)
        {
          SEN_ASSERT(std::uniform_int_distribution<uint16_t>()(gen) == value);
        }

        ListenerObjectSyncImpl::onTestObjectAdded(testObject_);
      }
    }
  }

protected:
  void onTestObjectAdded(TestObjectInterface* obj) override
  {
    testObject_ = obj;
    returnedValues_.reserve(numOfChecks);
  }

  void onTestObjectRemoved(TestObjectInterface* obj) override
  {
    testObject_ = nullptr;
    ListenerObjectSyncImpl::onTestObjectRemoved(obj);
  }

private:
  TestObjectInterface* testObject_ = nullptr;
  std::vector<uint16_t> returnedValues_;
};

SEN_EXPORT_CLASS(ListenerLocalMethod)

/// Listener that checks if confirmed return correctly when called
// TODO (SEN-1783): Check for order correctness in the calls once the WorkQueue issue has been handled
class ListenerConstMethod final: public ListenerObjectSyncImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerConstMethod)

public:
  using ListenerObjectSyncImpl::ListenerObjectSyncImpl;
  ~ListenerConstMethod() override = default;

public:
  void update(sen::kernel::RunApi& runApi) override
  {
    ListenerObjectSyncImpl::update(runApi);

    if (testObject_ != nullptr && callCount_ < numOfChecks)
    {
      ++callCount_;
      testObject_->constMethod(generateEnum(gen_),
                               {this,
                                [this](const auto& response)
                                {
                                  returnValues_.push_back(response.getValue());
                                  if (returnValues_.size() == numOfChecks)
                                  {
                                    // generate expected series of results
                                    std::vector<u32> expectedReturnValues;
                                    expectedReturnValues.reserve(numOfChecks);
                                    std::mt19937 gen {generatorSeed};
                                    for (size_t i = 0; i < numOfChecks; ++i)
                                    {
                                      expectedReturnValues.push_back(static_cast<u32>(generateEnum(gen)));
                                    }
                                    // NOTE: this will not be needed once the work queue does not change the order of
                                    // the return values
                                    std::sort(returnValues_.begin(), returnValues_.end());
                                    std::sort(expectedReturnValues.begin(), expectedReturnValues.end());

                                    SEN_ASSERT(returnValues_ == expectedReturnValues);

                                    ListenerObjectSyncImpl::onTestObjectAdded(testObject_);
                                  }
                                }});
    }
  }

protected:
  void onTestObjectAdded(TestObjectInterface* obj) override
  {
    testObject_ = obj;
    returnValues_.reserve(numOfChecks);
  }

private:
  TestObjectInterface* testObject_ = nullptr;
  uint32_t callCount_ = 0U;
  std::vector<uint32_t> returnValues_;
  std::mt19937 gen_ {generatorSeed};
};

SEN_EXPORT_CLASS(ListenerConstMethod)

/// Listener that checks if confirmed methods return correctly when called
// TODO (SEN-1783): Check for order correctness in the calls once the WorkQueue issue has been handled
class ListenerConfirmedMethod final: public ListenerObjectSyncImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerConfirmedMethod)

public:
  using ListenerObjectSyncImpl::ListenerObjectSyncImpl;
  ~ListenerConfirmedMethod() override = default;

public:
  void update(sen::kernel::RunApi& runApi) override
  {
    ListenerObjectSyncImpl::update(runApi);

    if (testObject_ != nullptr && callCount_ < numOfChecks)
    {
      ++callCount_;
      testObject_->confirmedMethod(
        std::uniform_int_distribution<uint16_t>()(gen_),
        {this,
         [this](const auto& response)
         {
           methodResults_.push_back(response.getValue());

           if (methodResults_.size() == numOfChecks)
           {
             std::vector<u8> expectedReturnValues;
             expectedReturnValues.reserve(numOfChecks);
             std::mt19937_64 gen {generatorSeed};
             for (size_t i = 0; i < numOfChecks; ++i)
             {
               expectedReturnValues.push_back(static_cast<u8>(std::uniform_int_distribution<uint16_t>()(gen)));
             }

             std::sort(methodResults_.begin(), methodResults_.end());
             std::sort(expectedReturnValues.begin(), expectedReturnValues.end());
             SEN_ASSERT(methodResults_ == expectedReturnValues);
             ListenerObjectSyncImpl::onTestObjectAdded(testObject_);
           }
         }});
    }
  }

protected:
  void onTestObjectAdded(TestObjectInterface* obj) override
  {
    testObject_ = obj;
    methodResults_.reserve(numOfChecks);
  }

  void onTestObjectRemoved(TestObjectInterface* obj) override
  {
    testObject_ = nullptr;
    ListenerObjectSyncImpl::onTestObjectRemoved(obj);
  }

private:
  TestObjectInterface* testObject_ = nullptr;
  std::vector<u8> methodResults_;
  uint32_t callCount_ = 0U;
  std::mt19937_64 gen_ {generatorSeed};
};

SEN_EXPORT_CLASS(ListenerConfirmedMethod)

/// Listener that checks if best effort methods return correctly when called
// TODO (SEN-1783): Check for order correctness in the calls once the WorkQueue issue has been handled
class ListenerBestEffortMethod final: public ListenerObjectSyncImpl
{
public:
  SEN_NOCOPY_NOMOVE(ListenerBestEffortMethod)

public:
  using ListenerObjectSyncImpl::ListenerObjectSyncImpl;
  ~ListenerBestEffortMethod() override = default;

public:
  void update(sen::kernel::RunApi& runApi) override
  {
    ListenerObjectSyncImpl::update(runApi);

    if (testObject_ != nullptr && callCount_ < numOfChecks)
    {
      ++callCount_;
      testObject_->bestEffortMethod(
        generateString(gen_),
        {this,
         [this](const auto& response)
         {
           methodResults_.push_back(response.getValue());

           if (methodResults_.size() == numOfChecks)
           {
             std::vector<u64> expectedReturnValues;
             expectedReturnValues.reserve(numOfChecks);
             std::mt19937 gen {generatorSeed};
             for (size_t i = 0; i < numOfChecks; ++i)
             {
               expectedReturnValues.push_back(std::hash<std::string>()(generateString(gen)));
             }

             std::sort(methodResults_.begin(), methodResults_.end());
             std::sort(expectedReturnValues.begin(), expectedReturnValues.end());
             SEN_ASSERT(methodResults_ == expectedReturnValues);
             ListenerObjectSyncImpl::onTestObjectAdded(testObject_);
           }
         }});
    }
  }

protected:
  void onTestObjectAdded(TestObjectInterface* obj) override
  {
    testObject_ = obj;
    methodResults_.reserve(numOfChecks);
  }

  void onTestObjectRemoved(TestObjectInterface* obj) override
  {
    testObject_ = nullptr;
    ListenerObjectSyncImpl::onTestObjectRemoved(obj);
  }

private:
  TestObjectInterface* testObject_ = nullptr;
  std::vector<u64> methodResults_;
  uint32_t callCount_ = 0U;
  std::mt19937 gen_ {generatorSeed};
};

SEN_EXPORT_CLASS(ListenerBestEffortMethod)

}  // namespace object_sync
