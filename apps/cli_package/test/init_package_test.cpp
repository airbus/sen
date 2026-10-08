// === init_package_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "../util.h"
#include "scoped_work_dir.h"

// google test
#include <gtest/gtest.h>

// cli11
#include <CLI/App.hpp>
#include <CLI/CLI.hpp>  // NOLINT
#include <CLI/Error.hpp>

// std
#include <filesystem>

/// @test
/// Rejects an init command line that omits the required package path argument.
TEST(CliPackageInit, NoPath)
{
  CLI::App app;
  app.name("sen package");

  setupInitPackage(app);

  EXPECT_THROW(app.parse("init"), CLI::RequiredError);
}

/// @test
/// Treats init --help as a call for help instead of creating a package.
TEST(CliPackageInit, Help)
{
  CLI::App app;
  app.name("sen package");

  setupInitPackage(app);

  EXPECT_THROW(app.parse("init --help"), CLI::CallForHelp);
}

/// @test
/// Rejects an init command line that omits the required --class option.
TEST(CliPackageInit, NoClass)
{
  CLI::App app;
  app.name("sen package");

  setupInitPackage(app);

  EXPECT_THROW(app.parse("init test_package"), CLI::RequiredError);
}

/// @test
/// Exits with code 1 and reports the clash when the target package directory already exists.
TEST(CliPackageInit, DirectoryAlreadyExists)
{
  const ScopedWorkDir workDir;

  CLI::App app;
  app.name("sen package");

  setupInitPackage(app);

  ASSERT_NO_THROW(std::filesystem::create_directories("test_package"));

  EXPECT_EXIT(app.parse("init test_package --class TestClass"),
              ::testing::ExitedWithCode(1),
              "directory test_package already exists\n");
}

/// @test
/// Creates the full package skeleton for init test_package --class TestClass, with CMakeLists.txt,
/// config.yaml, snake_case class sources under src, and basic_types.stl plus test_class.stl under
/// stl/test_package.
TEST(CliPackageInit, NoErrors)
{
  const ScopedWorkDir workDir;

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
}

/// @test
/// Rejects an init command line that carries an extra positional argument after the package path.
TEST(CliPackageInit, ExtraArgument)
{
  CLI::App app;
  app.name("sen package");

  setupInitPackage(app);

  EXPECT_THROW(app.parse("init test_package --class TestClass test"), CLI::ExtrasError);
}

/// @test
/// Rejects an init command line that passes the --class option twice.
TEST(CliPackageInit, ArgumentMismatch)
{
  CLI::App app;
  app.name("sen package");

  setupInitPackage(app);

  EXPECT_THROW(app.parse("init test_package --class TestClass --class TestClass"), CLI::ArgumentMismatch);
}
