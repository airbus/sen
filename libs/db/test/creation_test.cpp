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
/// Records a Creation entry for a live object and reads it back, the snapshot preserving the
/// object name, session name, bus name, object id, and a resolvable type.
/// @requirements(SEN-364)
TEST(CreationTest, WriteAndReadCreationWithRealObject)
{
  TempDir tempDir;
  SingleClassSetup setup;

  auto settings = makeArchiveSettings("test", tempDir);
  const auto archivePath = makeArchivePath("test", tempDir);

  {
    Output output(std::move(settings), []() {});

    auto info = makeObjectInfo(setup.object);
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
/// Lists an object recorded with the indexed flag in the archive's object index definitions,
/// carrying its session and bus names.
/// @requirements(SEN-364)
TEST(CreationTest, IndexedCreationAppearsInObjectIndex)
{
  TempDir tempDir;
  SingleClassSetup setup;

  auto settings = makeArchiveSettings("test", tempDir);
  const auto archivePath = makeArchivePath("test", tempDir);

  {
    Output output(std::move(settings), []() {});

    auto info = makeObjectInfo(setup.object);
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
/// Omits an object recorded without the indexed flag from the archive's object index
/// definitions, which read back empty.
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

  auto settings = makeArchiveSettings("test", tempDir);
  const auto archivePath = makeArchivePath("test", tempDir);

  {
    Output output(std::move(settings), []() {});

    auto info = makeObjectInfo(object);
    output.creation(kernel.getTime(), info, false);
    kernel.step();
    output.keyframe(kernel.getTime(), {});
  }

  Input input(archivePath.string(), kernel.getTypes());
  auto indexedObjects = input.getObjectIndexDefinitions();

  EXPECT_EQ(indexedObjects.size(), 0U);
}

}  // namespace sen::db::test
