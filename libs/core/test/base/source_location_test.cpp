// === source_location_test.cpp ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/source_location.h"

// google test
#include <gtest/gtest.h>

// std
#include <string_view>

using sen::SourceLocation;

/// @test
/// Checks default constructor of source location struct
/// @requirements(SEN-579)
TEST(SourceLocation, defaultConstructor)
{
  constexpr SourceLocation object;
  EXPECT_EQ(object.fileName, "file name not set");
  EXPECT_EQ(object.lineNumber, -1);
  EXPECT_EQ(object.functionName, "function name not set");
}

/// @test
/// Checks constructor of source location struct with parameters
/// @requirements(SEN-579)
TEST(SourceLocation, parameterConstructor)
{
  constexpr SourceLocation object {"path/a/b/c/file.extension", 0, "myFunction"};
  EXPECT_EQ(object.fileName, "path/a/b/c/file.extension");
  EXPECT_EQ(object.lineNumber, 0);
  EXPECT_EQ(object.functionName, "myFunction");
}

/// @test
/// Checks usage of macro in local scope to construct current file source location
/// @requirements(SEN-579)
TEST(SourceLocation, macro)
{
  const SourceLocation object = SEN_SL();
  EXPECT_EQ(object.fileName, "source_location_test.cpp");
  EXPECT_EQ(object.lineNumber, __LINE__ - 2);
  EXPECT_TRUE(object.functionName.find("TestBody") != std::string_view::npos);
}

/// @test
/// Checks the path parsing logic evaluates correctly at runtime and compile-time
/// @requirements(SEN-579)
TEST(SourceLocation, getFilenameOffset)
{
  EXPECT_EQ(sen::impl::getFilenameOffset("folder/subfolder/file.cpp"), 17);
  EXPECT_EQ(sen::impl::getFilenameOffset("folder\\subfolder\\file.cpp"), 17);
  EXPECT_EQ(sen::impl::getFilenameOffset("file.cpp"), 0);
  EXPECT_EQ(sen::impl::getFilenameOffset(""), 0);
  EXPECT_EQ(sen::impl::getFilenameOffset("/"), 1);

  static_assert(sen::impl::getFilenameOffset("a/b.txt") == 2);
  static_assert(sen::impl::getFilenameOffset("plain.txt") == 0);
}
