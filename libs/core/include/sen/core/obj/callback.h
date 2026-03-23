// === callback.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_CALLBACK_H
#define SEN_CORE_OBJ_CALLBACK_H

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/move_only_function.h"
#include "sen/core/base/result.h"
#include "sen/core/base/timestamp.h"

// std
#include "detail/work_queue.h"

#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <variant>

namespace sen
{
class NativeObject;
class Object;
}  // namespace sen

namespace sen::impl
{
class WorkQueue;
[[nodiscard]] inline WorkQueue* getWorkQueue(NativeObject* object) noexcept;
}  // namespace sen::impl

namespace sen
{

/// \addtogroup obj
/// @{

/// Metadata attached to every event emission, available inside `EventCallback` handlers.
struct EventInfo
{
  TimeStamp creationTime;  ///< Wall-clock time when the event was created, as recorded by the emitting component.
};

/// Metadata attached to every method call, available inside `MethodCallback` handlers.
/// Currently empty; reserved for future diagnostic fields (call latency, caller id, etc.).
struct MethodCallInfo
{
};

namespace impl
{

class CallbackBase;

/// Internal state shared between a Callback and its CallbackLock.
/// Access is always guarded by @p mutex.
struct CallbackData
{
  std::recursive_mutex mutex;  ///< Guards all reads and writes to the other fields.
  WorkQueue* queue = nullptr;  ///< The work queue to push responses onto; nullptr means cancelled.
  std::optional<std::weak_ptr<Object>>
    caller;  ///< If set, the callback is automatically cancelled when this object is destroyed.
};

/// Provides thread-safe access to the callback object.
class CallbackLock
{
  SEN_NOCOPY_NOMOVE(CallbackLock)

public:
  ~CallbackLock() { data_->mutex.unlock(); }

public:
  /// True if a callback has been set and it hasn't been cancelled.
  [[nodiscard]] bool isValid() const noexcept { return isCallbackDataValid(*data_); }

  /// Enqueues a response onto the caller's work queue if the callback is still valid.
  /// @param answer Callable to push; executed on the caller's thread.
  /// @param force  If true, push even when the caller's queue matches the current queue (same-thread fast path).
  void pushAnswer(sen::std_util::move_only_function<void()>&& answer, bool force) const;

  /// Invalidates further calls.
  void invalidate() const noexcept { data_->queue = nullptr; }

  /// Returns true if the caller's work queue matches the passed one.
  [[nodiscard]] bool isSameQueue(const WorkQueue* queue) const noexcept { return queue == data_->queue; }

  /// Helper function for checking if the callback data is valid.
  [[nodiscard]] static bool isCallbackDataValid(const CallbackData& data) noexcept
  {
    return data.queue != nullptr && (!data.caller.has_value() || !data.caller.value().expired());
  }

private:
  friend class CallbackBase;
  explicit CallbackLock(std::shared_ptr<CallbackData> data): data_(std::move(data)) { data_->mutex.lock(); }

private:
  std::shared_ptr<CallbackData> data_;
};

/// Base class for event and method callbacks.
/// Manages the work queue onto which responses are pushed and provides thread-safe
/// locking via CallbackLock. Subclasses (Callback<>) add the actual callable storage.
class CallbackBase: public std::enable_shared_from_this<CallbackBase>
{
public:
  SEN_COPY_MOVE(CallbackBase)

public:
  virtual ~CallbackBase() = default;

public:
  /// Acquires the internal mutex and returns a RAII lock.
  /// Use the returned lock to check validity and push work before releasing.
  /// @return A CallbackLock that holds the mutex until destroyed.
  [[nodiscard]] CallbackLock lock() const { return CallbackLock(data_); }

protected:
  /// Constructs a pre-cancelled (invalid) callback with no associated queue.
  CallbackBase() = default;

  /// Constructs a callback that posts responses to @p queue.
  /// @param queue The work queue owned by the calling component's thread.
  explicit CallbackBase(WorkQueue* queue) { data_->queue = queue; }

  /// Constructs a callback tied to the lifetime of @p obj.
  /// Responses are posted to the object's internal work queue.
  /// The callback is automatically cancelled when @p obj is destroyed.
  /// @param obj The owning NativeObject whose queue and lifetime govern this callback.
  explicit CallbackBase(NativeObject* obj);

  /// @return Mutable reference to the shared internal callback state.
  [[nodiscard]] CallbackData& getData() const noexcept { return *data_; }

private:
  std::shared_ptr<CallbackData> data_ = std::make_shared<CallbackData>();
};

}  // namespace impl

/// Type-safe callback for events and method responses.
/// Wraps a callable and a work queue so that the callable is always invoked on the
/// correct component thread, even when the call originates from a different thread or process.
///
/// @tparam InfoClass  Metadata type accompanying each invocation (`EventInfo` or `MethodCallInfo`).
/// @tparam Args       Types of the callback arguments (event arguments or method return value).
template <typename InfoClass, typename... Args>
class Callback final: public impl::CallbackBase
{
public:
  SEN_COPY_MOVE(Callback)

public:
  /// Callable receiving only the event/method arguments (no metadata).
  using Function = sen::std_util::move_only_function<void(MaybeRef<Args>...)>;
  /// Callable receiving metadata (@p InfoClass) followed by event/method arguments.
  using FunctionWithCallInfo = sen::std_util::move_only_function<void(const InfoClass&, MaybeRef<Args>...)>;
  /// Internal storage: either a plain Function or a FunctionWithCallInfo.
  using CallbackFunc = std::variant<Function, FunctionWithCallInfo>;

public:
  /// Constructs a pre-cancelled (empty) callback.
  Callback() = default;
  ~Callback() override = default;

  /// Constructs a callback that invokes @p function on @p queue.
  /// @param queue    The work queue onto which invocations are posted.
  /// @param function The callable to invoke; stored by move.
  Callback(impl::WorkQueue* queue, CallbackFunc&& function) noexcept: CallbackBase(queue), func_(std::move(function)) {}

  /// Constructs a callback tied to the lifetime of @p obj.
  /// @param obj      The owning NativeObject; the callback is cancelled when it is destroyed.
  /// @param function The callable to invoke; stored by move.
  Callback(NativeObject* obj, CallbackFunc&& function) noexcept: CallbackBase(obj), func_(std::move(function)) {}

  /// Constructs a callback that calls a member function on @p obj without metadata.
  /// @tparam C       Type of the owning object.
  /// @param obj      The owning object; the callback is cancelled when it is destroyed.
  /// @param function Pointer to a non-static member function of @p C.
  template <typename C>
  Callback(C* obj, void (C::*function)(MaybeRef<Args>...))
    : CallbackBase(obj, [obj, function](MaybeRef<Args>... args) { ((*obj).*(function))(args...); })
  {
  }

  /// Constructs a callback that calls a member function on @p obj, also passing call metadata.
  /// @tparam C       Type of the owning object.
  /// @param obj      The owning object; the callback is cancelled when it is destroyed.
  /// @param function Pointer to a non-static member function of @p C that receives an `InfoClass` as first argument.
  template <typename C>
  Callback(C* obj, void (C::*function)(const InfoClass&, MaybeRef<Args>...))
    : CallbackBase(obj)
    , func_([obj, function](const InfoClass& info, MaybeRef<Args>... args) { ((*obj).*(function))(info, args...); })
  {
  }

public:
  /// Invokes the stored callable on the caller's thread if the callback is still valid.
  /// No-op if the callback has been cancelled or the owning object has been destroyed.
  /// @param info  Metadata for this invocation.
  /// @param args  Arguments forwarded to the callable.
  void invoke(const InfoClass& info, MaybeRef<Args>... args) const
  {
    // directly use the data's mutex to avoid the overhead of creating a Lock object
    auto& data = getData();
    std::scoped_lock<std::recursive_mutex> lock(data.mutex);

    if (SEN_LIKELY(impl::CallbackLock::isCallbackDataValid(data)))
    {
      std::holds_alternative<Function>(func_) ? std::get<Function>(func_)(args...)
                                              : std::get<FunctionWithCallInfo>(func_)(info, args...);
    }
  }

private:
  mutable CallbackFunc func_;
};

/// Wraps a method return value or an exception pointer if the method threw.
/// @tparam R  The declared return type of the method.
template <typename R>
using MethodResult = Result<R, std::exception_ptr>;

/// Callback type for event subscriptions.
/// The callable receives zero or more event arguments.
/// @tparam Args  Types of the event arguments as declared in the STL interface.
template <typename... Args>
using EventCallback = Callback<EventInfo, Args...>;

/// Callback type for asynchronous method calls.
/// The callable receives a `MethodResult<R>` which is either the return value or an exception.
/// @tparam R  The declared return type of the method.
template <typename R>
using MethodCallback = Callback<MethodCallInfo, MethodResult<R>>;

/// Callback type for property-change notifications.
/// Equivalent to an `EventCallback` with no arguments; the new value can be read directly
/// from the object after the callback fires.
using PropertyCallback = EventCallback<>;

/// @}

}  // namespace sen

#endif  // SEN_CORE_OBJ_CALLBACK_H
