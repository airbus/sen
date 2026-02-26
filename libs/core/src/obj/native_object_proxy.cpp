// === native_object_proxy.cpp =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/obj/detail/native_object_proxy.h"

// sen
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/detail/types_fwd.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/core/obj/detail/event_buffer.h"
#include "sen/core/obj/detail/proxy_object.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object.h"

// std
#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>

namespace sen::impl
{

NativeObjectProxy::NativeObjectProxy(NativeObject* owner, std::string_view localPrefix)
  : owner_(owner)
  , guard_(owner_->shared_from_this())
  , localName_(senImplComputeLocalName(owner->getName(), localPrefix))
{
}

bool NativeObjectProxy::isRemote() const noexcept { return false; }

ObjectId NativeObjectProxy::getId() const noexcept { return owner_->getId(); }

const std::string& NativeObjectProxy::getName() const noexcept { return owner_->getName(); }

ConstTypeHandle<ClassType> NativeObjectProxy::getClass() const noexcept { return owner_->getClass(); }

const ProxyObject* NativeObjectProxy::asProxyObject() const noexcept { return this; }

ProxyObject* NativeObjectProxy::asProxyObject() noexcept { return this; }

RemoteObject* NativeObjectProxy::asRemoteObject() noexcept { return nullptr; }

const RemoteObject* NativeObjectProxy::asRemoteObject() const noexcept { return nullptr; }

ConnectionGuard NativeObjectProxy::onEventUntyped(const Event* ev, EventCallback<VarList>&& callback)
{
  /// For regular events, we rely on the owner.
  return owner_->onEventUntyped(ev, std::move(callback));
}

// NOLINTNEXTLINE
void NativeObjectProxy::invokeUntyped(const Method* method, const VarList& args, MethodCallback<Var>&& onDone)
{
  owner_->invokeUntyped(method, args, std::move(onDone));
}

ConnectionGuard NativeObjectProxy::onPropertyChangedUntyped(const Property* prop, EventCallback<VarList>&& callback)
{
  std::scoped_lock<std::recursive_mutex> lock(eventCallbacksMutex_);

  if (!eventCallbacks_)
  {
    eventCallbacks_ = std::make_unique<EventCallbackMap>();
  }

  if (auto callbackLock = callback.lock(); callbackLock.isValid())
  {
    const auto connId = senImplMakeConnectionId();
    const auto memberHash = prop->getId();

    auto [itr, done] = eventCallbacks_->try_emplace(memberHash, EventData {{}, prop->getTransportMode()});
    itr->second.list.push_back({connId.get(), std::move(callback)});

    return senImplMakeConnectionGuard(connId, memberHash, false);
  }

  return senImplMakeConnectionGuard(0, 0, false);
}

void NativeObjectProxy::invokeAllPropertyCallbacks() { owner_->invokeAllPropertyCallbacks(); }

const std::string& NativeObjectProxy::getLocalName() const noexcept { return localName_; }

TimeStamp NativeObjectProxy::getLastCommitTime() const noexcept { return lastCommitTime_; }

ConnId NativeObjectProxy::senImplMakeConnectionId() const noexcept { return owner_->senImplMakeConnectionId(); }

Var NativeObjectProxy::getPropertyUntyped(const Property* prop) const { return senImplGetPropertyImpl(prop->getId()); }

void NativeObjectProxy::senImplEventEmitted(MemberHash id, std::function<VarList()>&& argsGetter, const EventInfo& info)
{
  std::scoped_lock<std::recursive_mutex> lock(eventCallbacksMutex_);

  if (!eventCallbacks_)
  {
    return;
  }

  auto itr = eventCallbacks_->find(id);
  if (itr != eventCallbacks_->end())
  {
    const auto& eventData = itr->second;

    if (!eventData.list.empty())
    {
      const auto args = argsGetter();
      for (const auto& elem: eventData.list)
      {
        if (auto callbackLock = elem.callback.lock(); callbackLock.isValid())
        {
          callbackLock.pushAnswer([args, info, func = &elem.callback]() { func->invoke(info, args); },
                                  impl::cannotBeDropped(eventData.transportMode));
        }
      }
    }
  }
}

void NativeObjectProxy::removeTypedConnectionOnOwner(ConnId connId) { owner_->senImplRemoveTypedConnection(connId); }

void NativeObjectProxy::senImplRemoveUntypedConnection(ConnId id, MemberHash memberHash)
{
  std::scoped_lock<std::recursive_mutex> lock(eventCallbacksMutex_);

  if (eventCallbacks_)
  {
    auto itr = eventCallbacks_->find(memberHash.get());
    if (itr != eventCallbacks_->end())
    {
      auto& callbackList = itr->second.list;

      callbackList.erase(
        std::remove_if(
          callbackList.begin(), callbackList.end(), [&](const auto& elem) -> bool { return elem.id == id.get(); }),
        callbackList.end());

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

  owner_->senImplRemoveUntypedConnection(id, memberHash);
}

void NativeObjectProxy::drainInputs()
{
  lastCommitTime_ = owner_->getLastCommitTime();

  {  // ensure that the properties of the owner are currenlty not modified
    auto readLock = owner_->createReaderLock();
    drainInputsImpl(lastCommitTime_);
  }
}

void NativeObjectProxy::sendWork(std::function<void()>&& work, bool forcePush) const
{
  owner_->addWorkToQueue(std::move(work), forcePush);
}

}  // namespace sen::impl
