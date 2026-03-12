// === creation_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "db_test_helpers.h"
#include "sen/db/creation.h"
#include "sen/db/input.h"
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

namespace sen::db::test
{

/// @test
/// Write a creation entry and read it back
/// @requirements(SEN-364)
TEST(CreationTest, WriteAndReadCreationWithRealObject)
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
    output.keyframe(setup.kernel->getTime(), {info});
  }

  Input input(archivePath.string(), setup.kernel->getTypes());

  auto cursor = input.begin();
  bool foundCreation = false;

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

      EXPECT_EQ(snapshot.getName(), "testObj");
      EXPECT_EQ(snapshot.getSessionName(), "test_session");
      EXPECT_EQ(snapshot.getBusName(), "test_bus");
      EXPECT_EQ(snapshot.getObjectId(), setup.object->getId());
      EXPECT_NE(snapshot.getType().type(), nullptr);
      foundCreation = true;
      break;
    }
  }

  EXPECT_TRUE(foundCreation) << "A Creation entry should be found in the recording";
}

/// @test
/// Indexed creation appears in object index definitions
/// @requirements(SEN-364)
TEST(CreationTest, IndexedCreationAppearsInObjectIndex)
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
    output.keyframe(setup.kernel->getTime(), {info});
  }

  Input input(archivePath.string(), setup.kernel->getTypes());
  auto indexedObjects = input.getObjectIndexDefinitions();

  ASSERT_EQ(indexedObjects.size(), 1U);
  EXPECT_EQ(indexedObjects[0].session, "test_session");
  EXPECT_EQ(indexedObjects[0].bus, "test_bus");
}

/// @test
/// Verify that Non-indexed creation does not appear in the object index definitions
/// @requirements(SEN-364)
TEST(CreationTest, NonIndexedCreationNotInObjectIndex)
{
  TempDir tempDir;

  auto object = std::make_shared<TestObjImpl>("nonIndexedObj", sen::VarMap {});

  sen::kernel::TestComponent component;
  component.onInit(
    [&](sen::kernel::InitApi&& api) -> sen::kernel::PassResult
    {
      auto source = api.getSource("local.test");
      source->add(object);
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

    ObjectInfo info = {object.get(), "test_session", "test_bus"};
    output.creation(kernel.getTime(), info, false);
    kernel.step();
    output.keyframe(kernel.getTime(), {});
  }

  Input input(archivePath.string(), kernel.getTypes());
  auto indexedObjects = input.getObjectIndexDefinitions();

  EXPECT_EQ(indexedObjects.size(), 0U);
}

}  // namespace sen::db::test
