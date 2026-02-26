// === input_test.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/db/input.h"
#include "sen/db/output.h"
#include "sen/kernel/test_kernel.h"
#include "stl/sen/db/basic_types.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <ctime>
#include <filesystem>
#include <string>
#include <utility>

namespace sen::db::test
{

class TempDir
{
public:
  TempDir(): path_(std::filesystem::temp_directory_path() / ("db_test_" + std::to_string(std::time(nullptr))))
  {
    std::filesystem::create_directories(path_);
  }

  // prevent object copy and movable type
  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  TempDir(TempDir&&) = delete;
  TempDir& operator=(TempDir&&) = delete;

  ~TempDir()
  {
    if (std::filesystem::exists(path_))
    {
      std::filesystem::remove_all(path_);
    }
  }

  [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
  std::filesystem::path path_;
};

/// @test
/// Open a recording with one keyframe
/// @requirements(SEN-364)
TEST(InputTest, OpenRecordingWithOneKeyframe)
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
  const auto& summary = input.getSummary();

  EXPECT_EQ(1U, summary.keyframeCount);
}

/// @test
/// Open a recording with multiple keyframes and reads the summary
/// @requirements(SEN-364)
TEST(InputTest, OpenRecordingWithKeyframes)
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
    kernel.step();
    output.keyframe(kernel.getTime(), {});
    kernel.step();
    output.keyframe(kernel.getTime(), {});
  }

  Input input(archivePath.string(), kernel.getTypes());
  const auto& summary = input.getSummary();

  EXPECT_EQ(3U, summary.keyframeCount);
}

/// @test
/// Iterate through entries in a recording using cursor
/// @requirements(SEN-364)
TEST(InputTest, CanIterateThroughEntries)
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
    kernel.step();
    output.keyframe(kernel.getTime(), {});
    kernel.step();
    output.keyframe(kernel.getTime(), {});
  }

  Input input(archivePath.string(), kernel.getTypes());

  // Cursor starts at beginning (monostate), ++cursor reads entries until atEnd()
  auto cursor = input.begin();
  EXPECT_TRUE(cursor.atBegining());
  EXPECT_FALSE(cursor.atEnd());

  // Advance and check we can reach the end
  while (!cursor.atEnd())
  {
    ++cursor;
  }

  EXPECT_TRUE(cursor.atEnd());
}

/// @test
/// Handles truncated runtime gracefully
/// @requirements(SEN-364)
TEST(InputTest, TruncatedRuntimeHandledGracefully)
{
  TempDir tempDir;

  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;
  const auto runtimePath = archivePath / "runtime";

  {
    Output output(std::move(settings), []() {});

    for (int i = 0; i < 5; ++i)
    {
      output.keyframe(kernel.getTime(), {});
      kernel.step();
    }
  }

  const auto originalSize = std::filesystem::file_size(runtimePath);
  if (originalSize <= 50U)
  {
    GTEST_SKIP() << "Recording too small to truncate meaningfully";
  }

  // Truncate to simulate crash
  std::filesystem::resize_file(runtimePath, originalSize - 20);

  // Should not throw when opening or iterating
  EXPECT_NO_THROW({
    Input input(archivePath.string(), kernel.getTypes());
    auto cursor = input.begin();

    while (!cursor.atEnd())
    {
      ++cursor;
    }
  });
}

/// @test
/// Verifies that a truncated runtime file triggers EOF handling
/// @requirements(SEN-364)
TEST(InputTest, TruncatedRuntime_EOFProducesFewerEntries)
{
  TempDir tempDir;

  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  OutSettings settings;
  settings.name = "test";
  settings.folder = tempDir.path().string();
  settings.indexKeyframes = true;

  const auto archivePath = tempDir.path() / settings.name;
  const auto runtimePath = archivePath / "runtime";
  constexpr int writtenKeyframes = 10;

  {
    Output output(std::move(settings), []() {});

    for (int i = 0; i < writtenKeyframes; ++i)
    {
      output.keyframe(kernel.getTime(), {});
      kernel.step();
    }
  }

  // truncate: remove last byte to guarantee cutting mid-entry
  const auto originalSize = std::filesystem::file_size(runtimePath);
  std::filesystem::resize_file(runtimePath, originalSize - 1);

  // read and count entries
  Input input(archivePath.string(), kernel.getTypes());
  int readEntries = 0;
  auto cursor = input.begin();

  EXPECT_NO_THROW({
    while (!cursor.atEnd())
    {
      ++cursor;
      if (!cursor.atEnd())
      {
        ++readEntries;
      }
    }
  }) << "Reading a truncated file should handle EOF without throwing";

  // EOF handling: truncation must produce fewer entries than written
  EXPECT_GT(readEntries, 0) << "Should read at least some valid entries";
  EXPECT_EQ(readEntries, writtenKeyframes - 1) << "Truncation should invalidate the last entry (EOF handled)";
}

// TODO add more tests (SEN-1260)
}  // namespace sen::db::test
