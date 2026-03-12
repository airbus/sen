// === deletion_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "db_test_helpers.h"
#include "sen/core/obj/object.h"
#include "sen/db/deletion.h"
#include "sen/db/input.h"
#include "sen/db/output.h"
#include "sen/kernel/test_kernel.h"
#include "stl/sen/db/basic_types.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <utility>
#include <variant>
#include <vector>

namespace sen::db::test
{

/// @test
/// Write a deletion entry and verify it is readable
/// @requirements(SEN-364)
TEST(DeletionTest, WriteAndReadDeletion)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;
  const auto archivePath = tempDir.path() / settings.name;

  // limited scope to force to close output
  {
    Output output(std::move(settings), []() {});

    // write a keyframe to make it a valid archive
    output.keyframe(kernel.getTime(), {});
    kernel.step();

    // write a deletion
    sen::ObjectId objId = 10;
    output.deletion(kernel.getTime(), objId);
  }

  Input input(archivePath.string(), kernel.getTypes());

  auto cursor = input.begin();
  bool foundDeletion = false;

  while (!cursor.atEnd())
  {
    ++cursor;
    if (cursor.atEnd())
    {
      break;
    }

    const auto& entry = cursor.get();
    if (std::holds_alternative<Deletion>(entry.payload))
    {
      const auto& deletion = std::get<Deletion>(entry.payload);
      EXPECT_EQ(deletion.getObjectId(), sen::ObjectId(10));
      foundDeletion = true;
      break;
    }
  }

  EXPECT_TRUE(foundDeletion) << "A Deletion entry should be found in the recording";
}

/// @test
/// Write multiple deletion entries and verify all are readable
/// @requirements(SEN-364)
TEST(DeletionTest, MultipleDeletionEntries)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;
  const auto archivePath = tempDir.path() / settings.name;
  constexpr int deletionCount = 5;

  {
    Output output(std::move(settings), []() {});

    output.keyframe(kernel.getTime(), {});

    for (int i = 0; i < deletionCount; ++i)
    {
      kernel.step();
      sen::ObjectId objId = 10 + i;
      output.deletion(kernel.getTime(), objId);
    }
  }

  Input input(archivePath.string(), kernel.getTypes());
  auto cursor = input.begin();
  int foundDeletions = 0;

  while (!cursor.atEnd())
  {
    ++cursor;
    if (cursor.atEnd())
    {
      break;
    }

    const auto& entry = cursor.get();
    if (std::holds_alternative<Deletion>(entry.payload))
    {
      ++foundDeletions;
    }
  }

  EXPECT_EQ(foundDeletions, deletionCount);
}

/// @test
/// Verify that deletion returns different ids for different deletions
/// @requirements(SEN-364)
TEST(DeletionTest, DifferentObjectIds)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;
  const auto archivePath = tempDir.path() / settings.name;

  // limited scope to force to close output
  {
    Output output(std::move(settings), []() {});

    output.keyframe(kernel.getTime(), {});
    kernel.step();

    output.deletion(kernel.getTime(), sen::ObjectId(10));
    kernel.step();

    output.deletion(kernel.getTime(), sen::ObjectId(20));
  }

  Input input(archivePath.string(), kernel.getTypes());
  auto cursor = input.begin();
  std::vector<sen::ObjectId> ids;

  while (!cursor.atEnd())
  {
    ++cursor;
    if (cursor.atEnd())
    {
      break;
    }

    const auto& entry = cursor.get();
    if (std::holds_alternative<Deletion>(entry.payload))
    {
      ids.push_back(std::get<Deletion>(entry.payload).getObjectId());
    }
  }

  ASSERT_EQ(ids.size(), 2U);
  EXPECT_NE(ids[0], ids[1]);
  EXPECT_EQ(ids[0], sen::ObjectId(10));
  EXPECT_EQ(ids[1], sen::ObjectId(20));
}

}  // namespace sen::db::test
