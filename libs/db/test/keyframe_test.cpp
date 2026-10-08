// === keyframe_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "db_test_helpers.h"
#include "sen/db/input.h"
#include "sen/db/keyframe.h"
#include "sen/db/output.h"
#include "sen/kernel/test_kernel.h"
#include "stl/sen/db/basic_types.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstddef>
#include <cstdint>
#include <utility>
#include <variant>

namespace sen::db::test
{

/// @test
/// Produces a Keyframe entry whose snapshot list is empty when the keyframe was recorded with no objects.
/// @requirements(SEN-364)
TEST(KeyframeTest, EmptyKeyframeHasNoSnapshots)
{
  TempDir tempDir;

  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;

  {
    Output output(std::move(settings), []() {});
    output.keyframe(kernel.getTime(), {});
  }

  Input input(archivePath.string(), kernel.getTypes());

  auto cursor = input.begin();
  bool foundKeyframe = false;

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
      EXPECT_EQ(keyframe.getSnapshots().size(), 0U);
      foundKeyframe = true;
      break;
    }
  }

  EXPECT_TRUE(foundKeyframe) << "A Keyframe entry should be found in the recording";
}

/// @test
/// Indexes all five keyframes recorded at successive times, the summary and
/// getAllKeyframeIndexes both counting five.
/// @requirements(SEN-364)
TEST(KeyframeTest, MultipleKeyframeIndexes)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;
  constexpr int keyframeCount = 5;

  {
    Output output(std::move(settings), []() {});

    for (int i = 0; i < keyframeCount; ++i)
    {
      output.keyframe(kernel.getTime(), {});
      kernel.step();
    }
  }

  Input input(archivePath.string(), kernel.getTypes());

  // verify summary count
  const auto& summary = input.getSummary();
  EXPECT_EQ(summary.keyframeCount, static_cast<uint32_t>(keyframeCount));

  // verify all keyframe indexes
  auto keyframes = input.getAllKeyframeIndexes();
  EXPECT_EQ(keyframes.size(), static_cast<std::size_t>(keyframeCount));
}

/// @test
/// Returns keyframe indexes in non-decreasing time order after five keyframes were recorded at
/// successive times.
/// @requirements(SEN-364)
TEST(KeyframeTest, KeyframeIndexesSortedByTime)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;
  constexpr int keyframeCount = 5U;

  {
    Output output(std::move(settings), []() {});

    for (int i = 0; i < keyframeCount; ++i)
    {
      output.keyframe(kernel.getTime(), {});
      kernel.step();
    }
  }

  Input input(archivePath.string(), kernel.getTypes());
  auto keyframes = input.getAllKeyframeIndexes();
  ASSERT_EQ(keyframes.size(), keyframeCount);

  // verify that each keyframe should be at same or later time than the previous
  for (std::size_t i = 1; i < keyframes.size(); ++i)
  {
    EXPECT_LE(keyframes[i - 1].time, keyframes[i].time) << "Keyframe indexes should be sorted by time";
  }
}

/// @test
/// Positions a cursor at the second keyframe index, iteration from there yielding the four
/// remaining entries of the five recorded.
/// @requirements(SEN-364)
TEST(KeyframeTest, NavigateToSpecificKeyframe)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;
  constexpr int keyframeCount = 5U;

  {
    Output output(std::move(settings), []() {});

    for (int i = 0; i < keyframeCount; ++i)
    {
      output.keyframe(kernel.getTime(), {});
      kernel.step();
    }
  }

  Input input(archivePath.string(), kernel.getTypes());
  auto keyframes = input.getAllKeyframeIndexes();
  ASSERT_EQ(keyframes.size(), keyframeCount);

  // navigate to the second keyframe
  auto cursor = input.at(keyframes[1]);
  EXPECT_FALSE(cursor.atEnd());

  // count remaining entries
  int entries = 0;
  while (!cursor.atEnd())
  {
    ++cursor;
    if (!cursor.atEnd())
    {
      ++entries;
    }
  }

  EXPECT_EQ(entries, keyframeCount - 1) << "There should be 1 entry less after navigating to keyframe[1]";
}

/// @test
/// Returns the first keyframe index from getKeyframeIndex when queried with that keyframe's exact time.
/// @requirements(SEN-364)
TEST(KeyframeTest, GetClosestKeyframeIndex)
{
  TempDir tempDir;
  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;
  constexpr int keyframeCount = 5U;

  {
    Output output(std::move(settings), []() {});

    for (int i = 0; i < keyframeCount; ++i)
    {
      output.keyframe(kernel.getTime(), {});
      kernel.step();
    }
  }

  Input input(archivePath.string(), kernel.getTypes());
  auto keyframes = input.getAllKeyframeIndexes();
  ASSERT_EQ(keyframes.size(), keyframeCount);

  // querying the exact time of the first keyframe should return it
  auto result = input.getKeyframeIndex(keyframes[0].time);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->time, keyframes[0].time);
}

}  // namespace sen::db::test
