// === remote_object.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_DETAIL_REMOTE_OBJECT_H
#define SEN_CORE_OBJ_DETAIL_REMOTE_OBJECT_H

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/memory_block.h"
#include "sen/core/base/move_only_function.h"
#include "sen/core/base/result.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_traits.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/core/obj/detail/event_buffer.h"
#include "sen/core/obj/detail/proxy_object.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/object_provider.h"

// std
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sen
{

// Forward declarations
namespace kernel::impl
{

class RemoteParticipant;
class ProxyManager;

}  // namespace kernel::impl

namespace impl
{

/// Ticket for identifying a call
using CallId = uint32_t;

/// Helper for sending a remote call over a transport
using SendCallFunc =  // NOLINTNEXTLINE(misc-include-cleaner) false positive about ObjectId
  std::function<Result<CallId, std::string>(TransportMode, ObjectOwnerId, ObjectId, MemberHash, MemBlockPtr&& args)>;

/// Result of a call to a remote method
enum class RemoteCallResult : uint8_t
{
  success,
  objectNotFound,
  runtimeError,
  logicError,
  unknownException,
};

/// Contents of a method response
struct MethodResponseData
{
  CallId callId;
  std::shared_ptr<std::vector<uint8_t>> returnValBuffer;
  RemoteCallResult result;
  std::string error;
};

// Forward declarations
class RemoteObject;

/// Basic information required for creating a remote object
struct RemoteObjectInfo
{
  ConstTypeHandle<ClassType> type;
  std::string name;
  ObjectId id;
  WorkQueue* workQueue = nullptr;
  SendCallFunc sendCallFunc;
  std::string localName;
  std::function<void(RemoteObject*)> destructionCallback = nullptr;
  ObjectOwnerId ownerId;
  std::weak_ptr<kernel::impl::RemoteParticipant> remoteParticipant;
  MaybeConstTypeHandle<ClassType> writerSchema = std::nullopt;
};

/// Proxy for an object that resides in another process
class SEN_EXPORT RemoteObject: public ProxyObject
{
  SEN_NOCOPY_NOMOVE(RemoteObject)

public:  // special members
  explicit RemoteObject(RemoteObjectInfo&& info);
  ~RemoteObject() override;

public:
  virtual void copyStateFrom(const RemoteObject& other);

public:  // implements Object
  [[nodiscard]] ObjectId getId() const noexcept final;
  [[nodiscard]] const std::string& getName() const noexcept final;
  [[nodiscard]] ConstTypeHandle<ClassType> getClass() const noexcept override;
  [[nodiscard]] const ProxyObject* asProxyObject() const noexcept final;
  [[nodiscard]] ProxyObject* asProxyObject() noexcept final;
  [[nodiscard]] RemoteObject* asRemoteObject() noexcept final;
  [[nodiscard]] const RemoteObject* asRemoteObject() const noexcept final;
  [[nodiscard]] const std::string& getLocalName() const noexcept final;
  [[nodiscard]] TimeStamp getLastCommitTime() const noexcept final;
  [[nodiscard]] Var getPropertyUntyped(const Property* prop) const final;
  [[nodiscard]] ConnectionGuard onEventUntyped(const Event* ev, EventCallback<VarList>&& callback) final;
  // NOLINTNEXTLINE
  void invokeUntyped(const Method* method, const VarList& args, MethodCallback<Var>&& onDone = {}) final;
  [[nodiscard]] ConnectionGuard onPropertyChangedUntyped(const Property* prop, EventCallback<VarList>&& callback) final;

public:  // implements ProxyObject
  [[nodiscard]] bool isRemote() const noexcept final;

public:
  void initializeState(Span<const uint8_t> state, const TimeStamp& time);
  std::weak_ptr<const kernel::impl::RemoteParticipant> getParticipant() const;
  [[nodiscard]] MaybeConstTypeHandle<ClassType> getWriterSchema() const noexcept;

protected:
  friend class kernel::impl::RemoteParticipant;
  friend class kernel::impl::ProxyManager;

  [[nodiscard]] bool responseReceived(MethodResponseData& response);
  void propertyUpdatesReceived(const Span<const uint8_t>& properties, TimeStamp commitTime);
  virtual void eventReceived(MemberHash eventId, TimeStamp creationTime, const Span<const uint8_t>& args) = 0;
  [[nodiscard]] virtual bool readPropertyFromStream(MemberHash id, InputStream& in, bool initialState) = 0;
  [[nodiscard]] virtual Var senImplGetPropertyImpl(MemberHash propertyId) const = 0;
  virtual void serializeMethodArgumentsFromVariants(MemberHash methodId,
                                                    const VarList& args,
                                                    OutputStream& out) const = 0;
  [[nodiscard]] virtual Var deserializeMethodReturnValueAsVariant(MemberHash methodId, InputStream& in) const = 0;
  void clearDestructionCallback() noexcept;
  void senImplEventEmitted(MemberHash id, std::function<VarList()>&& argsGetter, const EventInfo& info) final;
  void senImplRemoveUntypedConnection(ConnId id, MemberHash memberHash) final;  // NOLINT

  template <typename R, typename... Args>
  void makeRemoteCall(MemberHash id, TransportMode transportMode, MethodCallback<R>&& callback, Args... args) const;

  template <typename R, typename... Args>
  void adaptAndMakeRemoteCall(MemberHash id,
                              const Method* readerMethod,
                              TransportMode transportMode,
                              MethodCallback<R>&& callback,
                              Args... args) const;

  Result<CallId, std::string> sendCall(TransportMode mode,
                                       ObjectOwnerId ownerId,
                                       ObjectId receiverId,
                                       MemberHash id,
                                       MemBlockPtr&& args) const;

  ConnId senImplMakeConnectionId() noexcept;

  template <typename T>
  void readOrAdapt(InputStream& in, T& val, MemberHash id, const Type& readerType, bool isSync);

  [[nodiscard]] bool memberTypesInSync(MemberHash id) const noexcept;

  [[nodiscard]] bool checkMemberTypesInSyncInDetail(MemberHash id) const noexcept;

  template <typename... T>
  void dispatchEventFromStream(const EventBuffer<T...>& eventBuffer,
                               bool inSync,
                               const Span<const Arg>& readerArgs,
                               MemberHash eventId,
                               TimeStamp creationTime,
                               ObjectId producerId,
                               TransportMode transportMode,
                               const Span<const uint8_t>& buffer,
                               Object* producer) const;

  void invalidateTransport();

private:
  template <typename R>
  [[nodiscard]] static inline MethodResult<R> readMethodCallReturnValue(const std::vector<uint8_t>& buffer);

  template <typename R, typename F>
  [[nodiscard]] static inline MethodResult<R> makeMethodResult(const MethodResponseData& response, F&& func);

  template <size_t i = 0, typename... T>
  void adaptEventArgs(std::tuple<T...>& args,
                      VarList& argsAsVariants,
                      const Span<const Arg>& readerArgs,
                      const Event* writerEvent) const;

  static void logEventArgsWarning(const std::string& eventName,
                                  size_t numOfWriterEventArgs,
                                  size_t numOfReaderEventArgs);

  virtual void cancelPendingCalls();

private:
  using MutexLock = std::scoped_lock<std::recursive_mutex>;
  struct EventCallbackData
  {
    uint32_t id;
    EventCallback<VarList> callback;
  };

  using EventCallbackList = std::vector<EventCallbackData>;

  struct EventCallbackInfo
  {
    EventCallbackList list;
    TransportMode transportMode;
  };

  using EventCallbackMap = std::unordered_map<MemberHash, EventCallbackInfo>;

private:
  using PendingResponseContainerType =
    std::unordered_map<CallId, sen::std_util::move_only_function<void(MethodResponseData&&)>>;

  mutable RemoteObjectInfo info_;
  mutable PendingResponseContainerType pendingResponses_;
  mutable std::recursive_mutex pendingResponsesMutex_;
  std::atomic_uint32_t nextConnection_ = 1;
  std::atomic<TimeStamp> lastCommitTime_;
  std::unique_ptr<EventCallbackMap> eventCallbacks_;
  std::recursive_mutex eventCallbacksMutex_;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

template <typename R, typename... Args>
void RemoteObject::makeRemoteCall(MemberHash id,
                                  TransportMode transportMode,
                                  MethodCallback<R>&& callback,
                                  Args... args) const
{
  // create a buffer containing the arguments
  auto argsBuffer = std::make_shared<ResizableHeapBlock>();

  if constexpr (sizeof...(Args) != 0U)
  {
    // resize buffer
    argsBuffer->resize((... + SerializationTraits<Args>::serializedSize(args)));

    BufferWriter writer(*argsBuffer);
    OutputStream out(writer);

    // serialize args
    (SerializationTraits<Args>::write(out, args), ...);
  }

  // send the call and get a ticket
  if (auto callId = sendCall(transportMode, info_.ownerId, info_.id, id, std::move(argsBuffer)); callId.isOk())
  {
    // if no callback, dismiss the response
    if (auto cbLock = callback.lock(); cbLock.isValid())
    {
      // when receiving the response, add the callback to the work queue
      auto responseHandler =
        [cb = std::move(callback), transportMode, us = weak_from_this()](const auto& response) mutable
      {
        if (auto callbackLock = cb.lock(); callbackLock.isValid())
        {
          if (us.expired())
          {
            auto work = [cb = std::move(cb)]() mutable
            { cb.invoke({}, ::sen::Err(std::make_exception_ptr(std::runtime_error("object deleted")))); };
            callbackLock.pushAnswer(std::move(work), impl::cannotBeDropped(transportMode));
          }
          else
          {
            auto result =
              makeMethodResult<R>(response, [](const auto& buffer) { return readMethodCallReturnValue<R>(buffer); });

            auto work = [res = std::move(result), cb = std::move(cb)]() mutable { cb.invoke({}, res); };
            callbackLock.pushAnswer(std::move(work), impl::cannotBeDropped(transportMode));
          }
        }
      };

      MutexLock pendingResponsesLock(pendingResponsesMutex_);
      pendingResponses_.insert({callId.getValue(), std::move(responseHandler)});
    }
  }
  else
  {
    // if no callback, dismiss the response
    if (auto cbLock = callback.lock(); cbLock.isValid())
    {
      auto work = [cb = std::move(callback), err = callId.getError()]() mutable
      { cb.invoke({}, ::sen::Err(std::make_exception_ptr(std::runtime_error(err)))); };
      cbLock.pushAnswer(std::move(work), impl::cannotBeDropped(transportMode));
    }
  }
}

template <typename R, typename... Args>
inline void RemoteObject::adaptAndMakeRemoteCall(MemberHash id,
                                                 const Method* readerMethod,
                                                 TransportMode transportMode,
                                                 MethodCallback<R>&& callback,
                                                 Args... args) const
{
  SEN_ASSERT(info_.writerSchema.has_value() && "WriterSchema is expected");
  const auto* writerMethod = info_.writerSchema.value()->searchMethodById(id);
  if (writerMethod == nullptr)
  {
    if (auto callbackLock = callback.lock(); callbackLock.isValid())
    {
      std::string err = "Cannot call remote method ";
      err.append(readerMethod->getName());
      err.append(" because it is not defined in the remote object.");

      auto result = Err(std::make_exception_ptr(std::runtime_error(err)));

      auto work = [res = std::move(err), callback = std::move(callback)]() mutable
      { callback.invoke({}, Err(std::make_exception_ptr(std::runtime_error(res)))); };

      callbackLock.pushAnswer(std::move(work), cannotBeDropped(transportMode));
    }
    return;
  }

  const auto& readerArgs = readerMethod->getArgs();
  const auto& writerArgs = writerMethod->getArgs();
  if (writerArgs.size() > readerArgs.size())
  {
    if (auto callbackLock = callback.lock(); callbackLock.isValid())
    {
      std::string err = "Cannot call remote method ";
      err.append(readerMethod->getName());
      err.append(". Remote method requires ");
      err.append(std::to_string(writerArgs.size()));
      err.append(" arguments. Local method specifies ");
      err.append(std::to_string(readerArgs.size()));
      err.append(" arguments.");

      auto result = Err(std::make_exception_ptr(std::runtime_error(err)));
      auto work = [res = std::move(err), callback = std::move(callback)]() mutable
      { callback.invoke({}, Err(std::make_exception_ptr(std::runtime_error(res)))); };

      callbackLock.pushAnswer(std::move(work), cannotBeDropped(transportMode));
    }

    return;
  }

  // create a buffer containing the arguments
  auto argsBuffer = std::make_shared<ResizableHeapBlock>();

  if (!readerArgs.empty())
  {
    ResizableBufferWriter writer(*argsBuffer);
    OutputStream out(writer);
    VarList argsAsVariants {toVariant(args)...};

    for (const auto& arg: writerArgs)
    {
      const auto readerArgIndex = readerMethod->getArgIndexFromNameHash(arg.getNameHash());
      auto& varArg = argsAsVariants.at(readerArgIndex);
      writeToStream(varArg, out, *arg.type, readerMethod->getArgs()[readerArgIndex].type);
    }
  }

  // send the call and get a ticket
  auto callId = sendCall(transportMode, info_.ownerId, info_.id, id, std::move(argsBuffer));

  // if no callback, dismiss the response
  if (callback.lock().isValid())
  {
    // when receiving the response, add the callback to the work queue
    auto responseHandler = [cb = std::move(callback),
                            writerReturnType = writerMethod->getReturnType(),
                            us = shared_from_this(),  // NOLINT
                            transportMode](const auto& response) mutable
    {
      if (auto cbLock = cb.lock(); cbLock.isValid())
      {
        auto result = makeMethodResult<R>(response,
                                          [writerReturnType](const auto& buffer)
                                          {
                                            if constexpr (std::is_void_v<R>)
                                            {
                                              std::ignore = buffer;
                                              return ::sen::Ok();
                                            }
                                            else
                                            {
                                              InputStream in(buffer);
                                              Var var {};
                                              readFromStream(var, in, *writerReturnType);
                                              std::ignore =
                                                adaptVariant(*MetaTypeTrait<R>::meta(), var, writerReturnType);
                                              return Ok(toValue<R>(var));
                                            }
                                          });

        auto work = [res = std::move(result), cb = std::move(cb)]() mutable { cb.invoke({}, res); };
        cbLock.pushAnswer(std::move(work), cannotBeDropped(transportMode));
      }
    };

    MutexLock pendingResponsesLock(pendingResponsesMutex_);
    pendingResponses_.try_emplace(callId.getValue(), std::move(responseHandler));
  }
}

template <typename T>
inline void RemoteObject::readOrAdapt(InputStream& in, T& val, MemberHash id, const Type& readerType, bool isSync)
{
  if (SEN_LIKELY(isSync))
  {
    SerializationTraits<T>::read(in, val);
    return;
  }

  SEN_ASSERT(info_.writerSchema.has_value() && "WriterSchema is expected");
  const auto* writerProp = info_.writerSchema.value()->searchPropertyById(id);
  if (writerProp == nullptr)
  {
    std::string err = "Error while adapting the runtime type of a property from a remote object. The property ID ";
    err.append(std::to_string(id.get()));
    err.append(" cannot be found among the properties of the writer schema.");
    throwRuntimeError(err);
  }

  Var var {};
  readFromStream(var, in, *writerProp->getType());
  std::ignore = adaptVariant(readerType, var, writerProp->getType());
  VariantTraits<T>::variantToValue(var, val);
}

inline bool RemoteObject::memberTypesInSync(MemberHash id) const noexcept
{
  return SEN_LIKELY(!info_.writerSchema) || checkMemberTypesInSyncInDetail(id);
}

template <typename... T>
inline void RemoteObject::dispatchEventFromStream(const EventBuffer<T...>& eventBuffer,
                                                  bool inSync,
                                                  const Span<const Arg>& readerArgs,
                                                  MemberHash eventId,
                                                  TimeStamp creationTime,
                                                  ObjectId producerId,
                                                  TransportMode transportMode,
                                                  const Span<const uint8_t>& buffer,
                                                  Object* producer) const
{
  if (SEN_LIKELY(inSync))
  {
    eventBuffer.dispatchFromStream(eventId, creationTime, producerId, transportMode, buffer, producer);
    return;
  }

  SEN_ASSERT(info_.writerSchema.has_value() && "WriterSchema is expected");
  const auto* writerEvent = info_.writerSchema.value()->searchEventById(eventId);
  const auto& writerArgs = writerEvent->getArgs();

  InputStream in(buffer);
  VarList argsAsVariants {};
  const auto& writerArgsSize = writerArgs.size();
  const auto& readerArgsSize = readerArgs.size();
  argsAsVariants.reserve(writerArgsSize);

  // if the remote implementation has fewer arguments than the local implementation, the event cannot be dispatched
  if (writerArgsSize < readerArgsSize)
  {
    logEventArgsWarning(writerEvent->getName().data(), writerArgsSize, readerArgsSize);
    return;
  }

  for (const auto& arg: writerArgs)
  {
    argsAsVariants.emplace_back();
    readFromStream(argsAsVariants.back(), in, *arg.type);
  }

  std::tuple<T...> argsNative {};
  adaptEventArgs(argsNative, argsAsVariants, readerArgs, writerEvent);

  // call emit with the tuple arguments
  std::apply([&eventBuffer](auto&&... arg)
             { eventBuffer.dispatch(std::forward<std::remove_reference_t<decltype(arg)>>(arg)...); },
             std::tuple_cat(std::make_tuple(eventId, creationTime, producerId, transportMode, false, nullptr, producer),
                            std::move(argsNative)));
}

template <typename R>
MethodResult<R> RemoteObject::readMethodCallReturnValue(const std::vector<uint8_t>& buffer)
{
  if constexpr (std::is_void_v<R>)
  {
    return ::sen::Ok();
  }
  else
  {
    InputStream in(buffer);

    R returnVal {};
    SerializationTraits<R>::read(in, returnVal);
    return ::sen::Ok(returnVal);
  }
}

template <typename R, typename F>
MethodResult<R> RemoteObject::makeMethodResult(const MethodResponseData& response, F&& func)
{
  switch (response.result)
  {
    case RemoteCallResult::logicError:
      return ::sen::Err(std::make_exception_ptr(std::logic_error(response.error)));

    case RemoteCallResult::objectNotFound:
      return ::sen::Err(std::make_exception_ptr(std::invalid_argument(response.error)));

    case RemoteCallResult::runtimeError:
      return ::sen::Err(std::make_exception_ptr(std::runtime_error(response.error)));

    case RemoteCallResult::unknownException:
      return ::sen::Err(std::make_exception_ptr(std::exception {}));

    case RemoteCallResult::success:
      return func(*response.returnValBuffer);

    default:
      SEN_UNREACHABLE();
  }

  SEN_ASSERT(false);
}

template <size_t i, typename... T>
inline void RemoteObject::adaptEventArgs(std::tuple<T...>& argsNative,
                                         VarList& argsAsVariants,
                                         const Span<const Arg>& readerArgs,
                                         const Event* writerEvent) const
{
  if constexpr (i == sizeof...(T))
  {
    return;
  }
  else
  {
    const auto writerArgIndex = writerEvent->getArgIndexFromNameHash(readerArgs[i].getNameHash());
    auto& varArg = argsAsVariants.at(writerArgIndex);
    std::ignore = adaptVariant(*readerArgs[i].type, varArg, writerEvent->getArgs()[writerArgIndex].type);
    VariantTraits<std::remove_reference_t<decltype(std::get<i>(argsNative))>>::variantToValue(varArg,
                                                                                              std::get<i>(argsNative));

    // next element of the tuple
    adaptEventArgs<i + 1>(argsNative, argsAsVariants, readerArgs, writerEvent);
  }
}

inline void RemoteObject::invalidateTransport()
{
  info_.sendCallFunc = nullptr;
  cancelPendingCalls();
}

}  // namespace impl
}  // namespace sen

#endif  // SEN_CORE_OBJ_DETAIL_REMOTE_OBJECT_H
