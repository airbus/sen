// === event_test.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "db_test_helpers.h"
#include "sen/core/base/numbers.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/native_types.h"
#include "sen/db/event.h"
#include "sen/db/input.h"
#include "sen/db/output.h"
#include "stl/sen/db/basic_types.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <utility>
#include <variant>

namespace sen::db::test
{

/// @test
/// Write an event entry and read it back
/// @requirements(SEN-364)
TEST(EventTest, WriteAndReadEvent)
{
  TempDir tempDir;
  SingleClassSetup setup;

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;

  const auto classType = setup.object->getClass();
  const auto* eventMeta = classType.type()->searchEventByName("valueChanged");
  ASSERT_NE(eventMeta, nullptr) << "TestObj should have a 'valueChanged' event";

  {
    const auto eventId = eventMeta->getId();
    Output output(std::move(settings), []() {});

    ObjectInfo info = {setup.object.get(), "test_session", "test_bus"};
    output.creation(setup.kernel->getTime(), info, true);

    setup.kernel->step();

    ::sen::kernel::Buffer evBuf;
    {
      sen::ResizableBufferWriter writer(evBuf);
      sen::OutputStream out(writer);
      sen::SerializationTraits<float64_t>::write(out, 99.1);
    }
    output.event(setup.kernel->getTime(), setup.object->getId(), eventId, std::move(evBuf));

    setup.kernel->step();
    output.keyframe(setup.kernel->getTime(), {});
  }

  Input input(archivePath.string(), setup.kernel->getTypes());

  auto cursor = input.begin();
  bool foundEvent = false;

  while (!cursor.atEnd())
  {
    ++cursor;
    if (cursor.atEnd())
    {
      break;
    }

    const auto& entry = cursor.get();
    if (std::holds_alternative<db::Event>(entry.payload))
    {
      const auto& event = std::get<db::Event>(entry.payload);

      EXPECT_EQ(event.getObjectId(), setup.object->getId());
      EXPECT_NE(event.getEvent(), nullptr);
      EXPECT_EQ(event.getEvent()->getName(), "valueChanged");

      // verify args buffer
      auto buffer = event.getArgsAsBuffer();
      EXPECT_FALSE(buffer.empty()) << "Event args buffer should not be empty";

      // verify args as variants
      auto variants = event.getArgsAsVariants();
      ASSERT_EQ(variants.size(), 1U);
      EXPECT_DOUBLE_EQ(variants[0].getCopyAs<float64_t>(), 99.1);

      foundEvent = true;
      break;
    }
  }

  EXPECT_TRUE(foundEvent) << "An Event entry should be found in the recording";
}

/// @test
/// Write multiple events and verify the count
/// @requirements(SEN-364)
TEST(EventTest, MultipleEventsAreReadable)
{
  TempDir tempDir;
  SingleClassSetup setup;

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;

  const auto classType = setup.object->getClass();
  const auto* eventMeta = classType.type()->searchEventByName("valueChanged");
  ASSERT_NE(eventMeta, nullptr);
  const auto eventId = eventMeta->getId();

  constexpr int eventCount = 3;

  {
    Output output(std::move(settings), []() {});

    ObjectInfo info = {setup.object.get(), "test_session", "test_bus"};
    output.creation(setup.kernel->getTime(), info, true);

    for (int i = 0; i < eventCount; ++i)
    {
      setup.kernel->step();
      ::sen::kernel::Buffer evBuf;
      {
        sen::ResizableBufferWriter writer(evBuf);
        sen::OutputStream out(writer);
        sen::SerializationTraits<float64_t>::write(out, static_cast<double>(i));
      }
      output.event(setup.kernel->getTime(), setup.object->getId(), eventId, std::move(evBuf));
    }

    setup.kernel->step();
    output.keyframe(setup.kernel->getTime(), {});
  }

  Input input(archivePath.string(), setup.kernel->getTypes());

  auto cursor = input.begin();
  int foundEvents = 0;

  while (!cursor.atEnd())
  {
    ++cursor;
    if (cursor.atEnd())
    {
      break;
    }

    const auto& entry = cursor.get();
    if (std::holds_alternative<db::Event>(entry.payload))
    {
      ++foundEvents;
    }
  }

  EXPECT_EQ(foundEvents, eventCount);
}

}  // namespace sen::db::test
