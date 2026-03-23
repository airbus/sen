// === native_object.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_NATIVE_OBJECT_H
#define SEN_CORE_OBJ_NATIVE_OBJECT_H

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/move_only_function.h"
#include "sen/core/base/result.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/core/obj/detail/event_buffer.h"
#include "sen/core/obj/detail/native_object_impl.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/object.h"

// std
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sen
{

/// \addtogroup obj
/// @{

using StreamCall = sen::std_util::move_only_function<void(OutputStream&)>;
using VariantCall = sen::std_util::move_only_function<void(Var&)>;
using StreamCallForwarder = sen::std_util::move_only_function<void(StreamCall&&)>;
using VariantCallForwarder = sen::std_util::move_only_function<void(VariantCall&&)>;

/// Base class for all user-defined objects that are instantiated within the local process.
///
/// Subclasses are typically generated from STL class declarations and compiled into component
/// shared libraries. Users override `registered()`, `update()`, and `unregistered()` to implement
/// object behaviour, and emit events or mutate properties through the generated typed API.
///
/// Objects publish their state at `commit()` time and receive remote changes during `drainInputs()`.
/// Thread safety within a single component cycle is guaranteed by the kernel runner.
class SEN_EXPORT NativeObject: public Object
{
  SEN_NOCOPY_NOMOVE(NativeObject)

public:
  ~NativeObject() override;

public:
  /// Called by the kernel after this object has been successfully published on its bus.
  /// Override to perform one-time initialisation that requires kernel context
  /// (e.g. subscribing to other objects, registering callbacks).
  /// @param api Kernel API available during the registration phase.
  virtual void registered(kernel::RegistrationApi& api);

  /// Called on every cycle by the component's `RunApi::update()`.
  /// Override to implement the object's per-cycle logic â€” read inputs, compute state,
  /// write outputs. The default implementation does nothing.
  /// @param runApi Kernel API available during the run phase.
  virtual void update(kernel::RunApi& runApi);

  /// Called by the kernel just before this object is removed from its bus.
  /// Override to release subscriptions or perform cleanup that requires kernel context.
  /// @param api Kernel API available during the un-registration phase.
  virtual void unregistered(kernel::RegistrationApi& api);

  /// Returns `true` if `preDrain()` and `preCommit()` should be called for this object.
  /// Override and return `true` if the object needs a hook before each drain or commit.
  /// Defaults to `false` to avoid per-object overhead in the common case.
  [[nodiscard]] virtual bool needsPreDrainOrPreCommit() const noexcept { return false; }

  /// Called just before `RunApi::drainInputs()` if `needsPreDrainOrPreCommit()` returns `true`.
  /// Override to snapshot any shared state before external updates arrive.
  virtual void preDrain() {}

  /// Called just before `RunApi::commit()` if `needsPreDrainOrPreCommit()` returns `true`.
  /// Override to prepare any final state before changes are published.
  virtual void preCommit() {}

  /// Sets a property's staged (next-commit) value using an untyped `Var`.
  /// Prefer the generated typed `setNextXxx()` methods; this is the runtime fallback.
  /// @param property Descriptor of the property to stage.
  /// @param value    New value; must be convertible to the property's declared type.
  void setNextPropertyUntyped(const Property* property, const Var& value);

  /// Returns a property's staged (next-commit) value as a `Var`.
  /// Prefer the generated typed `getNextXxx()` methods; this is the runtime fallback.
  /// @param property Descriptor of the property to read.
  /// @return The staged value that will be published at the next `commit()`.
  [[nodiscard]] Var getNextPropertyUntyped(const Property* property) const;

public:  // implements Object
  [[nodiscard]] ObjectId getId() const noexcept final;
  [[nodiscard]] const std::string& getName() const noexcept final;
  [[nodiscard]] Var getPropertyUntyped(const Property* prop) const final;
  [[nodiscard]] NativeObject* asNativeObject() noexcept final;
  [[nodiscard]] const NativeObject* asNativeObject() const noexcept final;
  [[nodiscard]] const std::string& getLocalName() const noexcept final;
  [[nodiscard]] TimeStamp getLastCommitTime() const noexcept override;
  [[nodiscard]] ConnectionGuard onEventUntyped(const Event* ev, EventCallback<VarList>&& callback) final;
  void invokeUntyped(const Method* method, const VarList& args, MethodCallback<Var>&& onDone) final;
  [[nodiscard]] ConnectionGuard onPropertyChangedUntyped(const Property* prop, EventCallback<VarList>&& callback) final;

protected:
  explicit NativeObject(const std::string& name);
  void commit(TimeStamp time);

protected:  // provided by the generated code
  [[nodiscard]] virtual Var senImplGetPropertyImpl(MemberHash propertyId) const = 0;
  virtual void senImplSetNextPropertyUntyped(MemberHash propertyId, const Var& value) = 0;
  [[nodiscard]] virtual Var senImplGetNextPropertyUntyped(MemberHash propertyId) const = 0;
  [[nodiscard]] uint32_t senImplComputeMaxReliableSerializedPropertySize() const
  {
    auto readLock = createReaderLock();
    return senImplComputeMaxReliableSerializedPropertySizeImpl();
  }
  virtual void senImplCommitImpl(TimeStamp time) = 0;
  virtual void senImplWriteChangedPropertiesToStream(OutputStream& confirmed,
                                                     impl::BufferProvider uni,
                                                     impl::BufferProvider multi) = 0;
  virtual void senImplStreamCall(MemberHash methodId, InputStream& in, StreamCallForwarder&& fwd) = 0;
  virtual void senImplVariantCall(MemberHash methodId, const VarList& args, VariantCallForwarder&& fwd) = 0;

  [[nodiscard]] virtual impl::FieldValueGetter senImplGetFieldValueGetter(MemberHash propertyId,
                                                                          Span<uint16_t> fields) const = 0;

private:
  [[nodiscard]] virtual uint32_t senImplComputeMaxReliableSerializedPropertySizeImpl() const = 0;

protected:  // called by the generated code
  [[nodiscard]] ConnId senImplMakeConnectionId() noexcept;

  void senImplRemoveUntypedConnection(ConnId id, MemberHash memberHash) override;

  /// Queues a call to a method for future execution (non-const version).
  template <typename R, typename... Args, typename F, class C>
  inline void senImplAsyncCall(C* instance, MethodCallback<R>&& callback, F&& f, bool forcePush, Args... args);

  /// Queues a call to a method for future execution (const version).
  template <typename R, typename... Args, typename F, class C>
  inline void senImplAsyncCall(const C* instance, MethodCallback<R>&& callback, F&& f, bool forcePush, Args... args)
    const;

  /// Queues a call to a method using a variant-based approach.
  void senImplAsyncCall(const Method* method, const VarList& args, MethodCallback<Var>&& callback);

  /// Queues a call to a method for future execution with deferred semantics (non-const version).
  template <typename R, typename... Args, typename F, class C>
  inline void senImplAsyncDeferredCall(C* instance, MethodCallback<R>&& callback, F&& f, bool forcePush, Args... args);

  /// Tries to get a result from the future, queuing back the check until timed out
  template <typename R>
  static inline void tryToGetResult(std::shared_ptr<std::future<R>> future,
                                    MethodCallback<R>&& callback,
                                    bool forcePush);

  /// Helper to call eventBuffer.produce() with our data
  template <typename... T>
  inline void senImplProduceEvent(impl::EventBuffer<T...>& eventBuffer,
                                  Emit emissionMode,
                                  MemberHash eventId,
                                  TransportMode transportMode,
                                  bool addToTransportQueue,
                                  MaybeRef<T>... args);

  /// Called by EventBuffer when emitting an event
  void senImplEventEmitted(MemberHash id, std::function<VarList()>&& argsGetter, const EventInfo& info) final;

  /// Queues a function (no specific task) into the work queue.
  void addWorkToQueue(sen::std_util::move_only_function<void()>&& call, bool forcePush) const;

  [[nodiscard]] impl::SerializableEventQueue* getOutputEventQueue() noexcept;

private:
  void setLocalPrefix(std::string_view localPrefix) noexcept;

  /// Called by the execution environment, replaces the queues
  /// the object uses to postpone pending work.
  void setQueues(impl::WorkQueue* workQueue, impl::SerializableEventQueue* eventQueue) noexcept;

  /// This method gets called by the kernel runner when the object gets registered.
  /// The object will carry this registration time until it gets unregistered.
  void setRegistrationTime(TimeStamp time) noexcept { registrationTime_ = time; }

  /// The time set by the runner at the point of registration.
  [[nodiscard]] TimeStamp getRegistrationTime() const noexcept { return registrationTime_; }

private:
  template <typename... T>
  friend class impl::EventBuffer;

  struct EventCallbackData
  {
    uint32_t id;
    std::shared_ptr<EventCallback<VarList>> callback;
  };

  using EventCallbackList = std::vector<EventCallbackData>;

  struct EventCallbackInfo
  {
    EventCallbackList list;
    TransportMode transportMode;
  };

  using EventCallbackMap = std::unordered_map<MemberHash, EventCallbackInfo>;

protected:
  [[nodiscard]] std::lock_guard<std::shared_mutex> createWriterLock() const
  {
    return std::lock_guard(dataAccessMutex_);
  }

  [[nodiscard]] std::shared_lock<std::shared_mutex> createReaderLock() const
  {
    return std::shared_lock(dataAccessMutex_);
  }

private:  // access to the kernel and generated proxies
  friend class impl::NativeObjectProxy;
  friend class impl::FilteredProvider;
  friend class kernel::impl::RemoteParticipant;
  friend class kernel::impl::LocalParticipant;
  friend class kernel::impl::Runner;
  friend class kernel::impl::ObjectUpdate;
  friend class kernel::PipelineComponent;
  friend impl::WorkQueue* impl::getWorkQueue(NativeObject* object) noexcept;

private:
  mutable std::atomic<impl::WorkQueue*> workQueue_;
  mutable std::atomic<impl::SerializableEventQueue*> outputEventQueue_;
  mutable impl::WorkQueue postCommitEvents_;
  TimeStamp lastCommitTime_;
  mutable std::shared_mutex dataAccessMutex_;
  ObjectId id_;
  std::string name_;
  std::string localName_;
  std::atomic_uint32_t nextConnection_ = 1U;
  std::unique_ptr<EventCallbackMap> eventCallbacks_;
  std::recursive_mutex eventCallbacksMutex_;
  TimeStamp registrationTime_;
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

namespace impl
{
inline WorkQueue* getWorkQueue(NativeObject* object) noexcept { return object->workQueue_.load(); }

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

template <typename R, typename... Args, typename F, class C>
[[nodiscard]] inline MethodResult<R> executeCallWithArgs(C instance, F&& f, const Args&... args) noexcept
{
  try
  {
    if constexpr (std::is_void_v<R>)
    {
      (*instance.*f)(args...);
      return ::sen::Ok();
    }
    else
    {
      return ::sen::Ok((*instance.*f)(args...));
    }
  }
  catch (...)
  {
    return ::sen::Err(std::current_exception());
  }
}

template <typename R, typename... Args, typename F, class C>
[[nodiscard]] inline std::exception_ptr executeDeferredCallWithArgs(C instance,
                                                                    F&& f,
                                                                    std::promise<R> promise,
                                                                    const Args&... args) noexcept
{
  try
  {
    (*instance.*f)(args..., std::move(promise));
    return nullptr;
  }
  catch (...)
  {
    return std::current_exception();
  }
}

}  // namespace impl

template <typename R, typename... Args, typename F, class C>
void NativeObject::senImplAsyncCall(C* instance, MethodCallback<R>&& callback, F&& f, bool forcePush, Args... args)
{
  // add work to our queue. Our runner will execute it during the drain stage. If the caller provided a callback,
  // then we need to provide the result and ensure that the caller executes the callback in its own execution context.
  addWorkToQueue(
    [instance,
     forcePush,
     args...,
     func = std::forward<F>(f),
     responseCallback = std::move(callback),
     us = weak_from_this()]() mutable
    {
      // try to execute the call on us, but first check if we have been deleted in the meantime
      if (us.expired())
      {
        if (auto callbackLock = responseCallback.lock(); callbackLock.isValid())
        {
          auto err = MethodResult<R> {Err(std::make_exception_ptr(std::runtime_error("object no longer exists")))};
          auto work = [rcb = std::move(responseCallback), err = std::move(err)]() mutable { rcb.invoke({}, err); };
          callbackLock.pushAnswer(std::move(work), forcePush);
        }
      }
      else
      {
        auto result = impl::executeCallWithArgs<R, Args...>(instance, std::move(func), args...);  // NOLINT
        if (auto callbackLock = responseCallback.lock(); callbackLock.isValid())
        {
          auto work = [ret = std::move(result), rcb = std::move(responseCallback)]() mutable { rcb.invoke({}, ret); };
          callbackLock.pushAnswer(std::move(work), forcePush);
        }
      }
    },
    forcePush);
}

inline void NativeObject::invokeUntyped(const Method* method, const VarList& args, MethodCallback<Var>&& onDone)
{
  senImplAsyncCall(method, args, std::move(onDone));
}

// add work to our queue. Our runner will execute it during the drain stage. If the caller provided a callback,
// then we need to provide the result and ensure that the caller executes the callback in its own execution context.
template <typename R, typename... Args, typename F, class C>
void NativeObject::senImplAsyncCall(const C* instance,
                                    MethodCallback<R>&& callback,
                                    F&& f,
                                    bool forcePush,
                                    Args... args) const
{
  addWorkToQueue(
    [instance,
     forcePush,
     args...,
     func = std::forward<F>(f),
     responseCallback = std::move(callback),
     us = weak_from_this()]() mutable
    {
      if (us.expired())
      {
        if (auto callbackLock = responseCallback.lock(); callbackLock.isValid())
        {
          auto err = MethodResult<R> {Err(std::make_exception_ptr(std::runtime_error("object no longer exists")))};
          auto work = [rcb = std::move(responseCallback), err = std::move(err)]() mutable { rcb.invoke({}, err); };
          callbackLock.pushAnswer(std::move(work), forcePush);
        }
      }
      else
      {
        auto result = impl::executeCallWithArgs<R, Args...>(instance, std::move(func), args...);
        if (auto callbackLock = responseCallback.lock(); callbackLock.isValid())
        {
          auto work = [ret = std::move(result), rcb = std::move(responseCallback)]() mutable { rcb.invoke({}, ret); };
          callbackLock.pushAnswer(std::move(work), forcePush);
        }
      }
    },
    forcePush);
}

template <typename R, typename... Args, typename F, class C>
void NativeObject::senImplAsyncDeferredCall(C* instance,
                                            MethodCallback<R>&& callback,
                                            F&& f,
                                            bool forcePush,
                                            Args... args)
{
  addWorkToQueue(
    [instance,
     forcePush,
     args...,
     func = std::forward<F>(f),
     responseCallback = std::move(callback),
     us = weak_from_this()]() mutable
    {
      if (us.expired())
      {
        if (auto callbackLock = responseCallback.lock(); callbackLock.isValid())
        {
          auto err = MethodResult<R> {Err(std::make_exception_ptr(std::runtime_error("object no longer exists")))};
          auto work = [err = std::move(err), rcb = std::move(responseCallback)]() mutable { rcb.invoke({}, err); };
          callbackLock.pushAnswer(std::move(work), forcePush);
        }
      }
      else
      {
        std::promise<R> promise;
        auto future = std::make_shared<std::future<R>>(promise.get_future());
        auto exceptionPtr = impl::executeDeferredCallWithArgs(instance, func, std::move(promise), args...);

        if (auto callbackLock = responseCallback.lock(); callbackLock.isValid())
        {
          if (exceptionPtr)
          {
            auto err = MethodResult<R> {::sen::Err(exceptionPtr)};
            auto work = [err = std::move(err), rcb = std::move(responseCallback)]() mutable { rcb.invoke({}, err); };
            callbackLock.pushAnswer(std::move(work), forcePush);
          }
          else
          {
            callbackLock.pushAnswer([fut = std::move(future), cb = std::move(responseCallback), forcePush]() mutable
                                    { tryToGetResult<R>(std::move(fut), std::move(cb), forcePush); },
                                    forcePush);
          }
        }
      }
    },
    forcePush);
}

template <typename R>
void NativeObject::tryToGetResult(std::shared_ptr<std::future<R>> future, MethodCallback<R>&& callback, bool forcePush)
{
  if (!future->valid())
  {
    // if the future is not valid, keep trying to get the result
    if (auto callbackLock = callback.lock(); callbackLock.isValid())
    {
      callbackLock.pushAnswer([fut = std::move(future), cb = std::move(callback), forcePush]() mutable
                              { tryToGetResult<R>(std::move(fut), std::move(cb), forcePush); },
                              forcePush);
    }
  }
  else
  {
    // poll the future
    auto status = future->wait_for(std::chrono::microseconds(0));

    // check the status
    try
    {
      switch (status)
      {
        case std::future_status::deferred:
        {
          if constexpr (std::is_same_v<R, void>)
          {
            future->get();
            callback.invoke({}, Ok());
          }
          else
          {
            callback.invoke({}, Ok(future->get()));
          }
        }
        break;
        case std::future_status::ready:
        {
          if constexpr (std::is_same_v<R, void>)
          {
            future->get();
            callback.invoke({}, Ok());
          }
          else
          {
            auto value = future->get();
            callback.invoke({}, Ok(value));
          }
        }
        break;
        case std::future_status::timeout:
        {
          // keep trying
          if (auto callbackLock = callback.lock(); callbackLock.isValid())
          {
            callbackLock.pushAnswer([fut = std::move(future), cb = std::move(callback), forcePush]() mutable
                                    { tryToGetResult<R>(std::move(fut), std::move(cb), forcePush); },
                                    forcePush);
          }
        }
        break;
        default:
          break;
      }
    }
    catch (...)
    {
      callback.invoke({}, Err(std::current_exception()));
    }
  }
}

template <typename... T>
void NativeObject::senImplProduceEvent(impl::EventBuffer<T...>& eventBuffer,
                                       Emit emissionMode,
                                       MemberHash eventId,
                                       TransportMode transportMode,
                                       bool addToTransportQueue,
                                       MaybeRef<T>... args)
{
  eventBuffer.produce(emissionMode,
                      eventId,
                      lastCommitTime_,
                      id_,
                      transportMode,
                      addToTransportQueue,
                      outputEventQueue_.load(),
                      workQueue_.load(),
                      this,
                      args...);
}

}  // namespace sen

#endif  // SEN_CORE_OBJ_NATIVE_OBJECT_H
