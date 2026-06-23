// === init_component_test.cpp =========================================================================================
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
#include <CLI/Error.hpp>

// std
#include <filesystem>

/// @test
/// Check sen package init-component fails when no path is provided
TEST(CliPackageInitComponent, NoPath)
{
  CLI::App app;
  app.name("sen package");

  setupInitComponent(app);

  EXPECT_THROW(app.parse("init-component"), CLI::RequiredError);
}

/// @test
/// Check sen package init-component shows help information when help flag is provided
TEST(CliPackageInitComponent, Help)
{
  CLI::App app;
  app.name("sen package");

  setupInitComponent(app);

  EXPECT_THROW(app.parse("init-component --help"), CLI::CallForHelp);
}

/// @test
/// Check sen package init-component fails when path file is not in UpperCamelCase
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
/// Check sen package init-component fails when the path provided already exists
TEST(CliPackageInitComponent, DirectoryAlreadyExists)
{
  CLI::App app;
  app.name("sen package");

  setupInitComponent(app);

  ASSERT_NO_THROW(std::filesystem::create_directories("test_component"));

  EXPECT_EXIT(app.parse("init-component TestComponent"),
              ::testing::ExitedWithCode(2),
              "directory test_component already exists\n");

  std::filesystem::remove_all("test_component");
}

/// @test
/// Check sen package init-component creates all expected directories and files when no errors occur
TEST(CliPackageInitComponent, NoFlags)
{
  CLI::App app;
  app.name("sen package");

  setupInitComponent(app);

  ASSERT_NO_THROW(app.parse("init-component TestComponent"));

  ASSERT_TRUE(std::filesystem::is_directory("test_component"));
  ASSERT_TRUE(std::filesystem::is_directory("test_component/src"));
  ASSERT_FALSE(std::filesystem::is_directory("test_component/stl"));

  EXPECT_TRUE(std::filesystem::exists("test_component/CMakeLists.txt"));
  EXPECT_TRUE(std::filesystem::exists("test_component/src/component.cpp"));

  std::filesystem::remove_all("test_component");
}

/// @test
/// Check sen package init-component creates all expected directories and files when --with-config flag is provided
TEST(CliPackageInitComponent, WithConfigFlag)
{
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

  std::filesystem::remove_all("test_component");
}

/// @test
/// Check sen package init-component creates all expected directories and files when --full flag is provided
TEST(CliPackageInitComponent, FullFlag)
{
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

  std::filesystem::remove_all("test_component");
}

/// @test
/// Check sen package init-component fails when both flags are provided
TEST(CliPackageInitComponent, AllFlags)
{
  CLI::App app;
  app.name("sen package");

  setupInitComponent(app);

  ASSERT_THROW(app.parse("init-component TestComponent --with-config --full"), CLI::ExcludesError);
}

/// @test
/// Check sen package init-component fails when an extra argument is provided
TEST(CliPackageInitComponent, ExtraArgument)
{
  CLI::App app;
  app.name("sen package");

  setupInitComponent(app);

  EXPECT_THROW(app.parse("init-component TestComponent test"), CLI::ExtrasError);
}
