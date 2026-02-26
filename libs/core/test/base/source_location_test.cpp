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
#include <string>

using sen::SourceLocation;

/// @test
/// Checks constructor of source location struct
/// @requirements(SEN-579)
TEST(SourceLocation, constructor)
{
  const SourceLocation object {"path/a/b/c/file.extension", 0};
  EXPECT_EQ(object.fileName.data(), std::string("path/a/b/c/file.extension"));
  EXPECT_EQ(object.lineNumber, 0);
}

/// @test
/// Checks usage of macro in local scope to construct current file source location
/// @requirements(SEN-579)
TEST(SourceLocation, macro)
{
  SourceLocation object {};
  object = SEN_SL();
  EXPECT_EQ(object.fileName.data(), std::string("source_location_test.cpp"));
  EXPECT_EQ(object.lineNumber, __LINE__ - 2);
}
