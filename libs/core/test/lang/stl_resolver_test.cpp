// === stl_resolver_test.cpp ===========================================================================================
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
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace
{

class AStlResolver: public ::testing::Test
{
protected:
  auto createResolver(const std::string& stlFile) -> sen::lang::StlResolver
  {
    parseStlStatements(stlFile);
    return {stlStatements_, resolverContext_, globalContext_};
  }

  auto createResolver(const std::string& stlFile, sen::lang::ResolverContext resolverContext) -> sen::lang::StlResolver
  {
    resolverContext_ = std::move(resolverContext);
    parseStlStatements(stlFile);
    return {stlStatements_, resolverContext_, globalContext_};
  }

  auto createResolver(const std::string& stlFile,
                      sen::lang::ResolverContext resolverContext,
                      sen::lang::TypeSetContext globalContext) -> sen::lang::StlResolver
  {
    resolverContext_ = std::move(resolverContext);
    globalContext_ = std::move(globalContext);
    parseStlStatements(stlFile);
    return {stlStatements_, resolverContext_, globalContext_};
  }

  auto resolve(const std::string& stlFile) -> const sen::lang::TypeSet*
  {
    auto stlResolver {createResolver(stlFile, sen::lang::ResolverContext {}, sen::lang::TypeSetContext {})};
    return stlResolver.resolve({});
  }

  auto resolve(const std::filesystem::path& stlFile) -> const sen::lang::TypeSet*
  {
    return sen::lang::readTypesFile(
      stlFile, {"bin"}, globalContext_, {}, std::filesystem::current_path().string() + "/bin");
  }

private:
  void parseStlStatements(const std::string& program)
  {
    sen::lang::StlScanner scanner {program};
    const auto tokens {scanner.scanTokens()};
    EXPECT_FALSE(tokens.empty());

    sen::lang::StlParser parser {tokens};
    EXPECT_NO_THROW(stlStatements_ = parser.parse());
  }

private:
  std::vector<sen::lang::StlStatement> stlStatements_;
  sen::lang::ResolverContext resolverContext_;
  sen::lang::TypeSetContext globalContext_;
};

TEST_F(AStlResolver, CorrectlyResolvesAValidComplexStlFile)
{
  // arrange
  std::filesystem::path stlFileWithComplexDeclarations {"lang/stl/stl_resolver_test/complex_stl_file.stl"};

  // act + assert
  const sen::lang::TypeSet* typeSet {};
  ASSERT_NO_THROW(typeSet = resolve(stlFileWithComplexDeclarations));
  ASSERT_NE(typeSet, nullptr);
  ASSERT_EQ(typeSet->types.size(), 11);
  ASSERT_TRUE(typeSet->types[0]->isSequenceType());
  ASSERT_TRUE(typeSet->types[1]->isSequenceType());
  ASSERT_TRUE(typeSet->types[2]->isSequenceType());
  ASSERT_TRUE(typeSet->types[3]->isEnumType());
  ASSERT_TRUE(typeSet->types[4]->isStructType());
  ASSERT_TRUE(typeSet->types[5]->isOptionalType());
  ASSERT_TRUE(typeSet->types[6]->isVariantType());
  ASSERT_TRUE(typeSet->types[7]->isQuantityType());
  ASSERT_TRUE(typeSet->types[8]->isAliasType());
  ASSERT_TRUE(typeSet->types[9]->isClassType());
  ASSERT_TRUE(typeSet->types[10]->isClassType());
}

TEST_F(AStlResolver, CorrectlyResolvesPropertiesDecoratedAsChecked)
{
  // arrange
  const std::string stlFileWithCheckedProperty {R"(
package test.classes;

class ClassUnderTest
{
  var checkedProperty : u32;
}

)"};
  const sen::lang::TypeSettings typeSettings {
    {{"test.classes.ClassUnderTest", sen::lang::ClassAnnotations {{"checkedProperty"}, {}}}}};
  auto stlResolver {createResolver(stlFileWithCheckedProperty)};

  // act
  const sen::lang::TypeSet* typeSet {};
  ASSERT_NO_THROW(typeSet = stlResolver.resolve(typeSettings));

  // assert (there must be one class type with a property decorated as checked)
  ASSERT_NE(typeSet, nullptr);
  ASSERT_EQ(typeSet->types.size(), 1);
  ASSERT_TRUE(typeSet->types.front()->isClassType());
  const auto property {typeSet->types.front()->asClassType()->searchPropertyByName("checkedProperty")};
  ASSERT_NE(property, nullptr);
  ASSERT_TRUE(property->getCheckedSet());
}

TEST_F(AStlResolver, CorrectlyResolvesMethodsDecoratedAsDeferred)
{
  // arrange
  const std::string stlFileWithDeferredMethod {R"(
package test.classes;

class ClassUnderTest
{
  fn deferredMethod() [bestEffort];
}

)"};
  const sen::lang::TypeSettings typeSettings {
    {{"test.classes.ClassUnderTest", sen::lang::ClassAnnotations {{}, {"deferredMethod"}}}}};
  auto stlResolver {createResolver(stlFileWithDeferredMethod)};

  // act
  const sen::lang::TypeSet* typeSet {};
  ASSERT_NO_THROW(typeSet = stlResolver.resolve(typeSettings));

  // assert (there must be one class type with a method decorated as deferred)
  ASSERT_NE(typeSet, nullptr);
  ASSERT_EQ(typeSet->types.size(), 1);
  ASSERT_TRUE(typeSet->types.front()->isClassType());
  const auto method {typeSet->types.front()->asClassType()->searchMethodByName("deferredMethod")};
  ASSERT_NE(method, nullptr);
  ASSERT_TRUE(method->getDeferred());
}

TEST_F(AStlResolver, CorrectlyResolvesTypesIncludedFromOtherStlFiles)
{
  // arrange
  const std::string stlFileWithTypeDefinition {R"(
package test.first_package;

struct TestStruct
{
  flags : u32,
  color : u32,
  text  : string
}

)"};
  const std::string anotherStlFileUsingTheTypeDefinitionFromAnotherFile {R"(
import "test.stl"

package test.another_package;

struct AnotherTestStruct
{
  flags : u32,
  color : u32,
  text  : string,
  type  : test.first_package.TestStruct
}

)"};

  // act
  // we first need to resolve the .stl file that is going to be included to populate the global context of resolution
  const sen::lang::ResolverContext localResolverContext {"test.stl", "test.stl", {}};
  auto stlResolverFirstFile {createResolver(stlFileWithTypeDefinition, localResolverContext)};
  ASSERT_NO_THROW([[maybe_unused]] auto typeSet = stlResolverFirstFile.resolve({}));
  // afterward we resolve the .stl that includes the first one
  auto stlResolverSecondFile {createResolver(anotherStlFileUsingTheTypeDefinitionFromAnotherFile)};
  const sen::lang::TypeSet* typeSet {};
  ASSERT_NO_THROW(typeSet = stlResolverSecondFile.resolve({}));

  // assert
  ASSERT_NE(typeSet, nullptr);
  ASSERT_EQ(typeSet->types.size(), 1);
  ASSERT_TRUE(typeSet->types.front()->isStructType());
  const auto fieldData {typeSet->types.front()->asStructType()->getFieldFromName("type")};
  ASSERT_NE(fieldData, nullptr);
  ASSERT_EQ(fieldData->name, "type");
}

TEST_F(AStlResolver, ThrowsIfClassIsUsedAsPropertyTypes)
{
  // arrange
  const std::string stlFileWithClassAsPropertyType {R"(
package test.classes;

class SomeClass {}
class ClassUnderTest
{
  var invalidProperty : SomeClass;
}

)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithClassAsPropertyType)});
}

TEST_F(AStlResolver, ThrowsIfClassIsUsedAsFunctionReturnType)
{
  // arrange
  const std::string stlFileWithClassAsFunctionReturnType {R"(
package test.classes;

class SomeClass {}
class ClassUnderTest
{
  fn invalidFunction() -> SomeClass;
}

)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithClassAsFunctionReturnType)});
}

TEST_F(AStlResolver, ThrowsIfClassIsUsedAsFunctionArgument)
{
  // arrange
  const std::string stlFileWithClassAsFunctionArgument {R"(
package test.classes;

class SomeClass {}
class ClassUnderTest
{
  fn invalidFunction(arg: SomeClass) -> bool;
}

)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithClassAsFunctionArgument)});
}

TEST_F(AStlResolver, ThrowsIfClassIsUsedAsEventArgument)
{
  // arrange
  const std::string stlFileWithClassAsEventArgument {R"(
package test.classes;

class SomeClass {}
class ClassUnderTest
{
  event invalidEvent(arg: SomeClass);
}

)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithClassAsEventArgument)});
}

TEST_F(AStlResolver, ThrowsIfClassIsUsedAsStructField)
{
  // arrange
  const std::string stlFileWithClassAsStructField {R"(
package test.classes;

class SomeClass {}
struct StructUnderTest
{
  x : SomeClass,
  y : u32
}

)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithClassAsStructField)});
}

TEST_F(AStlResolver, ThrowsIfClassIsUsedAsVariantType)
{
  // arrange
  const std::string stlFileWithClassAsVariantType {R"(
package test.classes;

class SomeClass {}
variant VariantUnderTest
{
  u32,
  SomeClass
}

)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithClassAsVariantType)});
}

TEST_F(AStlResolver, ThrowsIfClassIsUsedAsTypeOfSequenceOrArray)
{
  // arrange
  const std::string stlFileWithClassAsTypeOfUnboundedSequence {R"(
package test.classes;

class TestClass {}
sequence<TestClass> SequenceUnderTest;
)"};
  const std::string stlFileWithClassAsTypeOfBoundedSequence {R"(
package test.classes;

class TestClass {}
sequence<TestClass, 12> BoundedSequenceUnderTest;
)"};
  const std::string stlFileWithClassAsTypeOfArray {R"(
package test.classes;

class TestClass {}
array<TestClass, 12> ArrayUnderTest;
)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithClassAsTypeOfUnboundedSequence)});
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithClassAsTypeOfBoundedSequence)});
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithClassAsTypeOfArray)});
}

TEST_F(AStlResolver, ThrowsIfClassIsUsedAsTypeOfOptional)
{
  // arrange
  const std::string stlFileWithClassAsTypeOfOptional {R"(
package test.classes;

class TestClass {}
optional<TestClass> OptionalUnderTest;
)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithClassAsTypeOfOptional)});
}

TEST_F(AStlResolver, ThrowsIfClassIsUsedAsTypeOfQuantity)
{
  // arrange
  const std::string stlFileWithClassAsTypeOfQuantity {R"(
package test.classes;

class TestClass {}
quantity<TestClass, deg> QuantityUnderTest;
)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithClassAsTypeOfQuantity)});
}

TEST_F(AStlResolver, ThrowsIfNoPackageNameIsSpecifiedBeforeTheFirstTypeDeclaration)
{
  // arrange
  const std::string stlFileWithTypeDeclarationBeforePackageName {R"(
struct TestStruct;
package test.package_declarations;
)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithTypeDeclarationBeforePackageName)});
}

TEST_F(AStlResolver, ThrowsIfPackageNameIsRedeclared)
{
  // arrange
  const std::string stlFileWithPackageNameRedeclaration {R"(
package test.package_declarations;
package sen.package_redeclaration;
)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithPackageNameRedeclaration)});
}

TEST_F(AStlResolver, ThrowsIfParentStructIsNotDefined)
{
  // arrange
  const std::string stlFileWithUndefinedParentStruct {R"(
package test.structs;

struct StructUnderTest : UndefinedParentStruct;
)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithUndefinedParentStruct)});
}

TEST_F(AStlResolver, ThrowsIfParentStructIsNotAStructType)
{
  // arrange
  const std::string stlFileWithNonStructTypeAsParentStruct {R"(
package test.structs;

variant TestVariant
{
  u8,
  string,
  Duration
}

struct StructUnderTest : TestVariant;
)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithNonStructTypeAsParentStruct)});
}

TEST_F(AStlResolver, ThrowsIfTheStorageTypeOfAnEnumerationIsAnUnknownType)
{
  // arrange
  const std::string stlFileWithInvalidEnumeration {R"(
package test.enums;

enum EnumUnderTest: UnknownStorageType
{
  value1,
  value2,
  value3,
  value4,
  value5
}

)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithInvalidEnumeration)});
}

TEST_F(AStlResolver, ThrowsIfTheStorageTypeOfAnEnumerationIsNotNumeric)
{
  // arrange
  const std::string stlFileWithInvalidEnumeration {R"(
package test.enums;

struct TestStruct;

enum EnumUnderTest: TestStruct
{
  value1,
  value2,
  value3,
  value4,
  value5
}

)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithInvalidEnumeration)});
}

TEST_F(AStlResolver, ThrowsIfTheUnderlyingTypeOfAQuantityIsUnknown)
{
  // arrange
  const std::string stlFileWithInvalidQuantity {R"(
package test.quantities;

quantity<UnknownType, deg> QuantityUnderTest;
)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithInvalidQuantity)});
}

TEST_F(AStlResolver, ThrowsIfTheUnderlyingTypeOfAQuantityIsNotNumeric)
{
  // arrange
  const std::string stlFileWithInvalidQuantity {R"(
package test.quantities;

quantity<string, deg> QuantityUnderTest;
)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithInvalidQuantity)});
}

TEST_F(AStlResolver, ThrowsIfClassPropertyTransportAttributeValueIsRepeated)
{
  // arrange
  const std::string stlFileWithRepeatedConfirmed {R"(
package test.classes;

class ClassUnderTest
{
  var property : string [writable, confirmed, confirmed];
}

)"};
  const std::string stlFileWithRepeatedBestEffort {R"(
package test.classes;

class ClassUnderTest
{
  var property : u8 [writable, bestEffort, bestEffort];
}

)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithRepeatedConfirmed)});
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithRepeatedBestEffort)});
}

TEST_F(AStlResolver, ThrowsIfClassPropertyAttributeIsInvalid)
{
  // arrange
  const std::string stlFileWithInvalidSingleWordAttribute {R"(
package test.classes;

class ClassUnderTest
{
  var property : u32 [invalidAttribute, bestEffort];
}

)"};
  const std::string stlFileWithInvalidMultiWordAttribute {R"(
package test.classes;

class ClassUnderTest
{
  var property : u32 [invalidTag: invalid, confirmed];
}

)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithInvalidSingleWordAttribute)});
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithInvalidMultiWordAttribute)});
}

TEST_F(AStlResolver, ThrowsIfClassPropertyNameDoesNotComplyWithLowerCamelCase)
{
  // arrange
  const std::string stlFileWithInvalidClassPropertyName {R"(
package test.classes;

class TestClass
{
  var Invalid_Member_Name : string [writable, confirmed];
}

)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithInvalidClassPropertyName)});
}

TEST_F(AStlResolver, EmittsAWarningIfClassPropertyIsDecoratedWithCheckedAttribute)
{
  // arrange
  const std::string stlFileWithAClassPropertyMarkedAsChecked {R"(
package test.classes;

class ClassUnderTest
{
  var checkedProperty : string [writable, checked, confirmed];
}

)"};

  // act + assert
  auto stlResolver {createResolver(stlFileWithAClassPropertyMarkedAsChecked)};
  const sen::lang::TypeSet* typeSet {};
  ASSERT_NO_THROW(typeSet = stlResolver.resolve({}));
  ASSERT_NE(typeSet, nullptr);
  ASSERT_EQ(typeSet->types.size(), 1);
  ASSERT_TRUE(typeSet->types.front()->isClassType());
  const auto property {typeSet->types.front()->asClassType()->searchPropertyByName("checkedProperty")};
  ASSERT_NE(property, nullptr);
  ASSERT_TRUE(property->getCheckedSet());
}

TEST_F(AStlResolver, EmittsAWarningIfClassMethodIsDecoratedWithDeferredAttribute)
{
  // arrange
  const std::string stlFileWithClassMethodMarkedAsDeferredInStlFile {R"(
package test.classes;

class ClassUnderTest
{
  fn deferredMethod() -> u64 [deferred, confirmed];
}

)"};

  // act + assert
  auto stlResolver {createResolver(stlFileWithClassMethodMarkedAsDeferredInStlFile)};
  const sen::lang::TypeSet* typeSet {};
  ASSERT_NO_THROW(typeSet = stlResolver.resolve({}));
  ASSERT_NE(typeSet, nullptr);
  ASSERT_EQ(typeSet->types.size(), 1);
  ASSERT_TRUE(typeSet->types.front()->isClassType());
  const auto method {typeSet->types.front()->asClassType()->searchMethodByName("deferredMethod")};
  ASSERT_NE(method, nullptr);
  ASSERT_TRUE(method->getDeferred());
}

TEST_F(AStlResolver, ThrowsIfClassTypeTriesToExtendParentInterface)
{
  // arrange
  const std::string stlFileWithClassThatTriesToExtendParentInterface {R"(
package test.classes;

interface TestInterface
{
  fn someMethod(lhs: i32, rhs: i32) -> i32;
}

class ClassUnderTest : extends SomeInterface
{
}

)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithClassThatTriesToExtendParentInterface)});
}

TEST_F(AStlResolver, ThrowsIfClassTypeIsUsedAsParentInterface)
{
  // arrange
  const std::string stlFileWithClassTypeUsedAsInterface {R"(
package test.classes;

class TestClass
{
  fn someMethod(lhs: i32, rhs: i32) -> i32;
}

class ClassUnderTest : implements TestClass
{
}

)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithClassTypeUsedAsInterface)});
}

TEST_F(AStlResolver, ThrowsIfValueTypeIsUsedAsParentInterface)
{
  // arrange
  const std::string stlFileWithValueTypeAsInterface {R"(
package test.classes;

struct SomeStruct;

class ClassUnderTest : implements SomeStruct
{
}

)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithValueTypeAsInterface)});
}

TEST_F(AStlResolver, ThrowsIfClassMethodAttributeIsInvalid)
{
  // arrange
  const std::string stlFileWithInvalidSingleWordAttribute {R"(
package test.classes;

class ClassUnderTest
{
  fn methodWithInvalidAttribute() -> string [invalidAttribute, confirmed];
}

)"};
  const std::string stlFileWithInvalidMultiWordAttribute {R"(
package test.classes;

class ClassUnderTest
{
  fn methodWithInvalidAttribute() -> i32 [tag: invalidTag, bestEffort];
}

)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithInvalidSingleWordAttribute)});
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithInvalidMultiWordAttribute)});
}

TEST_F(AStlResolver, ThrowsIfClassEventTransportPropertyIsInvalid)
{
  // arrange
  const std::string stlFileWithInvalidSingleWordAttribute {R"(
package test.classes;

class ClassUnderTest
{
  event eventWithInvalidAttribute() [invalidAttribute];
}

)"};
  const std::string stlFileWithInvalidMultiWordAttribute {R"(
package test.classes;

class ClassUnderTest
{
  event eventWithInvalidAttribute() [tag: invalidTag];
}

)"};

  // act + assert
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithInvalidSingleWordAttribute)});
  ASSERT_ANY_THROW([[maybe_unused]] const auto typeSet {resolve(stlFileWithInvalidMultiWordAttribute)});
}

TEST_F(AStlResolver, ThrowsIfUnboundedDynamicPropertySpecifiesNonConfirmedTransportMode)
{
  // arrange
  const std::string stlFileWithUnboundedDynamicPropertyWithNonConfirmedTransportMode {R"(
package test.classes;

class ClassUnderTest
{
  var property : string [writable, bestEffort];
}

)"};
  const std::string stlFileWithUnboundedDynamicPropertyWithNoTransportMode {R"(
package test.classes;

class ClassUnderTest
{
  var property : string [writable];
}

)"};

  // act + assert
  ASSERT_ANY_THROW(
    [[maybe_unused]] const auto typeSet {resolve(stlFileWithUnboundedDynamicPropertyWithNonConfirmedTransportMode)});
  ASSERT_ANY_THROW(
    [[maybe_unused]] const auto typeSet {resolve(stlFileWithUnboundedDynamicPropertyWithNoTransportMode)});
}

TEST_F(AStlResolver, ThrowsIfCustomTypeDeclarationIsRepeated)
{
  // arrange
  const std::string stlFileWithRepeatedStructDeclaration {R"(
package test.structs;

struct StructUnderTest;
struct StructUnderTest;
)"};
  const std::string stlFileWithRepeatedEnumerationDeclaration {R"(
package test.enums;

enum EnumUnderTest: u8
{
  value
}

enum EnumUnderTest: u8
{
  value
}

)"};
  const std::string stlFileWithRepeatedOptionalDeclaration {R"(
package test.optionals;

optional<string> OptionalUnderTest;
optional<string> OptionalUnderTest;
)"};
  const std::string stlFileWithRepeatedClassDeclaration {R"(
package test.classes;

class ClassUnderTest
{
}

class ClassUnderTest
{
}

)"};
  const std::string stlFileWithRepeatedVariantDeclaration {R"(
package test.variants;

variant VariantUnderTest
{
  u8,
  string
}

variant VariantUnderTest
{
  u8,
  string
}

)"};
  const std::string stlFileWithRepeatedSequenceDeclaration {R"(
package test.sequences;

sequence<f32> UnboundedSequenceUnderTest;
sequence<f32> UnboundedSequenceUnderTest;
)"};
  const std::string stlFileWithRepeatedArrayDeclaration {R"(
package test.arrays;

array<f32, 50> ArrayUnderTest;
array<f32, 50> ArrayUnderTest;
)"};
  const std::string stlFileWithRepeatedQuantityDeclaration {R"(
package test.quantities;

quantity<f32, m_per_s> QuantityUnderTest;
quantity<f32, m_per_s> QuantityUnderTest;
)"};
  const std::string stlFileWithRepeatedAliasDeclaration {R"(
package test.aliases;

alias AliasUnderTest string;
alias AliasUnderTest string;
)"};
  const std::string stlFileWithRepeatedInterfaceDeclaration {R"(
package test.interfaces;

interface InterfaceUnderTest
{
}

interface InterfaceUnderTest
{
}

)"};

  // act + assert
  ASSERT_ANY_THROW(resolve(stlFileWithRepeatedStructDeclaration));
  ASSERT_ANY_THROW(resolve(stlFileWithRepeatedEnumerationDeclaration));
  ASSERT_ANY_THROW(resolve(stlFileWithRepeatedOptionalDeclaration));
  ASSERT_ANY_THROW(resolve(stlFileWithRepeatedClassDeclaration));
  ASSERT_ANY_THROW(resolve(stlFileWithRepeatedVariantDeclaration));
  ASSERT_ANY_THROW(resolve(stlFileWithRepeatedSequenceDeclaration));
  ASSERT_ANY_THROW(resolve(stlFileWithRepeatedArrayDeclaration));
  ASSERT_ANY_THROW(resolve(stlFileWithRepeatedQuantityDeclaration));
  ASSERT_ANY_THROW(resolve(stlFileWithRepeatedAliasDeclaration));
  ASSERT_ANY_THROW(resolve(stlFileWithRepeatedInterfaceDeclaration));
}

TEST_F(AStlResolver, ThrowsIfAVariantHoldsTheSameTypeMoreThanOnce)
{
  // arrange
  const std::string stlFileWithInvalidVariantDeclaration {R"(
package test.variants;

struct TestStruct;

variant VariantUnderTest
{
  u8,
  string,
  TestStruct,
  TestStruct
}

)"};

  // act + assert
  ASSERT_ANY_THROW(resolve(stlFileWithInvalidVariantDeclaration));
}

}  // namespace
