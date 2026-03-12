// === property_change_test.cpp ========================================================================================
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
#include "sen/db/input.h"
#include "sen/db/output.h"
#include "sen/db/property_change.h"
#include "stl/sen/db/basic_types.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <memory>
#include <utility>
#include <variant>

namespace sen::db::test
{

/// @test
/// Write a property change and read it back.
/// @requirements(SEN-364)
TEST(PropertyChangeTest, WriteAndReadPropertyChange)
{
  TempDir tempDir;
  SingleClassSetup setup;

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;

  // retrieve the property metadata by name for the correct ID
  const auto classType = setup.object->getClass();
  const auto* propMeta = classType.type()->searchPropertyByName("speed");
  ASSERT_NE(propMeta, nullptr) << "TestObj should have a 'speed' property";
  const auto propId = propMeta->getId();

  {
    Output output(std::move(settings), []() {});

    ObjectInfo info = {setup.object.get(), "test_session", "test_bus"};
    output.creation(setup.kernel->getTime(), info, true);

    setup.kernel->step();

    // build property value buffer with proper serialization
    ::sen::kernel::Buffer propBuf;
    {
      sen::ResizableBufferWriter writer(propBuf);
      sen::OutputStream out(writer);
      sen::SerializationTraits<float64_t>::write(out, 250.0);
    }
    output.propertyChange(setup.kernel->getTime(), setup.object->getId(), propId, std::move(propBuf));

    setup.kernel->step();
    output.keyframe(setup.kernel->getTime(), {});
  }

  Input input(archivePath.string(), setup.kernel->getTypes());

  auto cursor = input.begin();
  bool foundPropertyChange = false;

  while (!cursor.atEnd())
  {
    ++cursor;
    if (cursor.atEnd())
    {
      break;
    }

    const auto& entry = cursor.get();
    if (std::holds_alternative<PropertyChange>(entry.payload))
    {
      const auto& propChange = std::get<PropertyChange>(entry.payload);

      EXPECT_EQ(propChange.getObjectId(), setup.object->getId());
      EXPECT_NE(propChange.getProperty(), nullptr);
      EXPECT_EQ(propChange.getProperty()->getName(), "speed");

      auto buffer = propChange.getValueAsBuffer();
      EXPECT_FALSE(buffer.empty()) << "Property value buffer should not be empty";

      foundPropertyChange = true;
      break;
    }
  }

  EXPECT_TRUE(foundPropertyChange) << "A PropertyChange entry should be found in the recording";
}

/// @test
/// Write multiple property changes and verify all are readable
/// @requirements(SEN-364)
TEST(PropertyChangeTest, MultiplePropertyChanges)
{
  TempDir tempDir;
  SingleClassSetup setup;

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;

  const auto classType = setup.object->getClass();
  const auto* propMeta = classType.type()->searchPropertyByName("speed");
  ASSERT_NE(propMeta, nullptr);
  const auto propId = propMeta->getId();

  constexpr int changeCount = 4;

  {
    Output output(std::move(settings), []() {});

    ObjectInfo info = {setup.object.get(), "test_session", "test_bus"};
    output.creation(setup.kernel->getTime(), info, true);

    for (int i = 0; i < changeCount; ++i)
    {
      setup.kernel->step();
      ::sen::kernel::Buffer propBuf;
      {
        sen::ResizableBufferWriter writer(propBuf);
        sen::OutputStream out(writer);
        sen::SerializationTraits<float64_t>::write(out, static_cast<double>(i * 100));
      }
      output.propertyChange(setup.kernel->getTime(), setup.object->getId(), propId, std::move(propBuf));
    }

    setup.kernel->step();
    output.keyframe(setup.kernel->getTime(), {});
  }

  Input input(archivePath.string(), setup.kernel->getTypes());

  auto cursor = input.begin();
  int foundChanges = 0;

  while (!cursor.atEnd())
  {
    ++cursor;
    if (cursor.atEnd())
    {
      break;
    }

    const auto& entry = cursor.get();
    if (std::holds_alternative<PropertyChange>(entry.payload))
    {
      ++foundChanges;
    }
  }

  EXPECT_EQ(foundChanges, changeCount);
}

/// @test
/// Read a property change and verify getValueAsVariant works.
/// @requirements(SEN-364)
TEST(PropertyChangeTest, GetValueAsVariant)
{
  TempDir tempDir;
  SingleClassSetup setup;

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;

  const auto classType = setup.object->getClass();
  const auto* propMeta = classType.type()->searchPropertyByName("speed");
  ASSERT_NE(propMeta, nullptr);
  const auto propId = propMeta->getId();

  {
    Output output(std::move(settings), []() {});

    ObjectInfo info = {setup.object.get(), "test_session", "test_bus"};
    output.creation(setup.kernel->getTime(), info, true);

    setup.kernel->step();

    ::sen::kernel::Buffer propBuf;
    {
      sen::ResizableBufferWriter writer(propBuf);
      sen::OutputStream out(writer);
      sen::SerializationTraits<float64_t>::write(out, 123.456);
    }
    output.propertyChange(setup.kernel->getTime(), setup.object->getId(), propId, std::move(propBuf));

    setup.kernel->step();
    output.keyframe(setup.kernel->getTime(), {});
  }

  Input input(archivePath.string(), setup.kernel->getTypes());

  auto cursor = input.begin();
  bool foundPropertyChange = false;

  while (!cursor.atEnd())
  {
    ++cursor;
    if (cursor.atEnd())
    {
      break;
    }

    const auto& entry = cursor.get();
    if (std::holds_alternative<PropertyChange>(entry.payload))
    {
      const auto& propChange = std::get<PropertyChange>(entry.payload);

      const auto& variant = propChange.getValueAsVariant();
      EXPECT_DOUBLE_EQ(variant.getCopyAs<float64_t>(), 123.456);

      foundPropertyChange = true;
      break;
    }
  }

  EXPECT_TRUE(foundPropertyChange) << "A PropertyChange entry should be found";
}

}  // namespace sen::db::test
