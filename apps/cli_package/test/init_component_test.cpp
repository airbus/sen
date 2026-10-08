// === init_component_test.cpp =========================================================================================
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
#include <CLI/Error.hpp>

// std
#include <filesystem>

/// @test
/// Rejects an init-component command line that omits the required component name argument.
TEST(CliPackageInitComponent, NoPath)
{
  CLI::App app;
  app.name("sen package");

  setupInitComponent(app);

  EXPECT_THROW(app.parse("init-component"), CLI::RequiredError);
}

/// @test
/// Treats init-component --help as a call for help instead of creating a component.
TEST(CliPackageInitComponent, Help)
{
  CLI::App app;
  app.name("sen package");

  setupInitComponent(app);

  EXPECT_THROW(app.parse("init-component --help"), CLI::CallForHelp);
}

/// @test
/// Rejects the lower-case component name test_component, exiting with code 1 and the message
/// that component names must be upper camel cased.
TEST(CliPackageInitComponent, NoUpperCammelCase)
{
  CLI::App app;
  app.name("sen package");

  setupInitComponent(app);

  EXPECT_EXIT(app.parse("init-component test_component"),
              ::testing::ExitedWithCode(1),
              "component names must be upper camel cased\n");
}

/// @test
/// Exits with code 2 and reports the clash when the snake_case directory derived from the
/// component name already exists.
TEST(CliPackageInitComponent, DirectoryAlreadyExists)
{
  const ScopedWorkDir workDir;

  CLI::App app;
  app.name("sen package");

  setupInitComponent(app);

  ASSERT_NO_THROW(std::filesystem::create_directories("test_component"));

  EXPECT_EXIT(app.parse("init-component TestComponent"),
              ::testing::ExitedWithCode(2),
              "directory test_component already exists\n");
}

/// @test
/// Creates the snake_case component directory holding CMakeLists.txt and src/component.cpp,
/// and no stl directory, when init-component runs without flags.
TEST(CliPackageInitComponent, NoFlags)
{
  const ScopedWorkDir workDir;

  CLI::App app;
  app.name("sen package");

  setupInitComponent(app);

  ASSERT_NO_THROW(app.parse("init-component TestComponent"));

  ASSERT_TRUE(std::filesystem::is_directory("test_component"));
  ASSERT_TRUE(std::filesystem::is_directory("test_component/src"));
  ASSERT_FALSE(std::filesystem::is_directory("test_component/stl"));

  EXPECT_TRUE(std::filesystem::exists("test_component/CMakeLists.txt"));
  EXPECT_TRUE(std::filesystem::exists("test_component/src/component.cpp"));
}

/// @test
/// Creates the base component files plus an stl/test_component/config.stl skeleton when
/// init-component runs with the --with-config flag.
TEST(CliPackageInitComponent, WithConfigFlag)
{
  const ScopedWorkDir workDir;

  CLI::App app;
  app.name("sen package");

  setupInitComponent(app);

  ASSERT_NO_THROW(app.parse("init-component TestComponent --with-config"));

  ASSERT_TRUE(std::filesystem::is_directory("test_component"));
  ASSERT_TRUE(std::filesystem::is_directory("test_component/src"));
  ASSERT_TRUE(std::filesystem::is_directory("test_component/stl"));
  ASSERT_TRUE(std::filesystem::is_directory("test_component/stl/test_component"));

  EXPECT_TRUE(std::filesystem::exists("test_component/CMakeLists.txt"));
  EXPECT_TRUE(std::filesystem::exists("test_component/src/component.cpp"));
  EXPECT_TRUE(std::filesystem::exists("test_component/stl/test_component/config.stl"));
}

/// @test
/// Creates the base component files plus an stl/test_component/config.stl skeleton when
/// init-component runs with the --full flag.
TEST(CliPackageInitComponent, FullFlag)
{
  const ScopedWorkDir workDir;

  CLI::App app;
  app.name("sen package");

  setupInitComponent(app);

  ASSERT_NO_THROW(app.parse("init-component TestComponent --full"));

  ASSERT_TRUE(std::filesystem::is_directory("test_component"));
  ASSERT_TRUE(std::filesystem::is_directory("test_component/src"));
  ASSERT_TRUE(std::filesystem::is_directory("test_component/stl"));
  ASSERT_TRUE(std::filesystem::is_directory("test_component/stl/test_component"));

  EXPECT_TRUE(std::filesystem::exists("test_component/CMakeLists.txt"));
  EXPECT_TRUE(std::filesystem::exists("test_component/src/component.cpp"));
  EXPECT_TRUE(std::filesystem::exists("test_component/stl/test_component/config.stl"));
}

/// @test
/// Rejects an init-component command line that combines the mutually exclusive --with-config
/// and --full flags.
TEST(CliPackageInitComponent, AllFlags)
{
  CLI::App app;
  app.name("sen package");

  setupInitComponent(app);

  ASSERT_THROW(app.parse("init-component TestComponent --with-config --full"), CLI::ExcludesError);
}

/// @test
/// Rejects an init-component command line that carries an extra positional argument after the
/// component name.
TEST(CliPackageInitComponent, ExtraArgument)
{
  CLI::App app;
  app.name("sen package");

  setupInitComponent(app);

  EXPECT_THROW(app.parse("init-component TestComponent test"), CLI::ExtrasError);
}
