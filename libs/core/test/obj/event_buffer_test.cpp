// === event_buffer_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "../support/reader_writer.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/detail/event_buffer.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/object.h"

// generated code
#include "stl/example_class.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string>
#include <tuple>
#include <utility>

namespace example_class
{

class ExampleClassImpl2 final: public ExampleClassBase
{
public:
  SEN_NOCOPY_NOMOVE(ExampleClassImpl2)

public:
  using ExampleClassBase::ExampleClassBase;
  ~ExampleClassImpl2() override = default;

public:
  void myFunc(int value) { std::ignore = value; }
};

SEN_EXPORT_CLASS(ExampleClassImpl2)

}  // namespace example_class

namespace sen::impl
{

namespace
{

uint64_t counter = 0;
std::string name = "Aircraft1";  // NOLINT(cert-err58-cpp)

SerializableEvent event {{}, TimeStamp {203}, nullptr, 11, TransportMode::confirmed, 30};

void checkEventQueue(const size_t size, const bool dropOldest)
{
  EXPECT_NO_THROW(SerializableEventQueue(size, dropOldest));
}

void setCounter(const uint64_t value) { counter = value; }

void setCustomQualifiedName(const std::string& prefix) { name.insert(0, prefix + "."); }

template <typename T>
void copyIntoBufferAsBytes(test::BufferedTestReader& reader, const T data)
{
  if constexpr (std::is_same_v<T, std::string>)
  {
    test::TestWriter writer;
    OutputStream out(writer);
    out.writeString(data);

    reader.getBuffer() = writer.getBuffer();
  }
  else
  {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    auto dataPtr = reinterpret_cast<const uint8_t*>(&data);  // NOSONAR
    for (std::size_t i = 0U; i < sizeof(T); ++i)
    {
      reader.getBuffer().push_back(*(std::next(dataPtr, static_cast<int>(i))));
    }
  }
}

/// @test
/// Check creation of serializable event queue
/// @requirements(SEN-574)
TEST(SerializableEventQueue, make)
{
  // basic
  {
    checkEventQueue(50U, false);
    checkEventQueue(100U, true);
  }

  // numeric limits
  {
    checkEventQueue(std::numeric_limits<std::size_t>::max(), false);
    checkEventQueue(std::numeric_limits<std::size_t>::max(), true);

    checkEventQueue(std::numeric_limits<std::size_t>::min(), false);
    checkEventQueue(std::numeric_limits<std::size_t>::min(), true);
  }

  // zero
  {
    checkEventQueue(0U, false);
    checkEventQueue(0U, true);
  }

  // negative values
  {
    checkEventQueue(-20, false);
    checkEventQueue(-935, true);
  }

  // floating-point values
  {
    checkEventQueue(3.14f, false);  // NOLINT(bugprone-narrowing-conversions)
    checkEventQueue(3.14, true);    // NOLINT(bugprone-narrowing-conversions)
  }
}

/// @test
/// Check clear method of serializable event queue
/// @requirements(SEN-574)
TEST(SerializableEventQueue, clear)
{
  // basic
  {
    SerializableEventQueue eventQueue(10U, false);

    eventQueue.clear();
    EXPECT_EQ(eventQueue.getContents().size(), 0U);

    eventQueue.push(std::move(event));
    eventQueue.clear();
    EXPECT_EQ(eventQueue.getContents().size(), 0U);
  }

  // empty
  {
    SerializableEventQueue eventQueue(0U, true);

    eventQueue.clear();
    EXPECT_EQ(eventQueue.getContents().size(), 0U);

    eventQueue.push(std::move(event));
    eventQueue.clear();
    EXPECT_EQ(eventQueue.getContents().size(), 0U);
  }
}

/// @test
/// Check push method of serializable event queue
/// @requirements(SEN-574)
TEST(SerializableEventQueue, push)
{
  // basic
  {
    constexpr auto maxSize = 100U;

    SerializableEventQueue eventQueue(maxSize, false);

    for (auto i = 0; i < maxSize + 1; i++)
    {
      if (i % 2 == 0)
      {
        eventQueue.push({});
      }
      else
      {
        eventQueue.push(std::move(event));
      }
    }
    EXPECT_EQ(eventQueue.getContents().size(), maxSize);
  }

  // negative size
  {
    constexpr auto maxSize = -30U;

    SerializableEventQueue eventQueue(maxSize, false);

    for (auto i = 0; i > maxSize - 1; i--)
    {
      eventQueue.push(std::move(event));
    }
    EXPECT_EQ(eventQueue.getContents().size(), 0U);
  }
}

/// @test
/// Check zero size serializable event queue (no size limit)
/// @requirements(SEN-574)
TEST(SerializableEventQueue, zeroSize)
{
  constexpr auto eventsNum = 2000U;

  // do not drop oldest
  {
    SerializableEventQueue eventQueue(0U, false);

    for (auto i = 0; i < eventsNum; i++)
    {
      eventQueue.push(std::move(event));
    }

    EXPECT_EQ(eventQueue.getContents().size(), eventsNum);
  }

  // drop oldest
  {
    SerializableEventQueue eventQueue(0U, true);

    for (auto i = 0; i < eventsNum; i++)
    {
      eventQueue.push(std::move(event));
    }

    EXPECT_EQ(eventQueue.getContents().size(), eventsNum);
  }
}

/// @test
/// Check no drop oldest serializable event queue type
/// @requirements(SEN-574)
TEST(SerializableEventQueue, noDropOldest)
{
  constexpr size_t maxSizeQueue = 50U;

  // create queue
  SerializableEventQueue eventQueue(maxSizeQueue, false);
  EXPECT_EQ(eventQueue.getContents().size(), 0U);

  // enqueue first serializable event
  auto firstEvent = event;
  firstEvent.producerId = 5;
  eventQueue.push(std::move(firstEvent));

  EXPECT_EQ(eventQueue.getContents().size(), 1U);

  for (auto i = 0; i < maxSizeQueue - 1; i++)
  {
    eventQueue.push(std::move(event));
  }
  EXPECT_EQ(eventQueue.getContents().size(), maxSizeQueue);

  auto lastEvent = event;
  lastEvent.producerId = 27;

  eventQueue.push(std::move(lastEvent));
  EXPECT_EQ(eventQueue.getContents().size(), maxSizeQueue);

  // ensure we didn't drop the first queued event and also that last event was not queued
  EXPECT_EQ(eventQueue.getContents().front().producerId, ObjectId {5});
  EXPECT_NE(eventQueue.getContents().back().producerId, ObjectId {27});
}

/// @test
/// Check drop the oldest serializable event queue type
/// @requirements(SEN-574)
TEST(SerializableEventQueue, dropOldest)
{
  constexpr size_t maxSizeQueue = 30U;

  // create queue
  SerializableEventQueue eventQueue(maxSizeQueue, true);
  EXPECT_EQ(eventQueue.getContents().size(), 0U);

  // enqueue first serializable event
  auto firstEvent = event;
  firstEvent.producerId = 5;
  eventQueue.push(std::move(firstEvent));

  EXPECT_EQ(eventQueue.getContents().size(), 1U);

  for (auto i = 0; i < maxSizeQueue - 1; i++)
  {
    eventQueue.push(std::move(event));
  }
  EXPECT_EQ(eventQueue.getContents().size(), maxSizeQueue);

  auto lastEvent = event;
  lastEvent.producerId = 27;

  eventQueue.push(std::move(lastEvent));
  EXPECT_EQ(eventQueue.getContents().size(), maxSizeQueue);

  // ensure we dropped the first queued event and enqueued last event
  EXPECT_EQ(eventQueue.getContents().front().producerId, ObjectId {11});
  EXPECT_EQ(eventQueue.getContents().back().producerId, ObjectId {27});
}

/// @test
/// Check add and removal connections in event buffer
/// @requirements(SEN-574)
TEST(EventBuffer, addRemoveConnection)
{
  auto validQueue = WorkQueue(50, false);
  sen::InstanceStorageType obj;
  example_class::makeExampleClassImpl2("obj1", {}, obj);

  // add connection from an event callback of a plain function
  {
    constexpr auto connId = ConnId {1U};

    EventBuffer<uint32_t> buffer;

    buffer.addConnection(obj.get(), EventCallback<uint32_t>(&validQueue, setCounter), connId).keep();
    EXPECT_TRUE(buffer.removeConnection(connId));
  }

  // add connection from an event callback of a function with metadata
  {
    constexpr auto connId = ConnId {2U};

    EventBuffer<std::string> buffer;
    buffer.addConnection(obj.get(), EventCallback<std::string>(&validQueue, setCustomQualifiedName), connId).keep();
    EXPECT_TRUE(buffer.removeConnection(connId));
  }
}

/// @test
/// Check produce method of event buffer
/// @requirements(SEN-574)
TEST(EventBuffer, produce)
{
  constexpr size_t maxSizeQueue = 30U;
  SerializableEventQueue eventQueue(maxSizeQueue, true);

  auto workQueue = WorkQueue(50, false);
  sen::InstanceStorageType obj;
  example_class::makeExampleClassImpl2("obj1", {}, obj);

  // basic (emit now, unicast)
  {
    counter = 0;
    constexpr uint64_t value = 32U;
    constexpr auto connId = ConnId {1U};

    EventBuffer<uint32_t> buffer;
    buffer.addConnection(obj.get(), EventCallback<uint32_t>(&workQueue, setCounter), connId).keep();

    buffer.produce(Emit::now,
                   event.eventId,
                   TimeStamp {0},
                   event.producerId,
                   TransportMode::unicast,
                   true,
                   &eventQueue,
                   &workQueue,
                   obj.get(),
                   value);

    EXPECT_EQ(counter, value);
  }

  // basic 2 (emit on-commit, confirmed)
  {
    const std::string prefix = "rpr";
    constexpr auto connId = ConnId {2U};

    EventBuffer<std::string> buffer;
    buffer.addConnection(obj.get(), EventCallback<std::string>(&workQueue, setCustomQualifiedName), connId).keep();

    buffer.produce(Emit::onCommit,
                   event.eventId,
                   TimeStamp {0},
                   event.producerId,
                   TransportMode::confirmed,
                   true,
                   &eventQueue,
                   &workQueue,
                   obj.get(),
                   prefix);

    EXPECT_EQ(name, "Aircraft1");

    if (workQueue.executeAll())
    {
      EXPECT_EQ(name, "rpr.Aircraft1");
    }
  }
}

/// @test
/// Check dispatch method of event buffer
/// @requirements(SEN-574)
TEST(EventBuffer, dispatch)
{
  constexpr size_t maxSizeQueue = 30U;
  SerializableEventQueue eventQueue(maxSizeQueue, true);

  auto workQueue = WorkQueue(50, false);
  sen::InstanceStorageType obj;
  example_class::makeExampleClassImpl2("obj1", {}, obj);

  // confirmed
  {
    counter = 0;
    constexpr uint64_t value = 11U;
    constexpr auto connId = ConnId {1U};

    EventBuffer<uint32_t> buffer;
    buffer.addConnection(obj.get(), EventCallback<uint32_t>(&workQueue, setCounter), connId).keep();

    buffer.dispatch(
      event.eventId, TimeStamp {0}, event.producerId, TransportMode::confirmed, true, &eventQueue, obj.get(), value);

    EXPECT_EQ(counter, 0U);
    if (workQueue.executeAll())
    {
      EXPECT_EQ(counter, value);
    }
  }

  // multicast
  {
    name = "Platform1";
    const std::string prefix = "rpr";
    constexpr auto connId = ConnId {2U};

    EventBuffer<std::string> buffer;
    buffer.addConnection(obj.get(), EventCallback<std::string>(&workQueue, setCustomQualifiedName), connId).keep();

    buffer.dispatch(
      event.eventId, TimeStamp {0}, event.producerId, TransportMode::multicast, false, &eventQueue, obj.get(), prefix);

    EXPECT_EQ(name, "Platform1");
    if (workQueue.executeAll())
    {
      EXPECT_EQ(name, "rpr.Platform1");
    }
  }
}

/// @test
/// Check immediate dispatch of event buffer
/// @requirements(SEN-574)
TEST(EventBuffer, dispatchImmediate)
{
  constexpr size_t maxSizeQueue = 30U;
  SerializableEventQueue eventQueue(maxSizeQueue, true);

  auto workQueue = WorkQueue(50, false);
  sen::InstanceStorageType obj;
  example_class::makeExampleClassImpl2("obj1", {}, obj);

  // confirmed
  {
    counter = 0;
    constexpr uint64_t value = 11U;
    constexpr auto connId = ConnId {1U};

    EventBuffer<uint32_t> buffer;
    buffer.addConnection(obj.get(), EventCallback<uint32_t>(&workQueue, setCounter), connId).keep();

    buffer.immediateDispatch(event.eventId, TimeStamp {0}, TransportMode::confirmed, obj.get(), &workQueue, value);

    EXPECT_EQ(counter, value);
  }

  // multicast
  {
    name = "Aircraft1";
    const std::string prefix = "rpr";
    constexpr auto connId = ConnId {2U};

    EventBuffer<std::string> buffer;
    buffer.addConnection(obj.get(), EventCallback<std::string>(&workQueue, setCustomQualifiedName), connId).keep();

    buffer.immediateDispatch(event.eventId, TimeStamp {0}, TransportMode::multicast, obj.get(), &workQueue, prefix);

    EXPECT_EQ(name, "rpr.Aircraft1");
  }

  // unicast
  {
    counter = 0;
    constexpr uint64_t value = 44U;
    constexpr auto connId = ConnId {3U};

    EventBuffer<uint32_t> buffer;
    buffer.addConnection(obj.get(), EventCallback<uint32_t>(&workQueue, setCounter), connId).keep();

    buffer.immediateDispatch(event.eventId, TimeStamp {0}, TransportMode::unicast, obj.get(), &workQueue, value);

    EXPECT_EQ(counter, value);
  }
}

/// @test
/// Check dispatch from stream of event buffer
/// @requirements(SEN-574)
TEST(EventBuffer, dispatchFromStream)
{
  constexpr size_t maxSizeQueue = 30U;
  SerializableEventQueue eventQueue(maxSizeQueue, true);

  auto workQueue = WorkQueue(50, false);
  sen::InstanceStorageType obj;
  example_class::makeExampleClassImpl2("obj1", {}, obj);

  // multicast, native type
  {
    counter = 0;
    constexpr uint64_t value = 11U;
    constexpr auto connId = ConnId {1U};

    test::BufferedTestReader reader;
    copyIntoBufferAsBytes(reader, value);

    EventBuffer<uint32_t> buffer;
    buffer.addConnection(obj.get(), EventCallback<uint32_t>(&workQueue, setCounter), connId).keep();

    buffer.dispatchFromStream(
      event.eventId, TimeStamp {0}, event.producerId, TransportMode::multicast, reader.getBuffer(), obj.get());

    EXPECT_EQ(counter, 0U);
    if (workQueue.executeAll())
    {
      EXPECT_EQ(counter, value);
    }
  }

  // confirmed, string type
  {
    name = "Aircraft1";
    const std::string prefix = "rpr";
    constexpr auto connId = ConnId {2U};

    test::BufferedTestReader reader;
    copyIntoBufferAsBytes(reader, name);

    EventBuffer<std::string> buffer;
    buffer.addConnection(obj.get(), EventCallback<std::string>(&workQueue, setCustomQualifiedName), connId).keep();

    buffer.dispatchFromStream(
      event.eventId, TimeStamp {0}, event.producerId, TransportMode::confirmed, reader.getBuffer(), obj.get());

    EXPECT_EQ(name, "Aircraft1");
    if (workQueue.executeAll())
    {
      EXPECT_EQ(name, "rpr.Aircraft1");
    }
  }
}

}  // namespace

}  // namespace sen::impl
