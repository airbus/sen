// === string_case_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/lang/string_utils.h"

// google test
#include <gtest/gtest.h>

namespace sen
{

/// @test
/// toUpperCamelCase strips separators and uppercases the first character.
TEST(StringCaseTest, toUpperCamelCaseBasicConversion)
{
  EXPECT_EQ(capitalizeAndRemoveSeparators(""), "");
  EXPECT_EQ(capitalizeAndRemoveSeparators("foo"), "Foo");
  EXPECT_EQ(capitalizeAndRemoveSeparators("foo-bar"), "Foobar");
  EXPECT_EQ(capitalizeAndRemoveSeparators("foo_bar"), "Foobar");
  EXPECT_EQ(capitalizeAndRemoveSeparators("FooBar"), "FooBar");
}

/// @test
/// snakeCaseToPascalCase treats underscores as word separators and capitalizes each word's first
/// character, in contrast with toUpperCamelCase which only capitalizes the leading character.
TEST(StringCaseTest, toPascalCaseBasicConversion)
{
  EXPECT_EQ(snakeCaseToPascalCase(""), "");
  EXPECT_EQ(snakeCaseToPascalCase("foo"), "Foo");
  EXPECT_EQ(snakeCaseToPascalCase("foo_bar"), "FooBar");
  EXPECT_EQ(snakeCaseToPascalCase("option_type"), "OptionType");
  EXPECT_EQ(snakeCaseToPascalCase("already_PascalCase"), "AlreadyPascalCase");
}

/// @test
/// pascalCaseToSnakeCase inserts underscores before every uppercase letter except the first, and
/// lowercases the full result.
TEST(StringCaseTest, toSnakeCaseBasicConversion)
{
  EXPECT_EQ(pascalCaseToSnakeCase(""), "");
  EXPECT_EQ(pascalCaseToSnakeCase("Foo"), "foo");
  EXPECT_EQ(pascalCaseToSnakeCase("FooBar"), "foo_bar");
  EXPECT_EQ(pascalCaseToSnakeCase("MyHttpServer"), "my_http_server");
  EXPECT_EQ(pascalCaseToSnakeCase("alreadyLower"), "already_lower");
}

/// @test
/// isCapitalizedAndNoSeparators accepts names that start with uppercase and contain no spaces or
/// underscores, and rejects everything else.
TEST(StringCaseTest, isUpperCamelCaseAcceptance)
{
  EXPECT_TRUE(isCapitalizedAndNoSeparators(""));
  EXPECT_TRUE(isCapitalizedAndNoSeparators("Foo"));
  EXPECT_TRUE(isCapitalizedAndNoSeparators("FooBar"));
  EXPECT_TRUE(isCapitalizedAndNoSeparators("A"));

  EXPECT_FALSE(isCapitalizedAndNoSeparators("foo"));
  EXPECT_FALSE(isCapitalizedAndNoSeparators("fooBar"));
  EXPECT_FALSE(isCapitalizedAndNoSeparators("Foo Bar"));
  EXPECT_FALSE(isCapitalizedAndNoSeparators("Foo_Bar"));
}

}  // namespace sen
