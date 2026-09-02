// === json_generator_test.cpp =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/gen/json.h"

// sen
#include "sen/core/lang/stl_resolver.h"

// 3rd party
#include <gtest/gtest.h>

// std
#include <string>
#include <vector>

namespace
{

// A single JsonGenerator instance must produce identical output across calls.
TEST(JsonGenerator, generatePackageIsIdempotentOnEmptyContext)
{
  sen::gen::JsonGenerator generator;
  const sen::lang::TypeSetContext empty;

  const std::string first = generator.generatePackage(empty);
  const std::string second = generator.generatePackage(empty);

  EXPECT_EQ(first, second);
}

TEST(JsonGenerator, generatePackageEmitsObjectShapeOnEmptyContext)
{
  sen::gen::JsonGenerator generator;
  const sen::lang::TypeSetContext empty;

  const std::string schema = generator.generatePackage(empty);

  // Structural shape: outermost `{ ... }` braces, ignoring surrounding whitespace.
  const auto first = schema.find_first_not_of(" \t\r\n");
  const auto last = schema.find_last_not_of(" \t\r\n");
  ASSERT_NE(first, std::string::npos);
  ASSERT_NE(last, std::string::npos);
  EXPECT_EQ(schema[first], '{');
  EXPECT_EQ(schema[last], '}');
}

TEST(JsonGenerator, combineSchemasIsIdempotent)
{
  sen::gen::JsonGenerator generator;
  const std::vector<std::string> inputs;

  const std::string first = generator.combineSchemas(inputs, "test");
  const std::string second = generator.combineSchemas(inputs, "test");

  EXPECT_EQ(first, second);
}

}  // namespace
