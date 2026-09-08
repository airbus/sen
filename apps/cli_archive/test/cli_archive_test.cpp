// === cli_archive_test.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "../cli_archive_setup.h"

// sen
#include <sen/core/base/duration.h>
#include <sen/core/base/timestamp.h>

// google test
#include <gtest/gtest.h>

// cli11
#include <CLI/App.hpp>
#include <CLI/Error.hpp>

// gmock
#include <gmock/gmock-matchers.h>

// std
#include <filesystem>

/// @test
/// Checks sen archive show help information when --help flag is provided
TEST(CliArchive, Help)
{
  CLI::App app;
  app.name("sen archive");

  EXPECT_THROW(app.parse("--help"), CLI::CallForHelp);
}

/// @test
/// Checks sen archive info throws an exception when no path is provided
TEST(CliArchive, InfoNoPath)
{
  CLI::App app;
  app.name("sen archive");

  setupInfo(app);

  EXPECT_THROW(app.parse("info"), CLI::RequiredError);
}

/// @test
/// Checks sen archive info throws an exception when a non-existent path is provided
TEST(CliArchive, InfoNonExistentPath)
{
  CLI::App app;
  app.name("sen archive");

  setupInfo(app);

  EXPECT_THROW(app.parse("info ./nonExistentPath"), CLI::ValidationError);
}

/// @test
/// Checks sen archive info exits when an existing path does not contain archive files
TEST(CliArchive, InfoNonValidPath)
{
  CLI::App app;
  app.name("sen archive");

  setupInfo(app);

#ifdef _WIN32
  EXPECT_DEATH(app.parse("info ."), "file '\\.\\\\runtime' is not present");
#else
  EXPECT_DEATH(app.parse("info ."), "file '\\./runtime' is not present");
#endif
}

/// @test
/// Checks sen archive info shows the correct information when a valid path is provided
TEST(CliArchive, InfoSuccess)
{
  const std::filesystem::path archivePath = std::filesystem::path(TEST_DATA_DIR);

  ASSERT_TRUE(std::filesystem::exists(archivePath));

  CLI::App app;
  app.name("sen archive");

  setupInfo(app);

  const std::string command = "info " + archivePath.string();

  testing::internal::CaptureStdout();

  EXPECT_NO_THROW(app.parse(command));

  const auto output = testing::internal::GetCapturedStdout();

  ::sen::TimeStamp startTime(::sen::Duration(1785825920312357000));
  ::sen::TimeStamp endTime(::sen::Duration(1785826012579376000));

  EXPECT_THAT(output, testing::HasSubstr("path:            " + std::string(TEST_DATA_DIR)));
  EXPECT_THAT(output, testing::HasSubstr("duration:        92.267s"));
  EXPECT_THAT(output, testing::HasSubstr("start:           " + startTime.toLocalString()));
  EXPECT_THAT(output, testing::HasSubstr("end:             " + endTime.toLocalString()));
  EXPECT_THAT(output, testing::HasSubstr("objects:         1"));
  EXPECT_THAT(output, testing::HasSubstr("types:           7"));
  EXPECT_THAT(output, testing::HasSubstr("annotations:     0"));
  EXPECT_THAT(output, testing::HasSubstr("keyframes:       46"));
  EXPECT_THAT(output, testing::HasSubstr("indexed objects: 1"));
}

/// @test
/// Checks sen archive indexed throws an exception when no path is provided
TEST(CliArchive, IndexedNoPath)
{
  CLI::App app;
  app.name("sen archive");

  setupIndexed(app);

  EXPECT_THROW(app.parse("indexed"), CLI::RequiredError);
}

/// @test
/// Checks sen archive indexed throws an exception when a non-existent path is provided
TEST(CliArchive, IndexedNonExistentPath)
{
  CLI::App app;
  app.name("sen archive");

  setupIndexed(app);

  EXPECT_THROW(app.parse("indexed ./nonExistentPath"), CLI::ValidationError);
}

/// @test
/// Checks sen archive indexed exits when an existing path does not contain archive files
TEST(CliArchive, IndexedNonValidPath)
{
  CLI::App app;
  app.name("sen archive");

  setupIndexed(app);

#ifdef _WIN32
  EXPECT_DEATH(app.parse("indexed ."), "file '\\.\\\\runtime' is not present");
#else
  EXPECT_DEATH(app.parse("indexed ."), "file '\\./runtime' is not present");
#endif
}

/// @test
/// Checks sen archive indexed shows the correct information when a valid path is provided
TEST(CliArchive, IndexedSuccess)
{
  const std::filesystem::path archivePath = std::filesystem::path(TEST_DATA_DIR);

  ASSERT_TRUE(std::filesystem::exists(archivePath));

  CLI::App app;
  app.name("sen archive");

  setupIndexed(app);

  const std::string command = "indexed " + archivePath.string();

  testing::internal::CaptureStdout();

  EXPECT_NO_THROW(app.parse(command));

  const auto output = testing::internal::GetCapturedStdout();

  EXPECT_THAT(output, testing::HasSubstr("OBJECT NAME    TYPE NAME             BUS"));
  EXPECT_THAT(output, testing::HasSubstr("myObject       my_package.MyClass    local.example"));
}
