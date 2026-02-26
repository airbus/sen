// === remote_object.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/obj/detail/remote_object.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/memory_block.h"
#include "sen/core/base/result.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/core/obj/detail/event_buffer.h"
#include "sen/core/obj/detail/proxy_object.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_provider.h"

// spdlog
#include <spdlog/spdlog.h>

// std
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace sen::impl
{

// NOLINTNEXTLINE(google-default-arguments, readability-function-size)
void RemoteObject::invokeUntyped(const Method* method, const VarList& args, MethodCallback<Var>&& onDone)
{
  const auto metaArgs = method->getArgs();
  const auto methodId = method->getId();
  const auto numOfArgs = metaArgs.size();
  const auto mode = method->getTransportMode();

  SEN_ASSERT(args.size() == numOfArgs);

  // create a buffer containing the arguments
  auto argsBuffer = std::make_shared<ResizableHeapBlock>();

  if (!method->getArgs().empty())
  {
    argsBuffer->reserve(args.size() * 14U);  // reserve some space for each argument NOLINT
    ResizableBufferWriter writer(*argsBuffer);
    OutputStream out(writer);

    if (info_.writerSchema)
    {
      for (size_t i = 0; i < numOfArgs; ++i)
      {
        writeToStream(args[i], out, *metaArgs[i].type);
      }
    }
    else
    {
      serializeMethodArgumentsFromVariants(methodId, args, out);
    }
  }

  // send the call and get a ticket
  if (auto callId = sendCall(mode, info_.ownerId, info_.id, method->getId(), std::move(argsBuffer)); callId.isOk())
  {
    // when receiving the response, add the callback to the work queue
    auto responseHandler = [cb = std::move(onDone),
                            returnType = method->getReturnType(),
                            id = method->getId(),
                            us = weak_from_this(),
                            this,
                            mode](const auto& response) mutable
    {
      // if we are still alive, try to send a response to the caller
      if (auto ourselves = us.lock(); ourselves)
      {
        // if there's a callback, notify the user about the result
        if (auto lock = cb.lock(); lock.isValid())
        {
          auto result = makeMethodResult<Var>(response,
                                              [returnType, id, this](const auto& buffer)
                                              {
                                                if (returnType->isVoidType())
                                                {
                                                  return Ok(Var {});
                                                }

                                                InputStream in(buffer);
                                                return Ok(deserializeMethodReturnValueAsVariant(id, in));
                                              });

          lock.pushAnswer([res = std::move(result), callback = std::move(cb)]() mutable { callback.invoke({}, res); },
                          impl::cannotBeDropped(mode));
        }
      }
      else
      {
        // if we are gone, notify the user (if there's a callback)
        if (auto lock = cb.lock(); lock.isValid())
        {
          // the remote object is gone
          lock.pushAnswer(
            [callback = std::move(cb)]() mutable
            { callback.invoke({}, ::sen::Err(std::make_exception_ptr(std::runtime_error("object deleted")))); },
            impl::cannotBeDropped(mode));
        }
      }
    };

    MutexLock pendingResponsesLock(pendingResponsesMutex_);
    pendingResponses_.emplace(callId.getValue(), std::move(responseHandler));
  }
  else
  {
    // we have a failed call, notify the user, if there's a callback
    if (auto callbackLock = onDone.lock(); callbackLock.isValid())
    {
      onDone.invoke({}, ::sen::Err(std::make_exception_ptr(std::runtime_error(callId.getError()))));
    }
  }
}

ConnectionGuard RemoteObject::onEventUntyped(const Event* ev, EventCallback<VarList>&& callback)
{
  std::scoped_lock<std::recursive_mutex> lock(eventCallbacksMutex_);

  if (!eventCallbacks_)
  {
    eventCallbacks_ = std::make_unique<EventCallbackMap>();
  }

  if (auto callbackLock = callback.lock(); callbackLock.isValid())
  {
    const auto connId = nextConnection_.fetch_add(1U);
    const auto memberHash = ev->getId();
    auto [itr, done] = eventCallbacks_->try_emplace(memberHash, EventCallbackInfo {{}, ev->getTransportMode()});
    itr->second.list.push_back({connId, std::move(callback)});
    return senImplMakeConnectionGuard(connId, memberHash, false);
  }

  return senImplMakeConnectionGuard(0, {}, false);
}

ConnectionGuard RemoteObject::onPropertyChangedUntyped(const Property* prop, EventCallback<VarList>&& callback)
{
  std::scoped_lock<std::recursive_mutex> lock(eventCallbacksMutex_);

  if (!eventCallbacks_)
  {
    eventCallbacks_ = std::make_unique<EventCallbackMap>();
  }

  if (auto callbackLock = callback.lock(); callbackLock.isValid())
  {
    const auto connId = nextConnection_.fetch_add(1U);
    const auto memberHash = prop->getId();
    auto [itr, done] = eventCallbacks_->try_emplace(memberHash, EventCallbackInfo {{}, prop->getTransportMode()});
    itr->second.list.push_back({connId, std::move(callback)});

    return senImplMakeConnectionGuard(connId, memberHash, false);
  }

  return senImplMakeConnectionGuard(0, {}, false);
}

void RemoteObject::senImplRemoveUntypedConnection(ConnId id, MemberHash memberHash)
{
  std::scoped_lock<std::recursive_mutex> lock(eventCallbacksMutex_);

  if (!eventCallbacks_)
  {
    return;
  }

  auto itr = eventCallbacks_->find(memberHash.get());
  if (itr != eventCallbacks_->end())
  {
    auto& list = itr->second.list;
    for (auto elemItr = list.begin(); elemItr != list.end(); ++elemItr)
    {
      if (elemItr->id == id.get())
      {
        elemItr->callback.lock().invalidate();
        list.erase(elemItr);
        return;
      }
    }
  }
}

void RemoteObject::senImplEventEmitted(MemberHash id, std::function<VarList()>&& argsGetter, const EventInfo& info)
{
  if (!eventCallbacks_)
  {
    return;
  }

  auto us = shared_from_this();
  {
    std::scoped_lock<std::recursive_mutex> lock(eventCallbacksMutex_);

    if (auto itr = eventCallbacks_->find(id); itr != eventCallbacks_->end())
    {
      const auto args = argsGetter();

      // iterate over all the registered callbacks
      for (const auto& elem: itr->second.list)
      {
        if (auto callbackLock = elem.callback.lock(); callbackLock.isValid())
        {
          callbackLock.pushAnswer([argsGetter, info, func = &elem.callback, us]() { func->invoke(info, argsGetter()); },
                                  impl::cannotBeDropped(itr->second.transportMode));
        }
      }
    }
  }
}

void RemoteObject::initializeState(Span<const uint8_t> state, const TimeStamp& time)
{
  InputStream in(state);

  uint32_t id = 0U;
  uint32_t size = 0U;

  // read all the properties from the stream
  while (!in.atEnd())
  {
    in.readUInt32(id);
    in.readUInt32(size);

    if (!readPropertyFromStream(id, in, true))
    {
      std::ignore = in.advance(size);
      SPDLOG_TRACE("could not find matching property with id {} and size {} in the reading schema - ignoring", id);
    }
  }

  lastCommitTime_ = time;
}

bool RemoteObject::responseReceived(MethodResponseData& response)
{
  PendingResponseContainerType::node_type pendingResponseHandle;
  {  // try to find a matching handle
    MutexLock pendingResponsesLock(pendingResponsesMutex_);
    auto itr = pendingResponses_.find(response.callId);
    if (itr == pendingResponses_.end())
    {
      return false;
    }

    pendingResponseHandle = pendingResponses_.extract(itr);
  }

  // put the response handling in the work queue of the caller
  pendingResponseHandle.mapped()(std::move(response));
  return true;
}

void RemoteObject::propertyUpdatesReceived(const Span<const uint8_t>& properties, TimeStamp commitTime)
{
  const std::lock_guard lock(*this);
  InputStream in(properties);

  uint32_t id;
  uint32_t size;

  while (!in.atEnd())
  {
    in.readUInt32(id);
    in.readUInt32(size);

    if (!readPropertyFromStream(id, in, false))
    {
      std::ignore = in.advance(size);
      SPDLOG_TRACE("could not update matching property with id {} and size {} in the reading schema - ignoring", id);
    }
  }

  lastCommitTime_ = commitTime;
}

RemoteObject::RemoteObject(RemoteObjectInfo&& info): info_(std::move(info)) {}

RemoteObject::~RemoteObject()
{
  if (info_.destructionCallback)
  {
    info_.destructionCallback(this);
  }
}

void RemoteObject::copyStateFrom(const RemoteObject& other) { lastCommitTime_ = other.getLastCommitTime(); }

bool RemoteObject::isRemote() const noexcept { return true; }

void RemoteObject::clearDestructionCallback() noexcept { info_.destructionCallback = nullptr; }

ObjectId RemoteObject::getId() const noexcept { return info_.id; }

const std::string& RemoteObject::getName() const noexcept { return info_.name; }

const std::string& RemoteObject::getLocalName() const noexcept { return info_.localName; }

TimeStamp RemoteObject::getLastCommitTime() const noexcept { return lastCommitTime_.load(); }

ConstTypeHandle<ClassType> RemoteObject::getClass() const noexcept { return info_.type; }

const ProxyObject* RemoteObject::asProxyObject() const noexcept { return this; }

ProxyObject* RemoteObject::asProxyObject() noexcept { return this; }

RemoteObject* RemoteObject::asRemoteObject() noexcept { return this; }

const RemoteObject* RemoteObject::asRemoteObject() const noexcept { return this; }

Var RemoteObject::getPropertyUntyped(const Property* prop) const { return senImplGetPropertyImpl(prop->getId()); }

ConnId RemoteObject::senImplMakeConnectionId() noexcept { return {nextConnection_.fetch_add(1U)}; }

bool RemoteObject::checkMemberTypesInSyncInDetail(MemberHash id) const noexcept
{
  auto readerSchema = getClass();

  // search properties
  if (const auto* prop = readerSchema->searchPropertyById(id); prop != nullptr)
  {
    SEN_ASSERT(info_.writerSchema.has_value() && "WriterSchema is expected");
    if (const auto* writerProp = info_.writerSchema.value()->searchPropertyById(id);
        writerProp != nullptr && writerProp->getHash() == prop->getHash())
    {
      return true;
    }
  }

  // search methods
  if (const auto* method = readerSchema->searchMethodById(id); method != nullptr)
  {
    SEN_ASSERT(info_.writerSchema.has_value() && "WriterSchema is expected");
    if (const auto* writerMethod = info_.writerSchema.value()->searchMethodById(id);
        writerMethod != nullptr && writerMethod->getHash() == method->getHash())
    {
      return true;
    }
  }

  // search events
  if (const auto* event = readerSchema->searchEventById(id); event != nullptr)
  {
    SEN_ASSERT(info_.writerSchema.has_value() && "WriterSchema is expected");
    if (const auto* writerEvent = info_.writerSchema.value()->searchEventById(id);
        writerEvent != nullptr && writerEvent->getHash() == event->getHash())
    {
      return true;
    }
  }

  return false;
}

void RemoteObject::logEventArgsWarning(const std::string& eventName,
                                       size_t numOfWriterEventArgs,
                                       size_t numOfReaderEventArgs)
{
  SPDLOG_WARN(
    "Cannot dispatch incoming event {}. Remote object implementation has {} arguments. The local "
    "implementation has {} arguments.",
    eventName,
    numOfWriterEventArgs,
    numOfReaderEventArgs);
}

std::weak_ptr<const kernel::impl::RemoteParticipant> RemoteObject::getParticipant() const
{
  return info_.remoteParticipant;
}

MaybeConstTypeHandle<ClassType> RemoteObject::getWriterSchema() const noexcept { return info_.writerSchema; }

Result<CallId, std::string> RemoteObject::sendCall(TransportMode mode,
                                                   ObjectOwnerId ownerId,
                                                   ObjectId receiverId,
                                                   MemberHash id,
                                                   MemBlockPtr&& args) const
{

  if (info_.sendCallFunc)
  {
    return info_.sendCallFunc(mode, ownerId, receiverId, id, std::move(args));
  }
  return Err(std::string("invalidated transport"));
}

void RemoteObject::cancelPendingCalls()
{
  MutexLock lock(pendingResponsesMutex_);

  for (auto& [id, callback]: pendingResponses_)
  {
    MethodResponseData responseData {};
    responseData.callId = id;
    responseData.result = RemoteCallResult::runtimeError;
    responseData.error = "connection lost";

    // this will put this response in the caller's work queue
    callback(std::move(responseData));
  }
}

}  // namespace sen::impl
