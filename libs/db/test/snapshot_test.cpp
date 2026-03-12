// === snapshot_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "db_test_helpers.h"
#include "sen/core/obj/object.h"
#include "sen/db/creation.h"
#include "sen/db/input.h"
#include "sen/db/keyframe.h"
#include "sen/db/output.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/test_kernel.h"
#include "stl/sen/db/basic_types.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

namespace sen::db::test
{

/// @test
/// Verify Snapshot metadata fields from a Creation entry
/// @requirements(SEN-364)
TEST(SnapshotTest, SnapshotMetadataFromCreation)
{
  TempDir tempDir;
  SingleClassSetup setup;

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;

  {
    Output output(std::move(settings), []() {});

    ObjectInfo info = {setup.object.get(), "test_session", "test_bus"};
    output.creation(setup.kernel->getTime(), info, true);
    setup.kernel->step();
    output.keyframe(setup.kernel->getTime(), {});
  }

  Input input(archivePath.string(), setup.kernel->getTypes());

  auto cursor = input.begin();
  bool foundSnapshot = false;

  while (!cursor.atEnd())
  {
    ++cursor;
    if (cursor.atEnd())
    {
      break;
    }

    const auto& entry = cursor.get();
    if (std::holds_alternative<Creation>(entry.payload))
    {
      const auto& creation = std::get<Creation>(entry.payload);
      const auto& snapshot = creation.getSnapshot();

      EXPECT_EQ(snapshot.getObjectId(), setup.object->getId());
      EXPECT_EQ(snapshot.getName(), "testObj");
      EXPECT_EQ(snapshot.getSessionName(), "test_session");
      EXPECT_EQ(snapshot.getBusName(), "test_bus");
      EXPECT_NE(snapshot.getType().type(), nullptr);

      foundSnapshot = true;
      break;
    }
  }

  EXPECT_TRUE(foundSnapshot) << "A Creation with a Snapshot should be found";
}

/// @test
/// Verify Snapshot getAllPropertiesBuffer() returns non-empty data.
/// @requirements(SEN-364)
TEST(SnapshotTest, SnapshotPropertiesBuffer)
{
  TempDir tempDir;
  SingleClassSetup setup;

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;

  {
    Output output(std::move(settings), []() {});

    ObjectInfo info = {setup.object.get(), "test_session", "test_bus"};
    output.creation(setup.kernel->getTime(), info, true);
    setup.kernel->step();
    output.keyframe(setup.kernel->getTime(), {});
  }

  Input input(archivePath.string(), setup.kernel->getTypes());

  auto cursor = input.begin();

  while (!cursor.atEnd())
  {
    ++cursor;
    if (cursor.atEnd())
    {
      break;
    }

    const auto& entry = cursor.get();
    if (std::holds_alternative<Creation>(entry.payload))
    {
      const auto& creation = std::get<Creation>(entry.payload);
      const auto& snapshot = creation.getSnapshot();

      auto propBuf = snapshot.getAllPropertiesBuffer();
      EXPECT_FALSE(propBuf.empty()) << "Properties buffer should not be empty for an object with properties";
      break;
    }
  }
}

/// @test
/// Verify Snapshot getPropertyValue returns a valid Var for a known property.
/// @requirements(SEN-364)
TEST(SnapshotTest, SnapshotGetPropertyValue)
{
  TempDir tempDir;
  SingleClassSetup setup;

  // Set a known speed value before recording
  setup.object->setNextSpeed(99.1);

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;

  {
    Output output(std::move(settings), []() {});

    ObjectInfo info = {setup.object.get(), "test_session", "test_bus"};
    output.creation(setup.kernel->getTime(), info, true);
    setup.kernel->step();
    output.keyframe(setup.kernel->getTime(), {});
  }

  Input input(archivePath.string(), setup.kernel->getTypes());

  auto cursor = input.begin();

  while (!cursor.atEnd())
  {
    ++cursor;
    if (cursor.atEnd())
    {
      break;
    }

    const auto& entry = cursor.get();
    if (std::holds_alternative<Creation>(entry.payload))
    {
      const auto& creation = std::get<Creation>(entry.payload);
      const auto& snapshot = creation.getSnapshot();

      // get the property through the ClassType metadata
      const auto& properties = snapshot.getType().type()->getProperties(ClassType::SearchMode::includeParents);
      ASSERT_FALSE(properties.empty());

      const auto* speedProp = properties[0].get();

      const auto& value = snapshot.getPropertyAsVariant(speedProp);
      EXPECT_FALSE(value.isEmpty()) << "Property value should not be empty";
      break;
    }
  }
}

/// @test
/// Verify Snapshots from a Keyframe contain the correct object state.
/// @requirements(SEN-364)
TEST(SnapshotTest, SnapshotFromKeyframe)
{
  TempDir tempDir;
  SingleClassSetup setup;

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;

  {
    Output output(std::move(settings), []() {});

    ObjectInfo info = {setup.object.get(), "test_session", "test_bus"};
    output.creation(setup.kernel->getTime(), info, true);

    setup.kernel->step();

    // Write a keyframe with the object in the list
    output.keyframe(setup.kernel->getTime(), {info});
  }

  Input input(archivePath.string(), setup.kernel->getTypes());

  auto cursor = input.begin();
  bool foundKeyframeWithSnapshots = false;

  while (!cursor.atEnd())
  {
    ++cursor;
    if (cursor.atEnd())
    {
      break;
    }

    const auto& entry = cursor.get();
    if (std::holds_alternative<Keyframe>(entry.payload))
    {
      const auto& keyframe = std::get<Keyframe>(entry.payload);
      if (!keyframe.getSnapshots().empty())
      {
        const auto& snapshot = keyframe.getSnapshots()[0];
        EXPECT_EQ(snapshot.getName(), "testObj");
        EXPECT_EQ(snapshot.getSessionName(), "test_session");
        EXPECT_EQ(snapshot.getBusName(), "test_bus");
        foundKeyframeWithSnapshots = true;
        break;
      }
    }
  }

  EXPECT_TRUE(foundKeyframeWithSnapshots) << "A Keyframe with non-empty snapshots should be found in the recording";
}

/// @test
/// Verify that multiple snapshots from different creations have distinct objectIds.
/// @requirements(SEN-364)
TEST(SnapshotTest, MultipleSnapshotsHaveDistinctObjectIds)
{
  TempDir tempDir;

  auto object1 = std::make_shared<TestObjImpl>("test_obj1", sen::VarMap {});
  auto object2 = std::make_shared<TestObjImpl>("test_obj2", sen::VarMap {});

  sen::kernel::TestComponent component;
  component.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      auto source = api.getSource("local.test");
      source->add(object1);
      source->add(object2);
      return sen::kernel::done();
    });
  component.onRun([](auto& api) { return api.execLoop(std::chrono::seconds(1), []() {}); });

  sen::kernel::TestKernel kernel(&component);
  kernel.step();

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;

  {
    Output output(std::move(settings), []() {});

    ObjectInfo info1 = {object1.get(), "test_session1", "test_bus1"};
    output.creation(kernel.getTime(), info1, true);

    ObjectInfo info2 = {object2.get(), "test_session2", "test_bus2"};
    output.creation(kernel.getTime(), info2, true);

    kernel.step();
    output.keyframe(kernel.getTime(), {});
  }

  Input input(archivePath.string(), kernel.getTypes());

  auto cursor = input.begin();
  std::vector<ObjectId> objectIds;

  while (!cursor.atEnd())
  {
    ++cursor;
    if (cursor.atEnd())
    {
      break;
    }

    const auto& entry = cursor.get();
    if (std::holds_alternative<Creation>(entry.payload))
    {
      objectIds.push_back(std::get<Creation>(entry.payload).getSnapshot().getObjectId());
    }
  }

  ASSERT_EQ(objectIds.size(), 2U);
  EXPECT_NE(objectIds[0], objectIds[1]) << "Different creations should have distinct object IDs";
}

}  // namespace sen::db::test
