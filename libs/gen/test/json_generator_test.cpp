// === json_generator_test.cpp =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/gen/json.h"

// sen
#include "sen/core/lang/stl_resolver.h"

// test support
#include "every_kind_model.h"

// 3rd party
#include <gtest/gtest.h>

// std
#include <string>
#include <vector>

namespace
{

// A single JsonGenerator instance must produce identical output across calls.
/// @test
/// generatePackage returns the same schema each time it is called on an empty context.
TEST(JsonGenerator, generatePackageIsIdempotentOnEmptyContext)
{
  sen::gen::JsonGenerator generator;
  const sen::lang::TypeSetContext empty;

  const std::string first = generator.generatePackage(empty);
  const std::string second = generator.generatePackage(empty);

  EXPECT_EQ(first, second);
}

/// @test
/// The schema for an empty context is a JSON object.
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

/// @test
/// combineSchemas returns the same text each time it is called with the same inputs.
TEST(JsonGenerator, combineSchemasIsIdempotent)
{
  sen::gen::JsonGenerator generator;
  const std::vector<std::string> inputs;

  const std::string first = generator.combineSchemas(inputs, "test");
  const std::string second = generator.combineSchemas(inputs, "test");

  EXPECT_EQ(first, second);
}

/// The text of the schema's `"required"` array, so membership can be asked rather than guessed at
/// from substring order.
[[nodiscard]] std::string requiredArray(const std::string& schema)
{
  const auto key = schema.find("\"required\"");
  if (key == std::string::npos)
  {
    return {};
  }
  const auto open = schema.find('[', key);
  const auto close = schema.find(']', open);
  if (open == std::string::npos || close == std::string::npos)
  {
    return {};
  }
  return schema.substr(open, close - open + 1);
}

// A config writes a duration with its unit -- `2 s` -- which in YAML is a string, and a bare number
// of nanoseconds is accepted too. Emitting "integer" alone made every shipped config fail its own
// schema while the runtime accepted it.
/// @test
/// A Duration property accepts a string or an integer in the configuration schema.
TEST(JsonGenerator, aDurationPropertyTakesEitherFormInTheConfigSchema)
{
  const sen::gen::test::ResolvedModel model {R"(package d.test;

class Timed
{
  var period : Duration [static];
}
)"};

  sen::gen::JsonGenerator generator;
  const std::string schema = generator.generatePackage(model.context());

  ASSERT_NE(schema.find("\"period\""), std::string::npos) << schema;
  EXPECT_NE(schema.find(R"("type": ["string", "integer"])"), std::string::npos) << schema;
}

// Every property was listed as required, so a config leaving out a read-only one failed validation.
/// @test
/// Only a static writable property is listed as required. A read-only static property and a
/// dynamic one are not, so a configuration may leave them out.
TEST(JsonGenerator, onlyStaticWritablePropertiesAreRequired)
{
  const sen::gen::test::ResolvedModel model {R"(package d.test;

class Mixed
{
  var settable : Duration [static];
  var atStartup : Duration [static_no_config];
  var observed : Duration;
}
)"};

  sen::gen::JsonGenerator generator;
  const std::string schema = generator.generatePackage(model.context());
  const std::string required = requiredArray(schema);

  ASSERT_FALSE(required.empty()) << schema;
  EXPECT_NE(required.find("settable"), std::string::npos) << required;
  EXPECT_EQ(required.find("observed"), std::string::npos) << required;
  // Read-only static, which carries the same `static` flag as the one above it. Without this the
  // test holds for a filter on `static` rather than on the read-write category.
  EXPECT_EQ(required.find("atStartup"), std::string::npos) << required;
}

}  // namespace
