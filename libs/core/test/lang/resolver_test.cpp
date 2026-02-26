// === resolver_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/lang/stl_parser.h"
#include "sen/core/lang/stl_resolver.h"
#include "sen/core/lang/stl_scanner.h"
#include "sen/core/lang/stl_statement.h"
#include "sen/core/meta/class_type.h"

// google test
#include <gtest/gtest.h>

// std
#include <string>
#include <tuple>
#include <utility>
#include <vector>

void expectInvalidResolution(const std::string& program)
{
  sen::lang::StlScanner scanner(program);
  auto tokens = scanner.scanTokens();
  EXPECT_FALSE(tokens.empty());

  sen::lang::StlParser parser(tokens);
  std::vector<sen::lang::StlStatement> statements;
  EXPECT_NO_THROW(statements = parser.parse());

  sen::lang::ResolverContext localContext;
  sen::lang::TypeSetContext globalContext;

  sen::lang::StlResolver resolver(statements, localContext, globalContext);
  EXPECT_ANY_THROW(auto result = resolver.resolve({}));
}

const sen::lang::TypeSet* expectValidResolution(const std::string& program,
                                                sen::lang::ResolverContext& localContext,
                                                sen::lang::TypeSetContext& globalContext,
                                                const sen::lang::TypeSettings& settings = {})
{
  sen::lang::StlScanner scanner(program);
  auto tokens = scanner.scanTokens();
  EXPECT_FALSE(tokens.empty());

  sen::lang::StlParser parser(tokens);
  std::vector<sen::lang::StlStatement> statements;
  EXPECT_NO_THROW(statements = parser.parse());

  sen::lang::StlResolver resolver(statements, localContext, globalContext);
  return resolver.resolve(settings);
}

/// @test
/// Checks that classes cannot be used as property types
/// @requirements(SEN-903)
TEST(Resolver, invalidPropertyType)
{
  expectInvalidResolution(R"(
package my.test;
class SomeClass {}
class TestClass
{
  var x : SomeClass;
}

    )");
}

/// @test
/// Checks that classes cannot be used as function return types
/// @requirements(SEN-903)
TEST(Resolver, invalidReturnType)
{
  expectInvalidResolution(R"(
package my.test;
class SomeClass {}
class TestClass
{
    fn invalidFunction() -> SomeClass;
}

    )");
}

/// @test
/// Checks that classes cannot be used as function arguments
/// @requirements(SEN-903)
TEST(Resolver, invalidFunctionArgument)
{
  expectInvalidResolution(R"(
package my.test;
class SomeClass {}
class TestClass
{
    fn invalidFunction(arg: SomeClass) -> bool;
}

    )");
}

/// @test
/// Checks that classes cannot be used as event arguments
/// @requirements(SEN-903)
TEST(Resolver, invalidEventArgument)
{
  expectInvalidResolution(R"(
package my.test;
class SomeClass {}
class TestClass
{
    event invalidEvent(arg: SomeClass);
}

    )");
}

/// @test
/// Checks that classes cannot be used as struct fields
/// @requirements(SEN-903)
TEST(Resolver, invalidStructField)
{
  std::string program = R"(
package my.test;
class SomeClass {}
struct SomeStruct
{
  x : SomeClass,
  y : u32
}

    )";

  expectInvalidResolution(program);
}

/// @test
/// Checks that classes cannot be used as variant elements
/// @requirements(SEN-903)
TEST(Resolver, invalidVariantField)
{
  expectInvalidResolution(R"(
package my.test;
class SomeClass {}
variant SomeVariant { u32, SomeClass }
    )");
}

/// @test
/// Checks that classes cannot be used as the type of unbounded sequences
/// @requirements(SEN-903)
TEST(Resolver, invalidUnboundedSequence)
{
  expectInvalidResolution(R"(
package my.test;
class SomeClass {}
sequence<SomeClass> MyInvalidSequence;
    )");
}

/// @test
/// Checks that classes cannot be used as the type of bounded sequences
/// @requirements(SEN-903)
TEST(Resolver, invalidBoundedSequence)
{
  expectInvalidResolution(R"(
package my.test;
class SomeClass {}
sequence<SomeClass, 12> MyInvalidBoundedSequence;
    )");
}

/// @test
/// Checks that classes cannot be used as the type of arrays
/// @requirements(SEN-903)
TEST(Resolver, invalidArray)
{
  expectInvalidResolution(R"(
package my.test;
class SomeClass {}
array<SomeClass, 12> MyInvalidArray;
    )");
}

/// @test
/// Checks that classes cannot be used as the type of optionals
/// @requirements(SEN-903)
TEST(Resolver, invalidOptional)
{
  expectInvalidResolution(R"(
package my.test;
class SomeClass {}
optional<SomeClass> MyInvalidOptional;
    )");
}

/// @test
/// Checks that simple classes can be resolved
/// @requirements(SEN-903)
TEST(Resolver, validResolution)
{
  sen::lang::ResolverContext localContext;
  sen::lang::TypeSetContext globalContext;

  auto typeSet = expectValidResolution(R"(
package my.test;
class SomeClass {}
    )",
                                       localContext,
                                       globalContext);
  EXPECT_NE(nullptr, typeSet);

  ASSERT_EQ(1, typeSet->types.size());

  EXPECT_EQ("my.test.SomeClass", typeSet->types.front()->asClassType()->getQualifiedName());
}

/// @test
/// Checks that we can decorate a method as "deferred" using settings.
/// @requirements(SEN-903)
TEST(Resolver, deferredMethod)
{

  sen::lang::TypeSettings settings;
  {
    sen::lang::ClassAnnotations annotations;
    annotations.deferredMethods.insert("someFunc");
    settings.classAnnotations.emplace("my.test.SomeClass", std::move(annotations));
  }

  sen::lang::ResolverContext localContext;
  sen::lang::TypeSetContext globalContext;

  auto typeSet = expectValidResolution(R"(
package my.test;
class SomeClass
{
  fn someFunc();
}

    )",
                                       localContext,
                                       globalContext,
                                       settings);
  EXPECT_NE(nullptr, typeSet);

  ASSERT_EQ(1, typeSet->types.size());

  auto func = typeSet->types.front()->asClassType()->searchMethodByName("someFunc");
  EXPECT_NE(nullptr, func);

  EXPECT_TRUE(func->getDeferred());
}

/// @test
/// Checks that we can decorate a property as "checked" using settings.
/// @requirements(SEN-903)
TEST(Resolver, checkedProperty)
{

  sen::lang::TypeSettings settings;
  {
    sen::lang::ClassAnnotations annotations;
    annotations.checkedProperties.insert("someProp");
    settings.classAnnotations.emplace("my.test.SomeClass", std::move(annotations));
  }

  sen::lang::ResolverContext localContext;
  sen::lang::TypeSetContext globalContext;

  auto typeSet = expectValidResolution(R"(
package my.test;
class SomeClass
{
  var someProp : u32;
}

    )",
                                       localContext,
                                       globalContext,
                                       settings);
  EXPECT_NE(nullptr, typeSet);

  ASSERT_EQ(1, typeSet->types.size());

  auto prop = typeSet->types.front()->asClassType()->searchPropertyByName("someProp");
  EXPECT_NE(nullptr, prop);

  EXPECT_TRUE(prop->getCheckedSet());
}

TEST(Resolver, sequenceOfNonValueTypes)
{

  sen::lang::TypeSetContext globalContext;

  {
    std::string program = R"(
package main;

class Test{}
)";

    sen::lang::StlScanner scanner(program);
    auto tokens = scanner.scanTokens();
    EXPECT_FALSE(tokens.empty());

    sen::lang::StlParser parser(tokens);
    std::vector<sen::lang::StlStatement> statements;
    EXPECT_NO_THROW(statements = parser.parse());

    sen::lang::ResolverContext localContext {"test", "test"};
    sen::lang::StlResolver resolver(statements, localContext, globalContext);
    std::ignore = resolver.resolve({});
  }

  {
    sen::lang::ResolverContext localContext;
    std::string program = R"(
import "test"

package main;

sequence<main.Test> Tests;
)";

    sen::lang::StlScanner scanner(program);
    auto tokens = scanner.scanTokens();
    EXPECT_FALSE(tokens.empty());

    sen::lang::StlParser parser(tokens);
    std::vector<sen::lang::StlStatement> statements;
    EXPECT_NO_THROW(statements = parser.parse());

    sen::lang::StlResolver resolver(statements, localContext, globalContext);
    EXPECT_ANY_THROW(std::ignore = resolver.resolve({}));
  }
}
