// === string_case_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/base/string_case.h"

// google test
#include <gtest/gtest.h>

namespace sen
{

/// @test
/// toUpperCamelCase strips separators and uppercases the first character.
TEST(StringCaseTest, toUpperCamelCaseBasicConversion)
{
  EXPECT_EQ(toUpperCamelCase(""), "");
  EXPECT_EQ(toUpperCamelCase("foo"), "Foo");
  EXPECT_EQ(toUpperCamelCase("foo-bar"), "Foobar");
  EXPECT_EQ(toUpperCamelCase("foo_bar"), "Foobar");
  EXPECT_EQ(toUpperCamelCase("FooBar"), "FooBar");
}

/// @test
/// toPascalCase treats underscores as word separators and capitalizes each word's first
/// character, in contrast with toUpperCamelCase which only capitalizes the leading character.
TEST(StringCaseTest, toPascalCaseBasicConversion)
{
  EXPECT_EQ(toPascalCase(""), "");
  EXPECT_EQ(toPascalCase("foo"), "Foo");
  EXPECT_EQ(toPascalCase("foo_bar"), "FooBar");
  EXPECT_EQ(toPascalCase("option_type"), "OptionType");
  EXPECT_EQ(toPascalCase("already_PascalCase"), "AlreadyPascalCase");
}

/// @test
/// toSnakeCase inserts underscores before every uppercase letter except the first, and
/// lowercases the full result.
TEST(StringCaseTest, toSnakeCaseBasicConversion)
{
  EXPECT_EQ(toSnakeCase(""), "");
  EXPECT_EQ(toSnakeCase("Foo"), "foo");
  EXPECT_EQ(toSnakeCase("FooBar"), "foo_bar");
  EXPECT_EQ(toSnakeCase("MyHttpServer"), "my_http_server");
  EXPECT_EQ(toSnakeCase("alreadyLower"), "already_lower");
}

/// @test
/// isUpperCamelCase accepts names that start with uppercase and contain no spaces or
/// underscores, and rejects everything else.
TEST(StringCaseTest, isUpperCamelCaseAcceptance)
{
  EXPECT_TRUE(isUpperCamelCase(""));
  EXPECT_TRUE(isUpperCamelCase("Foo"));
  EXPECT_TRUE(isUpperCamelCase("FooBar"));
  EXPECT_TRUE(isUpperCamelCase("A"));

  EXPECT_FALSE(isUpperCamelCase("foo"));
  EXPECT_FALSE(isUpperCamelCase("fooBar"));
  EXPECT_FALSE(isUpperCamelCase("Foo Bar"));
  EXPECT_FALSE(isUpperCamelCase("Foo_Bar"));
}

}  // namespace sen
