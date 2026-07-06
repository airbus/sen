// === init_package_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "../util.h"

// google test
#include <gtest/gtest.h>

// cli11
#include <CLI/App.hpp>
#include <CLI/CLI.hpp>  // NOLINT
#include <CLI/Error.hpp>

// std
#include <filesystem>

/// @test
/// Check sen package init fails when no path is provided
TEST(CliPackageInit, NoPath)
{
  CLI::App app;
  app.name("sen package");

  setupInitPackage(app);

  EXPECT_THROW(app.parse("init"), CLI::RequiredError);
}

/// @test
/// Check sen package init shows help information when help flag is provided
TEST(CliPackageInit, Help)
{
  CLI::App app;
  app.name("sen package");

  setupInitPackage(app);

  EXPECT_THROW(app.parse("init --help"), CLI::CallForHelp);
}

/// @test
/// Check sen package init fails when no class is provided
TEST(CliPackageInit, NoClass)
{
  CLI::App app;
  app.name("sen package");

  setupInitPackage(app);

  EXPECT_THROW(app.parse("init test_package"), CLI::RequiredError);
}

/// @test
/// Check sen package init fails when the path provided already exists
TEST(CliPackageInit, DirectoryAlreadyExists)
{
  CLI::App app;
  app.name("sen package");

  setupInitPackage(app);

  ASSERT_NO_THROW(std::filesystem::create_directories("test_package"));

  EXPECT_EXIT(app.parse("init test_package --class TestClass"),
              ::testing::ExitedWithCode(1),
              "directory test_package already exists\n");

  std::filesystem::remove_all("test_package");
}

/// @test
/// Check sen package init creates all expected directories and files when no errors occur
TEST(CliPackageInit, NoErrors)
{
  CLI::App app;
  app.name("sen package");

  setupInitPackage(app);

  ASSERT_NO_THROW(app.parse("init test_package --class TestClass"));

  ASSERT_TRUE(std::filesystem::is_directory("test_package"));
  ASSERT_TRUE(std::filesystem::is_directory("test_package/src"));
  ASSERT_TRUE(std::filesystem::is_directory("test_package/stl"));
  ASSERT_TRUE(std::filesystem::is_directory("test_package/stl/test_package"));

  EXPECT_TRUE(std::filesystem::exists("test_package/CMakeLists.txt"));
  EXPECT_TRUE(std::filesystem::exists("test_package/config.yaml"));
  EXPECT_TRUE(std::filesystem::exists("test_package/src/test_class.h"));
  EXPECT_TRUE(std::filesystem::exists("test_package/src/test_class.cpp"));
  EXPECT_TRUE(std::filesystem::exists("test_package/stl/test_package/basic_types.stl"));
  EXPECT_TRUE(std::filesystem::exists("test_package/stl/test_package/test_class.stl"));

  std::filesystem::remove_all("test_package");
}

/// @test
/// Check sen package init fails when an extra argument is provided
TEST(CliPackageInit, ExtraArgument)
{
  CLI::App app;
  app.name("sen package");

  setupInitPackage(app);

  EXPECT_THROW(app.parse("init test_package --class TestClass test"), CLI::ExtrasError);
}

/// @test
/// Check sen package init fails when class argument is provided twice
TEST(CliPackageInit, ArgumentMismatch)
{
  CLI::App app;
  app.name("sen package");

  setupInitPackage(app);

  EXPECT_THROW(app.parse("init test_package --class TestClass --class TestClass"), CLI::ArgumentMismatch);
}
