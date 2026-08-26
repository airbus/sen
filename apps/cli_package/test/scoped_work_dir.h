// === scoped_work_dir.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_APPS_CLI_PACKAGE_TEST_SCOPED_WORK_DIR_H
#define SEN_APPS_CLI_PACKAGE_TEST_SCOPED_WORK_DIR_H

// google test
#include <gtest/gtest.h>

// std
#include <filesystem>
#include <random>
#include <string>

/// Runs a test in a directory of its own: ctest runs one process per test from a shared working
/// directory, so the fixed relative paths these tests create would collide.
class ScopedWorkDir
{
public:
  ScopedWorkDir()
  {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    // The suffix separates concurrent runs of the same test, which the name alone does not.
    const std::string name =
      std::string(info->test_suite_name()) + "." + info->name() + "." + std::to_string(std::random_device {}());

    previous_ = std::filesystem::current_path();
    dir_ = std::filesystem::temp_directory_path() / name;

    std::filesystem::create_directories(dir_);
    std::filesystem::current_path(dir_);
  }

  ~ScopedWorkDir()
  {
    std::error_code error;
    std::filesystem::current_path(previous_, error);
    std::filesystem::remove_all(dir_, error);
  }

  ScopedWorkDir(const ScopedWorkDir&) = delete;
  ScopedWorkDir(ScopedWorkDir&&) = delete;
  ScopedWorkDir& operator=(const ScopedWorkDir&) = delete;
  ScopedWorkDir& operator=(ScopedWorkDir&&) = delete;

private:
  std::filesystem::path previous_;
  std::filesystem::path dir_;
};

#endif  // SEN_APPS_CLI_PACKAGE_TEST_SCOPED_WORK_DIR_H
