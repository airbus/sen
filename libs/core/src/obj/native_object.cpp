// === native_object.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/obj/native_object.h"

// sen
#include "sen/core/base/move_only_function.h"
#include "sen/core/base/result.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/core/obj/detail/event_buffer.h"
#include "sen/core/obj/detail/native_object_impl.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/object.h"

// std
#include <algorithm>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace sen
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace impl
{

[[nodiscard]] SerializableEventQueue& getDefaultSerializableEventQueue() noexcept
{
  static SerializableEventQueue result(0, true);
  return result;
}

[[nodiscard]] WorkQueue& getDefaultWorkQueue() noexcept
{
  static WorkQueue result(0, true);
  return result;
}

}  // namespace impl

//--------------------------------------------------------------------------------------------------------------
// NativeObject
//--------------------------------------------------------------------------------------------------------------

NativeObject::NativeObject(const std::string& name)
  : workQueue_(&impl::getDefaultWorkQueue())
  , outputEventQueue_(&impl::getDefaultSerializableEventQueue())
  , postCommitEvents_(0, true)
  , id_(senImplMakeId(name))
  , name_(name)
  , localName_(name)
{
  senImplValidateName(name);
}

void NativeObject::senImplAsyncCall(const Method* method, const VarList& args, MethodCallback<Var>&& callback)
{
  bool forcePush = impl::cannotBeDropped(method->getTransportMode());

  addWorkToQueue(
    [this,
     usWeak = weak_from_this(),
     methodId = method->getId(),
     list = args,
     responseCallback = std::move(callback),
     forcePush]() mutable
    {
      if (auto usLock = usWeak.lock(); usLock)
      {
        auto forwarder = [forcePush, responseCallback = std::move(responseCallback)](VariantCall&& call) mutable
        {
          try
          {
            Var result;
            call(result);

            if (auto lock = responseCallback.lock(); lock.isValid())
            {
              auto work = [ret = result, rcb = std::move(responseCallback)]() mutable { rcb.invoke({}, Ok(ret)); };
              lock.pushAnswer(std::move(work), forcePush);
            }
          }
          catch (const std::runtime_error& err)
          {
            if (auto lock = responseCallback.lock(); lock.isValid())
            {
              const auto errPtr = std::make_exception_ptr(err);
              lock.pushAnswer([rcb = std::move(responseCallback), errPtr]() mutable { rcb.invoke({}, Err(errPtr)); },
                              forcePush);
            }
          }
          catch (...)
          {
            if (auto lock = responseCallback.lock(); lock.isValid())
            {
              auto err = std::current_exception();
              lock.pushAnswer([rcb = std::move(responseCallback), err]() mutable { rcb.invoke({}, Err(err)); },
                              forcePush);
            }
          }
        };

        senImplVariantCall(methodId, list, std::move(forwarder));
      }
      else
      {
        if (auto lock = responseCallback.lock(); lock.isValid())
        {
          auto err = std::make_exception_ptr(std::runtime_error("object deleted"));
          lock.pushAnswer([rcb = std::move(responseCallback), err]() mutable { rcb.invoke({}, Err(err)); }, forcePush);
        }
      }
    },
    forcePush);
}

void NativeObject::setQueues(impl::WorkQueue* workQueue, impl::SerializableEventQueue* eventQueue) noexcept
{
  if (workQueue == nullptr)
  {
    workQueue_.store(&impl::getDefaultWorkQueue());
  }
  else
  {
    workQueue_.store(workQueue);
  }

  if (eventQueue == nullptr)
  {
    outputEventQueue_.store(&impl::getDefaultSerializableEventQueue());
  }
  else
  {
    outputEventQueue_.store(eventQueue);
  }

  if (workQueue != nullptr && eventQueue != nullptr)
  {
    postCommitEvents_.enable();
  }
  else
  {
    postCommitEvents_.disable();
  }
}

void NativeObject::senImplRemoveUntypedConnection(ConnId id, MemberHash memberHash)
{
  std::scoped_lock<std::recursive_mutex> lock(eventCallbacksMutex_);

  if (!eventCallbacks_)
  {
    return;
  }

  auto itr = eventCallbacks_->find(memberHash.get());
  if (itr != eventCallbacks_->end())
  {
    auto& callbackList = itr->second.list;

    bool found = false;
    do
    {
      // find the callback in the list
      auto callbackListItr = std::find_if(
        callbackList.begin(), callbackList.end(), [&](const auto& elem) -> bool { return elem.id == id.get(); });

      found = callbackListItr != callbackList.end();

      // if found, invalidate it, and erase it
      if (found)
      {
        callbackListItr->callback->lock().invalidate();
        callbackList.erase(callbackListItr);
      }
    } while (found);

    // delete the entry for this member if empty
    if (callbackList.empty())
    {
      eventCallbacks_->erase(itr);
    }

    // delete the callbacks container if empty
    if (eventCallbacks_->empty())
    {
      eventCallbacks_ = {};
    }
  }
}

ConnectionGuard NativeObject::onEventUntyped(const Event* ev, EventCallback<VarList>&& callback)
{
  std::scoped_lock<std::recursive_mutex> lock(eventCallbacksMutex_);

  if (!eventCallbacks_)
  {
    eventCallbacks_ = std::make_unique<EventCallbackMap>();
  }

  if (const auto callbackLock = callback.lock(); callbackLock.isValid())
  {
    const auto connId = nextConnection_.fetch_add(1U);
    const auto memberHash = ev->getId();
    auto [itr, done] = eventCallbacks_->try_emplace(memberHash, EventCallbackInfo {{}, ev->getTransportMode()});
    itr->second.list.push_back({connId, std::make_shared<EventCallback<VarList>>(std::move(callback))});
    return senImplMakeConnectionGuard(connId, memberHash, false);
  }

  return senImplMakeConnectionGuard(0, {}, false);
}

ConnectionGuard NativeObject::onPropertyChangedUntyped(const Property* prop, EventCallback<VarList>&& callback)
{
  std::scoped_lock<std::recursive_mutex> lock(eventCallbacksMutex_);

  if (!eventCallbacks_)
  {
    eventCallbacks_ = std::make_unique<EventCallbackMap>();
  }

  if (const auto callbackLock = callback.lock(); callbackLock.isValid())
  {
    const auto connId = nextConnection_.fetch_add(1U);
    const auto memberHash = prop->getId();
    auto [itr, done] = eventCallbacks_->try_emplace(memberHash, EventCallbackInfo {{}, prop->getTransportMode()});
    itr->second.list.push_back({connId, std::make_shared<EventCallback<VarList>>(std::move(callback))});
    return senImplMakeConnectionGuard(connId, memberHash, false);
  }

  return senImplMakeConnectionGuard(0, 0, false);
}

void NativeObject::senImplEventEmitted(MemberHash id, std::function<VarList()>&& argsGetter, const EventInfo& info)
{
  std::scoped_lock<std::recursive_mutex> lock(eventCallbacksMutex_);

  if (!eventCallbacks_)
  {
    return;
  }

  auto itr = eventCallbacks_->find(id);
  if (itr != eventCallbacks_->end())
  {
    const auto& eventCallbackInfo = itr->second;
    if (!eventCallbackInfo.list.empty())
    {
      const auto args = argsGetter();
      for (const auto& elem: eventCallbackInfo.list)
      {
        if (const auto callbackLock = elem.callback->lock(); callbackLock.isValid())
        {
          callbackLock.pushAnswer(
            [args, info, callbackWeakPtr = elem.callback->weak_from_this(), funcPtr = elem.callback.get()]() mutable
            {
              if (auto callbackPointer = callbackWeakPtr.lock(); callbackPointer)
              {
                funcPtr->invoke(info, args);
              }
            },
            impl::cannotBeDropped(eventCallbackInfo.transportMode));
        }
      }
    }
  }
}

//--------------------------------------------------------------------------------------------------------------
// NativeObject
//--------------------------------------------------------------------------------------------------------------

NativeObject::~NativeObject() = default;

void NativeObject::registered(kernel::RegistrationApi& api) { std::ignore = api; }

void NativeObject::update(kernel::RunApi& runApi) { std::ignore = runApi; }

void NativeObject::unregistered(kernel::RegistrationApi& api) { std::ignore = api; }

NativeObject* NativeObject::asNativeObject() noexcept { return this; }

const NativeObject* NativeObject::asNativeObject() const noexcept { return this; }

const std::string& NativeObject::getName() const noexcept { return name_; }

Var NativeObject::getPropertyUntyped(const Property* prop) const { return senImplGetPropertyImpl(prop->getId()); }

void NativeObject::setNextPropertyUntyped(const Property* property, const Var& value)
{
  senImplSetNextPropertyUntyped(property->getId(), value);
}

Var NativeObject::getNextPropertyUntyped(const Property* property) const
{
  return senImplGetNextPropertyUntyped(property->getId());
}

void NativeObject::commit(TimeStamp time)
{
  auto writeLock = createWriterLock();
  lastCommitTime_ = time;
  senImplCommitImpl(time);
  std::ignore = postCommitEvents_.executeAll();
}

ObjectId NativeObject::getId() const noexcept { return id_; }

ConnId NativeObject::senImplMakeConnectionId() noexcept { return nextConnection_.fetch_add(1U); }

impl::SerializableEventQueue* NativeObject::getOutputEventQueue() noexcept { return outputEventQueue_.load(); }

void NativeObject::setLocalPrefix(std::string_view localPrefix) noexcept
{
  localName_ = senImplComputeLocalName(getName(), localPrefix);
}

const std::string& NativeObject::getLocalName() const noexcept { return localName_; }

TimeStamp NativeObject::getLastCommitTime() const noexcept
{
  auto readLock = createReaderLock();
  return lastCommitTime_;
}

void NativeObject::addWorkToQueue(sen::std_util::move_only_function<void()>&& call, bool forcePush) const
{
  auto* queue = workQueue_.load();
  queue->push(std::move(call), forcePush);
}

}  // namespace sen
