// === recorder_crash_test.cpp =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/meta/property.h"
#include "sen/db/input.h"
#include "sen/db/keyframe.h"
#include "sen/db/output.h"
#include "sen/db/property_change.h"
#include "sen/kernel/test_kernel.h"
#include "stl/sen/db/basic_types.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace
{

class RecorderCrashResilienceTest: public ::testing::Test
{
protected:
  void SetUp() override
  {
    testDir_ = std::filesystem::temp_directory_path() / ("recorder_test_" + std::to_string(std::time(nullptr)));
    std::filesystem::create_directories(testDir_);
  }

  void TearDown() override
  {
    if (std::filesystem::exists(testDir_))
    {
      std::filesystem::remove_all(testDir_);
    }
  }

  static void truncateFile(const std::filesystem::path& path, std::uintmax_t newSize)
  {
    std::filesystem::resize_file(path, newSize);
  }

  static std::uintmax_t getFileSize(const std::filesystem::path& path) { return std::filesystem::file_size(path); }

  [[nodiscard]] const std::filesystem::path& getTestDir() const { return testDir_; }

private:
  std::filesystem::path testDir_;
};

/// @test
/// Creates a valid recording that can be opened with db::Input
TEST_F(RecorderCrashResilienceTest, RecordingCreatesValidArchive)
{
  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  sen::db::OutSettings settings;
  settings.name = "test_recording";
  settings.folder = getTestDir().string();
  settings.indexKeyframes = true;

  const auto archivePath = getTestDir() / settings.name;

  {
    sen::db::Output output(std::move(settings), []() {});

    output.keyframe(kernel.getTime(), {});
    kernel.step();
    output.keyframe(kernel.getTime(), {});
    kernel.step();
    output.keyframe(kernel.getTime(), {});
  }

  sen::db::Input input(archivePath.string(), kernel.getTypes());
  const auto& summary = input.getSummary();

  EXPECT_EQ(3U, summary.keyframeCount) << "Recording should have 3 keyframes";
}

/// @test
/// A truncated recording can still be opened and read partially
TEST_F(RecorderCrashResilienceTest, TruncatedRecording_CanStillBeOpened)
{
  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  sen::db::OutSettings settings;
  settings.name = "test_recording";
  settings.folder = getTestDir().string();
  settings.indexKeyframes = true;

  const auto archivePath = getTestDir() / settings.name;
  const auto runtimePath = archivePath / "runtime";

  {
    sen::db::Output output(std::move(settings), []() {});

    for (int i = 0; i < 10; ++i)
    {
      output.keyframe(kernel.getTime(), {});
      kernel.step();
    }
  }

  ASSERT_TRUE(std::filesystem::exists(runtimePath)) << "runtime should exist";

  const auto originalSize = getFileSize(runtimePath);
  ASSERT_GT(originalSize, 100U) << "Recording should have significant data";

  truncateFile(runtimePath, originalSize * 7 / 10);

  EXPECT_NO_THROW({
    sen::db::Input input(archivePath.string(), kernel.getTypes());

    auto cursor = input.begin();
    while (!cursor.atEnd())
    {
      ++cursor;
    }
  });
}

/// @test
/// Types file is created when archive is created
TEST_F(RecorderCrashResilienceTest, TypesFileCreatedWithArchive)
{
  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  sen::db::OutSettings settings;
  settings.name = "test_recording";
  settings.folder = getTestDir().string();
  settings.indexKeyframes = true;

  const auto archivePath = getTestDir() / settings.name;

  {
    sen::db::Output output(std::move(settings), []() {});
    output.keyframe(kernel.getTime(), {});
  }

  const auto typesPath = archivePath / "types";
  const auto runtimePath = archivePath / "runtime";

  EXPECT_TRUE(std::filesystem::exists(typesPath)) << "types should exist";
  EXPECT_TRUE(std::filesystem::exists(runtimePath)) << "runtime should exist";
}

/// @test
/// Real crashed recording can be read and changes are recoverable
TEST_F(RecorderCrashResilienceTest, RealCrashedRecording_CanBeOpenedAndReadPartially)
{
#ifndef TEST_DATA_DIR
  GTEST_SKIP() << "TEST_DATA_DIR not defined";
#else
  const std::filesystem::path crashedRecordingPath = std::filesystem::path(TEST_DATA_DIR) / "crash_recording";

  ASSERT_TRUE(std::filesystem::exists(crashedRecordingPath)) << "Crashed recording test data should exist";

  auto kernel = sen::kernel::TestKernel::fromYamlString("");

  sen::db::Input input(crashedRecordingPath.string(), kernel.getTypes());
  const auto& summary = input.getSummary();

  EXPECT_GT(summary.keyframeCount, 0U) << "Crashed recording should have at least one keyframe";

  std::vector<double> speedChanges;
  auto cursor = input.begin();

  while (!cursor.atEnd())
  {
    ++cursor;
    const auto& entry = cursor.get();

    if (std::holds_alternative<sen::db::PropertyChange>(entry.payload))
    {
      const auto& change = std::get<sen::db::PropertyChange>(entry.payload);
      const auto* prop = change.getProperty();

      if (prop != nullptr && prop->getName() == "speed")
      {
        const auto& value = change.getValueAsVariant();

        speedChanges.push_back(value.getCopyAs<float64_t>());
      }
    }
  }

  ASSERT_GE(speedChanges.size(), 2U) << "Should have at least 2 speed changes in the recording";
  EXPECT_EQ(speedChanges.front(), 250.0) << "First speed change should be 250";
  EXPECT_EQ(speedChanges.back(), 123.0) << "Last speed change should be 123";
#endif
}

}  // namespace
