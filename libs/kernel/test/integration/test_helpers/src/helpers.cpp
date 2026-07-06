// === helpers.cpp =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// test_helpers
#include "test_helpers/helpers.h"

#include "test_helpers/test_helpers.stl.h"

// sen
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/kernel/component_api.h"

// spdlog
#include <spdlog/logger.h>

// std
#include <array>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>

namespace sen::test
{

// ================================================================================================================
// ProcessTerminatorImpl
// ================================================================================================================

ProcessTerminatorImpl::ProcessTerminatorImpl(std::string name, const sen::VarMap& args)
  : ProcessTerminatorBase(name, args), logger_(sen::kernel::KernelApi::getOrCreateLogger(name))
{
}

void ProcessTerminatorImpl::registered(sen::kernel::RegistrationApi& api)
{
  objectsSub_ = api.selectAllFrom<StatefulObjectInterface>(
    "session.bus",
    [this, &api](const auto& objects)
    {
      for (auto* obj: objects)
      {
        guards_.emplace_back(obj->onStateChanged(
          {this,
           [this, obj, &api]()
           {
             logger_->info("{} received state change {} : {}",
                           getName(),
                           obj->asObject().getName(),
                           StringConversionTraits<ConnectionState>::toString(obj->getState()));

             logger_->info("{} current states:", getName());
             for (const auto* objectElement: objectsSub_->list.getObjects())
             {
               logger_->info("{} : {}",
                             objectElement->asObject().getName(),
                             StringConversionTraits<ConnectionState>::toString(objectElement->getState()));
             }
             if (obj->getState() == ConnectionState::finished &&
                 allObjectsWithState(objectsSub_->list.getObjects(), ConnectionState::finished))
             {
               logger_->info("{} stopping process", getName());
               api.requestKernelStop();
             }
           }}));
      }
    });
}

SEN_EXPORT_CLASS(ProcessTerminatorImpl)

// ================================================================================================================
// StatefulObjectImpl
// ================================================================================================================

StateFulObjectImpl::StateFulObjectImpl(std::string name, const sen::VarMap& args)
  : StatefulObjectBase(name, args), logger_(sen::kernel::KernelApi::getOrCreateLogger(name))
{
}

void StateFulObjectImpl::registered(sen::kernel::RegistrationApi& api) { api_ = &api; }

sen::kernel::KernelApi* StateFulObjectImpl::getApi() const { return api_; }
spdlog::logger* StateFulObjectImpl::getLogger() const { return logger_.get(); }

SEN_EXPORT_CLASS(StateFulObjectImpl)

// ================================================================================================================
// PublisherImpl
// ================================================================================================================

PublisherImpl::PublisherImpl(std::string name, const sen::VarMap& args): PublisherBase(std::move(name), args) {}

void PublisherImpl::registered(sen::kernel::RegistrationApi& api)
{
  StateFulObjectImpl::registered(api);

  bus_ = api.getSource("session.bus");

  // detect listeners
  listenersSub_ = api.selectAllFrom<ListenerInterface>(
    "session.bus",
    [this](const auto& listeners)
    {
      cbGuards_.reserve(std::distance(listeners.begin(), listeners.end()));

      for (auto* listener: listeners)
      {
        getLogger()->info("{} detected {} : {}",
                          getName(),
                          listener->asObject().getName(),
                          StringConversionTraits<ConnectionState>::toString(listener->getState()));

        cbGuards_.emplace_back(listener->onStateChanged(
          {this,
           [this, listener]()
           {
             constexpr std::array<ConnectionState, 5> actionStates {ConnectionState::step1,
                                                                    ConnectionState::step2,
                                                                    ConnectionState::step3,
                                                                    ConnectionState::step4,
                                                                    ConnectionState::finished};

             getLogger()->info("{} detected : {} state changed to {}",
                               getName(),
                               listener->asObject().getName(),
                               StringConversionTraits<ConnectionState>::toString(listener->getState()));

             for (size_t i = 0; i < 5U; ++i)
             {
               if (allObjectsWithState(listenersSub_->list.getObjects(), actionStates.at(i)))
               {
                 getLogger()->info("{} detected all listeners with state {}",
                                   getName(),
                                   StringConversionTraits<ConnectionState>::toString(actionStates.at(i)));
                 actionFlags_.at(i) = true;
               }
             }
           }}));
      }

      if (getState() == ConnectionState::starting && listenersSub_->list.getObjects().size() == getNumOfListeners())
      {
        getLogger()->info("{} -> ready", getName());
        setNextState(ConnectionState::ready);
      }
    });
}

void PublisherImpl::update(sen::kernel::RunApi& runApi)
{
  std::ignore = runApi;

  getLogger()->info(
    "{} updating with state {}", getName(), StringConversionTraits<ConnectionState>::toString(getState()));

  constexpr std::array<void (PublisherImpl::*)(), 4> actions = {
    &PublisherImpl::action1, &PublisherImpl::action2, &PublisherImpl::action3, &PublisherImpl::action4};

  for (size_t i = 0; i < 5U; ++i)
  {
    if (auto& flag = actionFlags_.at(i); flag)
    {
      if (i < 4U)
      {
        (this->*actions.at(i))();
      }
      else
      {
        getLogger()->info("{} -> finished", getName());
        setNextState(ConnectionState::finished);
      }

      flag = false;
    }
  }
}

void PublisherImpl::action1()
{
  getLogger()->info("{} -> step1", getName());
  setNextState(ConnectionState::step1);
}

void PublisherImpl::action2()
{
  getLogger()->info("{} -> step2", getName());
  setNextState(ConnectionState::step2);
}

void PublisherImpl::action3()
{
  getLogger()->info("{} -> step3", getName());
  setNextState(ConnectionState::step3);
}

void PublisherImpl::action4()
{
  getLogger()->info("{} -> step4", getName());
  setNextState(ConnectionState::step4);
}

SEN_EXPORT_CLASS(PublisherImpl)

// ================================================================================================================
// ListenerImpl
// ================================================================================================================

ListenerImpl::ListenerImpl(std::string name, const sen::VarMap& args): ListenerBase(std::move(name), args) {}

void ListenerImpl::check1()
{
  getLogger()->info("{} -> step2", getName());
  setNextState(ConnectionState::step2);
}

void ListenerImpl::check2()
{
  getLogger()->info("{} -> step3", getName());
  setNextState(ConnectionState::step3);
}

void ListenerImpl::check3()
{
  getLogger()->info("{} -> step4", getName());
  setNextState(ConnectionState::step4);
}

void ListenerImpl::check4()
{
  getLogger()->info("{} -> finished", getName());
  setNextState(ConnectionState::finished);
}

void ListenerImpl::registered(sen::kernel::RegistrationApi& api)
{
  StateFulObjectImpl::registered(api);

  bus_ = api.getSource("session.bus");
  publisherSub_ = api.selectAllFrom<PublisherInterface>(
    "session.bus",
    [this](const auto& publishers)
    {
      cbGuards_.reserve(std::distance(publishers.begin(), publishers.end()));
      for (auto* publisher: publishers)
      {
        getLogger()->info("{} detected {} : {}",
                          getName(),
                          publisher->asObject().getName(),
                          StringConversionTraits<ConnectionState>::toString(publisher->getState()));

        cbGuards_.emplace_back(publisher->onStateChanged(
          {this,
           [this, publisher]()
           {
             constexpr std::array<ConnectionState, 5> checkStates {ConnectionState::ready,
                                                                   ConnectionState::step1,
                                                                   ConnectionState::step2,
                                                                   ConnectionState::step3,
                                                                   ConnectionState::step4};

             getLogger()->info("{} detected : {} state changed to {}",
                               getName(),
                               publisher->asObject().getName(),
                               StringConversionTraits<ConnectionState>::toString(publisher->getState()));
             for (size_t i = 0; i < 5U; ++i)
             {
               if (allObjectsWithState(publisherSub_->list.getObjects(), checkStates.at(i)))
               {
                 getLogger()->info("{} detected all publishers with state {}",
                                   getName(),
                                   StringConversionTraits<ConnectionState>::toString(checkStates.at(i)));
                 checkFlags_.at(i) = true;
               }
             }
           }}));
      }

      if (allObjectsWithState(publisherSub_->list.getObjects(), ConnectionState::ready))
      {
        getLogger()->info("{} -> step1", getName());
        setNextState(ConnectionState::step1);
      }
    });
}

void ListenerImpl::update(sen::kernel::RunApi& runApi)
{
  std::ignore = runApi;

  getLogger()->info(
    "{} updating with state {}", getName(), StringConversionTraits<ConnectionState>::toString(getState()));

  constexpr std::array<void (ListenerImpl::*)(), 4> checks = {
    &ListenerImpl::check1, &ListenerImpl::check2, &ListenerImpl::check3, &ListenerImpl::check4};

  for (size_t i = 0; i < 5U; ++i)
  {
    if (auto& flag = checkFlags_.at(i); flag)
    {
      if (i == 0U)
      {
        getLogger()->info("{} -> step1", getName());
        setNextState(ConnectionState::step1);
      }
      else
      {
        (this->*checks.at(i - 1))();
      }

      flag = false;
    }
  }
}

SEN_EXPORT_CLASS(ListenerImpl)

}  // namespace sen::test
