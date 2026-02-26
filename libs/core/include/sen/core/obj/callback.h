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

/// Information about an event emission.
struct EventInfo
{
  TimeStamp creationTime;  ///< When was the event created, timestamped by the source.
};

/// Information about a method call.
struct MethodCallInfo
{
};

namespace impl
{

class CallbackBase;

/// Holds internal data related to callbacks
struct CallbackData
{
  std::recursive_mutex mutex;
  WorkQueue* queue = nullptr;
  std::optional<std::weak_ptr<Object>> caller;
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

  /// If valid, it pushes work to the caller's queue.
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

/// Base class for event or method callbacks.
/// It stores the queue where to push the response.
/// The queue can be obtained directly as an argument, or through an object.
class CallbackBase: public std::enable_shared_from_this<CallbackBase>
{
public:
  SEN_COPY_MOVE(CallbackBase)

public:
  virtual ~CallbackBase() = default;

public:
  /// Get thread-safe access to this callback object.
  [[nodiscard]] CallbackLock lock() const { return CallbackLock(data_); }

protected:
  /// A cancelled callback.
  CallbackBase() = default;

  /// A callback with an explicit queue and a callable
  explicit CallbackBase(WorkQueue* queue) { data_->queue = queue; }

  /// A callback where the queue is extracted from an object and a callable.
  /// If the object gets deleted the callback won't be executed.
  explicit CallbackBase(NativeObject* obj);

  [[nodiscard]] CallbackData& getData() const noexcept { return *data_; }

private:
  std::shared_ptr<CallbackData> data_ = std::make_shared<CallbackData>();
};

}  // namespace impl

/// Base class for event or method callbacks.
/// It stores the queue where to push the response.
/// The queue can be obtained directly as an argument, or through an object.
template <typename InfoClass, typename... Args>
class Callback final: public impl::CallbackBase
{
public:
  SEN_COPY_MOVE(Callback)

public:
  using Function = sen::std_util::move_only_function<void(MaybeRef<Args>...)>;
  using FunctionWithCallInfo = sen::std_util::move_only_function<void(const InfoClass&, MaybeRef<Args>...)>;
  using CallbackFunc = std::variant<Function, FunctionWithCallInfo>;

public:
  Callback() = default;
  ~Callback() override = default;

  /// A callback with an explicit queue and a callable
  Callback(impl::WorkQueue* queue, CallbackFunc&& function) noexcept: CallbackBase(queue), func_(std::move(function)) {}

  /// A callback where the queue is extracted from an object and a callable.
  /// If the object gets deleted the callback won't be executed.
  Callback(NativeObject* obj, CallbackFunc&& function) noexcept: CallbackBase(obj), func_(std::move(function)) {}

  /// A callback where the queue is extracted from an object and a function pointer to a member.
  /// If the object gets deleted the callback won't be executed.
  template <typename C>
  Callback(C* obj, void (C::*function)(MaybeRef<Args>...))
    : CallbackBase(obj, [obj, function](MaybeRef<Args>... args) { ((*obj).*(function))(args...); })
  {
  }

  /// A callback where the queue is extracted from an object and a function pointer to member that
  /// also receives meta information.
  template <typename C>
  Callback(C* obj, void (C::*function)(const InfoClass&, MaybeRef<Args>...))
    : CallbackBase(obj)
    , func_([obj, function](const InfoClass& info, MaybeRef<Args>... args) { ((*obj).*(function))(info, args...); })
  {
  }

public:
  /// Invokes the function passed in the constructor.
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

/// The result of a method (which can be an exception in case of error).
template <typename R>
using MethodResult = Result<R, std::exception_ptr>;

/// An event callback.
template <typename... Args>
using EventCallback = Callback<EventInfo, Args...>;

/// A method callback.
template <typename R>
using MethodCallback = Callback<MethodCallInfo, MethodResult<R>>;

/// A property change callback.
using PropertyCallback = EventCallback<>;

/// @}

}  // namespace sen

#endif  // SEN_CORE_OBJ_CALLBACK_H
