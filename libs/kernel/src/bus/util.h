// === util.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_BUS_UTIL_H
#define SEN_LIBS_KERNEL_SRC_BUS_UTIL_H

// sen
#include "sen/core/base/span.h"
#include "sen/core/io/detail/serialization_traits.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/obj/detail/event_buffer.h"
#include "sen/kernel/transport.h"

// std
#include <algorithm>
#include <cstdint>
#include <vector>

namespace sen::kernel::impl
{

template <typename T>
[[nodiscard]] inline bool isPresent(const T& element, const std::vector<T>& list)
{
  return std::find(list.begin(), list.end(), element) != list.end();
}

template <typename T>
inline void removeIfPresent(T element, std::vector<T>& list)
{
  list.erase(std::remove(list.begin(), list.end(), element), list.end());
}

template <typename T, typename Predicate>
inline void removeIf(std::vector<T>& list, Predicate shouldRemove)
{
  list.erase(std::remove_if(list.begin(), list.end(), shouldRemove), list.end());
}

template <typename AssociativeContainer, typename Predicate>
void removeIfFromAssociative(AssociativeContainer& container, Predicate shouldRemove)
{
  for (auto it = begin(container); it != end(container);)
  {
    if (shouldRemove(*it))
    {
      it = container.erase(it);
    }
    else
    {
      ++it;
    }
  }
}

template <typename T>
inline void copyAppend(std::vector<T>& list, const std::vector<T>& toAppend)
{
  list.insert(list.end(), toAppend.begin(), toAppend.end());
}

template <typename T>
inline void moveAppend(std::vector<T>& dst, std::vector<T>& src)
{
  dst.reserve(dst.size() + src.size());
  std::move(std::begin(src), std::end(src), std::back_inserter(dst));
  src.clear();
}

[[nodiscard]] inline uint32_t getMaxEventSerializedSize(
  const ::sen::impl::SerializableEvent& serializableEvent) noexcept
{
  return ::sen::impl::getSerializedSize(serializableEvent.producerId.get()) +  // producer
         ::sen::impl::getSerializedSize(serializableEvent.eventId.get()) +     // event id
         ::sen::impl::getSerializedSize(serializableEvent.creationTime) +      // creation
         ::sen::impl::getSerializedSize(serializableEvent.serializedSize) +    // size
         serializableEvent.serializedSize;                                     // bytes
}

inline void writeEventToStream(OutputStream& out, const ::sen::impl::SerializableEvent& serializableEvent)
{
  out.writeUInt32(serializableEvent.producerId.get());
  out.writeUInt32(serializableEvent.eventId.get());
  out.writeTimestamp(serializableEvent.creationTime);
  out.writeUInt32(serializableEvent.serializedSize);

  serializableEvent.serializeFunc(out);
}

struct EventFromStream
{
  ObjectId producerId;
  MemberHash eventId;
  TimeStamp creationTime;
  Span<const uint8_t> argumentsBuffer;
};

inline void readEventFromStream(InputStream& in, EventFromStream& data)
{
  uint32_t producerId = 0;
  in.readUInt32(producerId);
  data.producerId.set(producerId);

  uint32_t eventId = 0;
  in.readUInt32(eventId);
  data.eventId.set(eventId);

  in.readTimeStamp(data.creationTime);

  uint32_t payloadSize = 0;
  in.readUInt32(payloadSize);
  data.argumentsBuffer = makeConstSpan(in.advance(payloadSize), payloadSize);
}

template <typename Pool>
[[nodiscard]] inline ResizableBufferWriter<FixedMemoryBlock> getBestEffortBuffer(Pool& pool,
                                                                                 BestEffortBlockPtr& currentBuffer,
                                                                                 BestEffortBufferList& bufferList,
                                                                                 std::size_t maxMessageSize)
{
  if (maxMessageSize > maxBestEffortMessageSize)
  {
    ::sen::throwRuntimeError("cannot fit data into a best-effort message");
  }

  // add a buffer if there is none or if there's not enough space
  if (!currentBuffer || currentBuffer->size() + maxMessageSize >= maxBestEffortMessageSize)
  {
    bufferList.emplace_back(pool.getBlockPtr());
    currentBuffer = bufferList.back();
  }

  return ResizableBufferWriter<FixedMemoryBlock>(*currentBuffer);
}

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_BUS_UTIL_H
