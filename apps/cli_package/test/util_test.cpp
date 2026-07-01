// === util_test.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "../util.h"
#include "test_txt.h"

// inja
#include <inja/json.hpp>

// google test
#include <gtest/gtest.h>

// std
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

/// @test
/// Check writeFile creates the file successfully and writes the content correctly
TEST(CliPackageUtil, WriteFile)
{
  inja::json packageData;
  packageData["package_name"] = "MyPackage";
  packageData["class_name"] = "MyClass";

  writeFile("testTemplate.txt", test_txt, test_txtSize, packageData);

  std::ifstream testFile("testTemplate.txt");
  std::stringstream readBuffer;
  readBuffer << testFile.rdbuf();
  std::string testFileContent = readBuffer.str();

  EXPECT_EQ(testFileContent, "Package name: MyPackage, class name: MyClass\n");

  testFile.close();
  std::filesystem::remove("testTemplate.txt");
}
