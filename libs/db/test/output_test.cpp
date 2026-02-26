// === output_test.cpp =================================================================================================
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
/// Creates archive directory and all required files
/// @requirements(SEN-364)
TEST(OutputTest, CreatesArchiveFiles)
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

  EXPECT_TRUE(std::filesystem::exists(archivePath / "summary"));
  EXPECT_TRUE(std::filesystem::exists(archivePath / "types"));
  EXPECT_TRUE(std::filesystem::exists(archivePath / "runtime"));
  EXPECT_TRUE(std::filesystem::exists(archivePath / "indexes"));
}

/// @test
/// Keyframe increments summary count
/// @requirements(SEN-364)
TEST(OutputTest, KeyframeIncrementsSummary)
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
/// Types file is created with archive
/// @requirements(SEN-364)
TEST(OutputTest, TypesFileIsCreated)
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

  EXPECT_TRUE(std::filesystem::exists(archivePath / "types"));
}

// TODO add more tests (SEN-1260)
}  // namespace sen::db::test
