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
/// Defaults to placeholder state, fileName and functionName reading as not set and lineNumber as -1.
/// @requirements(SEN-579)
TEST(SourceLocation, defaultConstructor)
{
  constexpr SourceLocation object;
  EXPECT_EQ(object.fileName, "file name not set");
  EXPECT_EQ(object.lineNumber, -1);
  EXPECT_EQ(object.functionName, "function name not set");
}

/// @test
/// Stores the file name, line number, and function name given at construction unchanged, keeping the full
/// path.
/// @requirements(SEN-579)
TEST(SourceLocation, parameterConstructor)
{
  constexpr SourceLocation object {"path/a/b/c/file.extension", 0, "myFunction"};
  EXPECT_EQ(object.fileName, "path/a/b/c/file.extension");
  EXPECT_EQ(object.lineNumber, 0);
  EXPECT_EQ(object.functionName, "myFunction");
}

/// @test
/// Captures the invocation site through SEN_SL, the file name trimmed to its basename, the line number
/// matching the invoking line, and the function name containing TestBody.
/// @requirements(SEN-579)
TEST(SourceLocation, macro)
{
  const SourceLocation object = SEN_SL();
  EXPECT_EQ(object.fileName, "source_location_test.cpp");
  EXPECT_EQ(object.lineNumber, __LINE__ - 2);
  EXPECT_TRUE(object.functionName.find("TestBody") != std::string_view::npos);
}

/// @test
/// Returns the offset where the basename starts for forward and backward slash paths, 0 for a bare name or an
/// empty string, and 1 for a lone slash, evaluating both at runtime and in constexpr context.
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
