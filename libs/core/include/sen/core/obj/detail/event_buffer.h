// === event_buffer.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_DETAIL_EVENT_BUFFER_H
#define SEN_CORE_OBJ_DETAIL_EVENT_BUFFER_H

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/move_only_function.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/object.h"

// std
#include <cstddef>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace sen::impl
{

//--------------------------------------------------------------------------------------------------------------
// SerializableEvent
//--------------------------------------------------------------------------------------------------------------

using SerializationFunc = std::function<void(OutputStream&)>;

/// Info about an event that might be serialized
struct SerializableEvent
{
  MemberHash eventId = 0U;
  TimeStamp creationTime;
  SerializationFunc serializeFunc = nullptr;
  ObjectId producerId;
  TransportMode transportMode = TransportMode::multicast;
  uint32_t serializedSize = 0U;
};

//--------------------------------------------------------------------------------------------------------------
// SerializableEventQueue
//--------------------------------------------------------------------------------------------------------------

/// A (thread-unaware) ordered list of serializable events
class SerializableEventQueue
{
public:
  SEN_NOCOPY_NOMOVE(SerializableEventQueue)

public:
  SerializableEventQueue(std::size_t maxSize, bool dropOldest): maxSize_(maxSize), dropOldest_(dropOldest) {}

  ~SerializableEventQueue() = default;

public:
  void setOnDropped(std_util::move_only_function<void()>&& func) { onDropped_ = std::move(func); }
  void push(SerializableEvent&& event);
  void clear() { queue_.clear(); }
  [[nodiscard]] const std::list<SerializableEvent>& getContents() const noexcept { return queue_; }

private:
  std::list<SerializableEvent> queue_;
  std::size_t maxSize_;
  std::size_t overflowCount_ = 0U;
  bool dropOldest_;
  std_util::move_only_function<void()> onDropped_ = []() {};
};

//--------------------------------------------------------------------------------------------------------------
// EventBuffer
//--------------------------------------------------------------------------------------------------------------

/// Holds the connection callbacks of a given event
/// and provides a type-safe interface for the caller.
/// This class is not thread aware
template <typename... T>
class EventBuffer final
{
  SEN_MOVE_ONLY(EventBuffer)

public:
  using Callback = EventCallback<T...>;

public:  // special members
  EventBuffer() = default;
  ~EventBuffer() = default;

public:
  /// Registers a callback that shall be invoked.
  [[nodiscard]] ConnectionGuard addConnection(Object* source, Callback callback, ConnId id);

  /// Removes a previously registered callback.
  [[nodiscard]] bool removeConnection(ConnId id);

  /// Emit the event, invoking or enqueuing the dispatch.
  /// The transportQueue parameter might be null.
  void produce(Emit emissionMode,
               MemberHash eventId,
               TimeStamp creationTime,
               ObjectId producerId,
               TransportMode transportMode,
               bool addToTransportQueue,
               SerializableEventQueue* transportQueue,
               WorkQueue* queue,
               Object* producer,
               MaybeRef<T>... args) const;

  /// Reads the event arguments from a buffer and calls dispatch.
  void dispatchFromStream(MemberHash eventId,
                          TimeStamp creationTime,
                          ObjectId producerId,
                          TransportMode transportMode,
                          const Span<const uint8_t>& buffer,
                          Object* producer) const;

  /// Notifies all connected receivers (including the transport queue if needed).
  void dispatch(MemberHash eventId,
                TimeStamp creationTime,
                ObjectId producerId,
                TransportMode transportMode,
                bool addToTransportQueue,
                SerializableEventQueue* transportQueue,
                Object* producer,
                MaybeRef<T>... args) const;

  /// Same as dispatch, but for events emitted with the 'now' flag.
  void immediateDispatch(MemberHash eventId,
                         TimeStamp creationTime,
                         TransportMode transportMode,
                         Object* producer,
                         WorkQueue* queue,
                         MaybeRef<T>... args) const;

private:
  struct CallbackEntry
  {
    using CallbackStorageType = std::shared_ptr<Callback>;

    CallbackEntry(ConnId id, CallbackStorageType callback): id_(id), callback_(std::move(callback)) {}

    [[nodiscard]] ConnId getConnectionId() const noexcept { return id_; }
    const CallbackStorageType& getCallback() const noexcept { return callback_; }

  private:
    ConnId id_;
    CallbackStorageType callback_;
  };
  std::vector<CallbackEntry> eventCallbacks_;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

template <typename T>
inline void addVariantToList(const T& val, VarList& list)
{
  list.emplace_back();
  VariantTraits<T>::valueToVariant(val, list.back());
}

[[nodiscard]] inline bool cannotBeDropped(TransportMode mode) noexcept { return mode == TransportMode::confirmed; }

//--------------------------------------------------------------------------------------------------------------
// SerializableEventQueue
//--------------------------------------------------------------------------------------------------------------

inline void SerializableEventQueue::push(SerializableEvent&& event)
{
  if (maxSize_ == 0)
  {
    queue_.push_back(std::move(event));
    return;
  }

  if (queue_.size() < maxSize_)
  {
    queue_.push_back(std::move(event));
    return;
  }

  if (dropOldest_)
  {
    queue_.pop_front();
    queue_.push_back(std::move(event));
  }

  onDropped_();
}

//--------------------------------------------------------------------------------------------------------------
// EventBuffer
//--------------------------------------------------------------------------------------------------------------

template <typename... T>
inline ConnectionGuard EventBuffer<T...>::addConnection(Object* source, Callback callback, ConnId id)
{
  eventCallbacks_.emplace_back(id, std::make_shared<decltype(callback)>(std::move(callback)));
  return {source, id.get(), 0U, true};
}

template <typename... T>
inline bool EventBuffer<T...>::removeConnection(ConnId id)
{
  for (auto itr = eventCallbacks_.begin(); itr != eventCallbacks_.end(); ++itr)
  {
    if (itr->id() == id)
    {

      if (auto callbackLock = itr->callback()->lock(); callbackLock.isValid())
      {
        callbackLock.invalidate();
      }

      eventCallbacks_.erase(itr);
      return true;
    }
  }

  return false;
}

template <typename... T>
inline void EventBuffer<T...>::produce(Emit emissionMode,
                                       MemberHash eventId,
                                       TimeStamp creationTime,
                                       ObjectId producerId,
                                       TransportMode transportMode,
                                       bool addToTransportQueue,
                                       SerializableEventQueue* transportQueue,
                                       WorkQueue* queue,
                                       Object* producer,
                                       MaybeRef<T>... args) const
{
  if (emissionMode == Emit::now)
  {
    immediateDispatch(eventId, creationTime, transportMode, producer, queue, args...);
  }
  else
  {
    queue->push(
      [this, eventId, creationTime, producerId, transportMode, addToTransportQueue, transportQueue, producer, args...]()
      {
        dispatch(
          eventId, creationTime, producerId, transportMode, addToTransportQueue, transportQueue, producer, args...);
      },
      impl::cannotBeDropped(transportMode));
  }
}

template <typename... T>
inline void EventBuffer<T...>::dispatch(MemberHash eventId,
                                        TimeStamp creationTime,
                                        ObjectId producerId,
                                        TransportMode transportMode,
                                        bool addToTransportQueue,
                                        SerializableEventQueue* transportQueue,
                                        Object* producer,
                                        MaybeRef<T>... args) const
{
  EventInfo info {creationTime};

  // for (const auto& callback: callbacks_)
  for (const auto& callbackEntry: eventCallbacks_)
  {
    if (auto callbackLock = callbackEntry.callback()->lock(); callbackLock.isValid())
    {
#if SEN_GCC_VERSION_CHECK_SMALLER(12, 4, 0)
      // TODO (SEN-717): clean up with gcc12.4 on debian
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
      auto work = [cb = callbackEntry.callback(), info, args...]() { cb->invoke(info, args...); };
#  pragma GCC diagnostic pop
#else
      // Note: if modified, patch line below
      auto work = [cb = callbackEntry.callback(), info, args...]() { cb->invoke(info, args...); };
#endif
      callbackLock.pushAnswer(std::move(work), impl::cannotBeDropped(transportMode));
    }
  }

  if (addToTransportQueue)
  {
    uint32_t serializedSize = 0U;
    if constexpr (sizeof...(T) != 0U)
    {
      serializedSize = (... + SerializationTraits<T>::serializedSize(args));
    }

    transportQueue->push({eventId,
                          creationTime,
                          [args...](OutputStream& out)
                          {
                            std::ignore = out;
                            (SerializationTraits<T>::write(out, args), ...);
                          },
                          producerId,
                          transportMode,
                          serializedSize});
  }

  producer->senImplEventEmitted(
    eventId,
    [args...]()
    {
      VarList result;
      result.reserve(sizeof...(T));
      (addVariantToList<T>(args, result), ...);
      return result;
    },
    info);
}

template <typename... T>
inline void EventBuffer<T...>::immediateDispatch(MemberHash eventId,
                                                 TimeStamp creationTime,
                                                 TransportMode transportMode,
                                                 Object* producer,
                                                 WorkQueue* queue,
                                                 MaybeRef<T>... args) const
{
  EventInfo info {creationTime};

  for (auto& callbackEntry: eventCallbacks_)
  {
    if (auto callbackLock = callbackEntry.callback()->lock(); callbackLock.isValid())
    {
      if (callbackLock.isSameQueue(queue))
      {
        callbackEntry.callback()->invoke(info, args...);
      }
      else
      {
        auto work = [cb = callbackEntry.callback(), info, args...]() { cb->invoke(info, args...); };
        callbackLock.pushAnswer(std::move(work), impl::cannotBeDropped(transportMode));
      }
    }
  }

  producer->senImplEventEmitted(
    eventId,
    [args...]()
    {
      VarList result;
      result.reserve(sizeof...(T));
      (addVariantToList<T>(args, result), ...);
      return result;
    },
    info);
}

template <typename... T>
inline void EventBuffer<T...>::dispatchFromStream(MemberHash eventId,
                                                  TimeStamp creationTime,
                                                  ObjectId producerId,
                                                  TransportMode transportMode,
                                                  const Span<const uint8_t>& buffer,
                                                  Object* producer) const
{
  std::tuple<T...> argValues = {};

  InputStream in(buffer);

  // deserialize into the tuple
  std::apply([&in](auto&&... arg)
             { ((SerializationTraits<std::remove_reference_t<decltype(arg)>>::read(in, arg)), ...); },
             argValues);

  // call emit with the tuple arguments
  std::apply(
    &EventBuffer::dispatch,
    std::tuple_cat(std::make_tuple(this, eventId, creationTime, producerId, transportMode, false, nullptr, producer),
                   std::move(argValues)));
}

}  // namespace sen::impl

#endif  // SEN_CORE_OBJ_DETAIL_EVENT_BUFFER_H
