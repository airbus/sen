#include "mkdocs_fixture_test.h"

// 3rd party
#include <gtest/gtest.h>

// std
#include <string>

constexpr auto empty = R"()";

constexpr auto errorFormat = R"(;)";

constexpr auto package = R"(package my.test;)";

constexpr auto emptyClass = R"(package my_test_package;

class MyTestClass
{}
)";

constexpr auto basicProperties = R"(package my_test_package;

class MyTestClass
{
  var prop1 : string       [static];
  var prop2 : i32          [writable];
}
)";

constexpr auto structProperty = R"(package my_test_package;

struct MyTestStruct
{
  x : f32,
  y : f64
}

class MyTestClass
{
  var struct1 : MyTestStruct          [writable];
}
)";

constexpr auto enumProperty = R"(package my_test_package;

enum MyTestEnum: u8
{
  first,
  second,
  third
}

class MyTestClass
{
  var enum1 : MyTestEnum          [writable];
}
)";

constexpr auto variantProperty = R"(package my_test_package;

variant MyTestVariant
{
  i32,
  bool,
  string
}

class MyTestClass
{
  var variant1 : MyTestVariant          [confirmed, writable];
}
)";

constexpr auto sequenceProperty = R"(package my_test_package;

sequence<i64, 100> MyTestBoundedSequence;
sequence<i32> MyTestUnboundedSequence;

class MyTestClass
{
  var sequence1 : MyTestBoundedSequence          [writable];
  var sequence2 : MyTestUnboundedSequence            [writable, confirmed];
}
)";

constexpr auto aliasProperty = R"(package my_test_package;

alias MyTestAlias i64;

class MyTestClass
{
  var alias1 : MyTestAlias          [writable];
}
)";

constexpr auto optionalProperty = R"(package my_test_package;

optional<f32> MyTestOptional;

class MyTestClass
{
  var optional1 : MyTestOptional          [writable];
}
)";

constexpr auto quantityProperty = R"(package my_test_package;

quantity<f32, m> MyTestQuantity;

class MyTestClass
{
  var quantity1 : MyTestQuantity          [writable];
}
)";

constexpr auto comment = R"(package my_test_package;

quantity<f32, m> MyTestQuantity; //This is my test quantity type
)";

constexpr auto methods = R"(package my_test_package;

class MyTestClass
{
  var result : i32          [writable];

  fn addNumbers(a: i32, b: i32) -> i32;
  fn subtractNumbers(a: i32, b: i32) -> i32;
}
)";

constexpr auto events = R"(package my_test_package;

class MyTestClass
{
  var result : i32          [writable];

  // Emitted when there's an attempt to divide by zero.
  event divisionByZero();
}
)";

constexpr auto inheritance = R"(package my_test_package;

class MyBaseTestClass
{
  var baseProp : string [confirmed];
}

class MyDerivedTestClass : extends MyBaseTestClass
{
  var derivedProp: f64 [writable];
}
)";

constexpr auto nodeTree = R"(package my_test_package;

class MyBaseTestClass
{
}

class MyDerivedTestClass : extends MyBaseTestClass
{
}

class AnotherBaseTestClass
{
}
)";

constexpr auto structList = R"(package my_test_package;

struct MyTestStruct
{
}

struct MyTestStruct2
{
}

struct MyTestStruct3
{
}

struct MyTestStruct4
{
}

struct MyTestStruct5
{
}
)";

/// @test
/// Check sen gen mkdocs generates a correct documentation file from an empty STL file
TEST_F(AMkdocsGenerator, StlEmpty)
{
  generateStl(empty);

  EXPECT_FALSE(content().empty()) << content();
}

/// @test
/// Check sen gen mkdocs does not generate a documentation file from an invalid STL file
TEST_F(AMkdocsGenerator, StlErrorFormat) { ASSERT_ANY_THROW(generateStl(errorFormat)); }

/// @test
/// Check sen gen mkdocs generates a correct documentation file with a package definition
TEST_F(AMkdocsGenerator, StlPackage)
{
  generateStl(package);

  EXPECT_NE(find("\"my.test\""), std::string::npos) << content();
}

/// @test
/// Check sen gen mkdocs generates a correct documentation file from an STL file with an empty class
TEST_F(AMkdocsGenerator, StlEmptyClass)
{
  generateStl(emptyClass);

  const auto nodeTreeClassPos = find("[my_test_package.MyTestClass](#my_test_packagemytestclass)");
  ASSERT_NE(nodeTreeClassPos, std::string::npos) << content();

  EXPECT_NE(find("### my_test_package.MyTestClass", nodeTreeClassPos), std::string::npos) << content();
}

/// @test
/// Check sen gen mkdocs generates a correct documentation file with basic properties in the class
TEST_F(AMkdocsGenerator, StlBasicProperties)
{
  generateStl(basicProperties);

  const auto nodeTreeClassPos = find("[my_test_package.MyTestClass](#my_test_packagemytestclass)");
  ASSERT_NE(nodeTreeClassPos, std::string::npos) << content();

  const auto classDefinitionPos = find("### my_test_package.MyTestClass", nodeTreeClassPos);
  ASSERT_NE(classDefinitionPos, std::string::npos) << content();

  const auto propHeaderPos = find("**Properties**", classDefinitionPos);
  ASSERT_NE(propHeaderPos, std::string::npos) << content();

  const auto prop1DeclarationPos = find("**prop1**", propHeaderPos);
  ASSERT_NE(prop1DeclarationPos, std::string::npos) << content();

  const auto prop1QualifierPos = find("{ title=\"Static\" }", prop1DeclarationPos);
  ASSERT_NE(prop1QualifierPos, std::string::npos) << content();

  const auto prop1TypePos = find("[string](#string)", prop1QualifierPos);
  ASSERT_NE(prop1TypePos, std::string::npos) << content();

  const auto prop2DeclarationPos = find("**prop2**", classDefinitionPos);
  ASSERT_NE(prop2DeclarationPos, std::string::npos) << content();

  const auto prop2QualifierPos = find("{ title=\"Writable\" }", prop2DeclarationPos);
  ASSERT_NE(prop2QualifierPos, std::string::npos) << content();

  EXPECT_NE(find("[i32](#i32)", prop2QualifierPos), std::string::npos) << content();
}

/// @test
/// Check sen gen mkdocs generates a correct documentation file from STL file with a struct property in the class
TEST_F(AMkdocsGenerator, StlStructProperty)
{
  generateStl(structProperty);

  const auto nodeTreeClassPos = find("[my_test_package.MyTestClass](#my_test_packagemytestclass)");
  ASSERT_NE(nodeTreeClassPos, std::string::npos) << content();

  const auto nodeTreeStructPos = find("[MyTestStruct](#my_test_packagemyteststruct)", nodeTreeClassPos);
  ASSERT_NE(nodeTreeStructPos, std::string::npos) << content();

  const auto classDefinitionPos = find("### my_test_package.MyTestClass", nodeTreeStructPos);
  ASSERT_NE(classDefinitionPos, std::string::npos) << content();

  const auto propHeaderPos = find("**Properties**", classDefinitionPos);
  ASSERT_NE(propHeaderPos, std::string::npos) << content();

  const auto struct1DeclarationPos = find("**struct1**", propHeaderPos);
  ASSERT_NE(struct1DeclarationPos, std::string::npos) << content();

  const auto struct1QualifierPos = find("{ title=\"Writable\" }", struct1DeclarationPos);
  ASSERT_NE(struct1QualifierPos, std::string::npos) << content();

  const auto struct1TypePos = find("[my_test_package.MyTestStruct](#my_test_packagemyteststruct)", struct1QualifierPos);
  ASSERT_NE(struct1TypePos, std::string::npos) << content();

  const auto structDefinitionPos = find("### my_test_package.MyTestStruct", struct1TypePos);
  ASSERT_NE(structDefinitionPos, std::string::npos) << content();

  const auto xPropPos = find("**x** | [f32](#f32)", structDefinitionPos);
  ASSERT_NE(xPropPos, std::string::npos) << content();

  EXPECT_NE(find("**y** | [f64](#f64)", xPropPos), std::string::npos) << content();
}

/// @test
/// Check sen gen mkdocs generates a correct documentation file from STL file with an enum property in the class
TEST_F(AMkdocsGenerator, StlEnumProperty)
{
  generateStl(enumProperty);

  const auto nodeTreeClassPos = find("[my_test_package.MyTestClass](#my_test_packagemytestclass)");
  ASSERT_NE(nodeTreeClassPos, std::string::npos) << content();

  const auto nodeTreeEnumPos = find("[MyTestEnum](#my_test_packagemytestenum)", nodeTreeClassPos);
  ASSERT_NE(nodeTreeEnumPos, std::string::npos) << content();

  const auto classDefinitionPos = find("### my_test_package.MyTestClass", nodeTreeEnumPos);
  ASSERT_NE(classDefinitionPos, std::string::npos) << content();

  const auto propHeaderPos = find("**Properties**", classDefinitionPos);
  ASSERT_NE(propHeaderPos, std::string::npos) << content();

  const auto enum1DeclarationPos = find("**enum1**", propHeaderPos);
  ASSERT_NE(enum1DeclarationPos, std::string::npos) << content();

  const auto enum1QualifierPos = find("{ title=\"Writable\" }", enum1DeclarationPos);
  ASSERT_NE(enum1QualifierPos, std::string::npos) << content();

  const auto enum1TypePos = find("[my_test_package.MyTestEnum](#my_test_packagemytestenum)", enum1QualifierPos);
  ASSERT_NE(enum1TypePos, std::string::npos) << content();

  const auto enumDefinitionPos = find("### my_test_package.MyTestEnum", enum1TypePos);
  ASSERT_NE(enumDefinitionPos, std::string::npos) << content();

  const auto enumRepresentationPos = find("Representation: [u8](#u8)", enumDefinitionPos);
  ASSERT_NE(enumRepresentationPos, std::string::npos) << content();

  const auto firstPropPos = find("first | 0 |", enumRepresentationPos);
  ASSERT_NE(firstPropPos, std::string::npos) << content();

  const auto secondPropPos = find("second | 1 |", firstPropPos);
  ASSERT_NE(secondPropPos, std::string::npos) << content();

  EXPECT_NE(find("third | 2 |", secondPropPos), std::string::npos) << content();
}

/// @test
/// Check sen gen mkdocs generates a correct documentation file from STL file with a variant property in the class
TEST_F(AMkdocsGenerator, StlVariantProperty)
{
  generateStl(variantProperty);

  const auto nodeTreeClassPos = find("[my_test_package.MyTestClass](#my_test_packagemytestclass)");
  ASSERT_NE(nodeTreeClassPos, std::string::npos) << content();

  const auto nodeTreeVariantPos = find("[MyTestVariant](#my_test_packagemytestvariant)", nodeTreeClassPos);
  ASSERT_NE(nodeTreeVariantPos, std::string::npos) << content();

  const auto classDefinitionPos = find("### my_test_package.MyTestClass", nodeTreeVariantPos);
  ASSERT_NE(classDefinitionPos, std::string::npos) << content();

  const auto propHeaderPos = find("**Properties**", classDefinitionPos);
  ASSERT_NE(propHeaderPos, std::string::npos) << content();

  const auto variant1DeclarationPos = find("**variant1**", propHeaderPos);
  ASSERT_NE(variant1DeclarationPos, std::string::npos) << content();

  const auto variant1QualifierPos = find("{ title=\"Writable\" }", variant1DeclarationPos);
  ASSERT_NE(variant1QualifierPos, std::string::npos) << content();

  const auto variant1TypePos =
    find("[my_test_package.MyTestVariant](#my_test_packagemytestvariant)", variant1QualifierPos);
  ASSERT_NE(variant1TypePos, std::string::npos) << content();

  const auto variantDefinitionPos = find("### my_test_package.MyTestVariant", variant1TypePos);
  ASSERT_NE(variantDefinitionPos, std::string::npos) << content();

  const auto firstTypePos = find("[i32](#i32)", variantDefinitionPos);
  ASSERT_NE(firstTypePos, std::string::npos) << content();

  const auto secondTypePos = find("[bool](#bool)", variantDefinitionPos);
  ASSERT_NE(secondTypePos, std::string::npos) << content();

  EXPECT_NE(find("[string](#string)", variantDefinitionPos), std::string::npos) << content();
}

/// @test
/// Check sen gen mkdocs generates a correct documentation file from STL file with a sequence property in the class
TEST_F(AMkdocsGenerator, StlSequenceProperty)
{
  generateStl(sequenceProperty);

  const auto nodeTreeClassPos = find("[my_test_package.MyTestClass](#my_test_packagemytestclass)");
  ASSERT_NE(nodeTreeClassPos, std::string::npos) << content();

  const auto nodeTreeSequencesPos = find(
    "[MyTestBoundedSequence](#my_test_packagemytestboundedsequence)| "
    "[MyTestUnboundedSequence](#my_test_packagemytestunboundedsequence)");
  ASSERT_NE(nodeTreeSequencesPos, std::string::npos) << content();

  const auto classDefinitionPos = find("### my_test_package.MyTestClass", nodeTreeSequencesPos);
  ASSERT_NE(classDefinitionPos, std::string::npos) << content();

  const auto propHeaderPos = find("**Properties**", classDefinitionPos);
  ASSERT_NE(propHeaderPos, std::string::npos) << content();

  const auto sequence1DeclarationPos = find("**sequence1**", propHeaderPos);
  ASSERT_NE(sequence1DeclarationPos, std::string::npos) << content();

  const auto sequence1QualifierPos = find("{ title=\"Writable\" }", sequence1DeclarationPos);
  ASSERT_NE(sequence1QualifierPos, std::string::npos) << content();

  const auto sequence1TypePos =
    find("[my_test_package.MyTestBoundedSequence](#my_test_packagemytestboundedsequence)", sequence1QualifierPos);
  ASSERT_NE(sequence1TypePos, std::string::npos) << content();

  const auto sequence2DeclarationPos = find("**sequence2**", sequence1TypePos);
  ASSERT_NE(sequence2DeclarationPos, std::string::npos) << content();

  const auto sequence2QualifierPos = find("{ title=\"Writable\" }", sequence2DeclarationPos);
  ASSERT_NE(sequence2QualifierPos, std::string::npos) << content();

  const auto sequence2TypePos =
    find("[my_test_package.MyTestUnboundedSequence](#my_test_packagemytestunboundedsequence)", sequence2QualifierPos);
  ASSERT_NE(sequence2TypePos, std::string::npos) << content();

  const auto boundedSequenceDefinitionPos = find("### my_test_package.MyTestBoundedSequence", sequence2TypePos);
  ASSERT_NE(boundedSequenceDefinitionPos, std::string::npos) << content();

  const auto boundedSequenceTypePos = find("Element Type | [i64](#i64)", boundedSequenceDefinitionPos);
  ASSERT_NE(boundedSequenceTypePos, std::string::npos) << content();

  const auto boundedSequenceIsBoundedPos = find("Bounded | Yes", boundedSequenceDefinitionPos);
  ASSERT_NE(boundedSequenceIsBoundedPos, std::string::npos) << content();

  const auto boundedSequenceMaxSizePos = find("Max size | 100", boundedSequenceDefinitionPos);
  ASSERT_NE(boundedSequenceMaxSizePos, std::string::npos) << content();

  const auto unboundedSequenceDefinitionPos =
    find("### my_test_package.MyTestUnboundedSequence", boundedSequenceMaxSizePos);
  ASSERT_NE(unboundedSequenceDefinitionPos, std::string::npos) << content();

  const auto unboundedSequenceTypePos = find("Element Type | [i32](#i32)", unboundedSequenceDefinitionPos);
  ASSERT_NE(unboundedSequenceTypePos, std::string::npos) << content();

  EXPECT_NE(find("Bounded | No", unboundedSequenceDefinitionPos), std::string::npos) << content();
}

/// @test
/// Check sen gen mkdocs generates a correct documentation file from STL file with an alias property in the class
TEST_F(AMkdocsGenerator, StlAliasProperty)
{
  generateStl(aliasProperty);

  const auto nodeTreeClassPos = find("[my_test_package.MyTestClass](#my_test_packagemytestclass)");
  ASSERT_NE(nodeTreeClassPos, std::string::npos) << content();

  const auto nodeTreeAliasPos = find("[MyTestAlias](#my_test_packagemytestalias)", nodeTreeClassPos);
  ASSERT_NE(nodeTreeAliasPos, std::string::npos) << content();

  const auto classDefinitionPos = find("### my_test_package.MyTestClass", nodeTreeAliasPos);
  ASSERT_NE(classDefinitionPos, std::string::npos) << content();

  const auto propHeaderPos = find("**Properties**", classDefinitionPos);
  ASSERT_NE(propHeaderPos, std::string::npos) << content();

  const auto alias1DeclarationPos = find("**alias1**", propHeaderPos);
  ASSERT_NE(alias1DeclarationPos, std::string::npos) << content();

  const auto alias1QualifierPos = find("{ title=\"Writable\" }", alias1DeclarationPos);
  ASSERT_NE(alias1QualifierPos, std::string::npos) << content();

  const auto alias1TypePos = find("[my_test_package.MyTestAlias](#my_test_packagemytestalias)", alias1QualifierPos);
  ASSERT_NE(alias1TypePos, std::string::npos) << content();

  const auto aliasDefinitionPos = find("### my_test_package.MyTestAlias", alias1TypePos);
  ASSERT_NE(aliasDefinitionPos, std::string::npos) << content();

  EXPECT_NE(find("Aliased Type: [i64](#i64)", aliasDefinitionPos), std::string::npos) << content();
}

/// @test
/// Check sen gen mkdocs generates a correct documentation file from STL file with an optional property in the class
TEST_F(AMkdocsGenerator, StlOptionalProperty)
{
  generateStl(optionalProperty);

  const auto nodeTreeClassPos = find("[my_test_package.MyTestClass](#my_test_packagemytestclass)");
  ASSERT_NE(nodeTreeClassPos, std::string::npos) << content();

  const auto classDefinitionPos = find("### my_test_package.MyTestClass", nodeTreeClassPos);
  ASSERT_NE(classDefinitionPos, std::string::npos) << content();

  const auto propHeaderPos = find("**Properties**", classDefinitionPos);
  ASSERT_NE(propHeaderPos, std::string::npos) << content();

  const auto optional1DeclarationPos = find("**optional1**", propHeaderPos);
  ASSERT_NE(optional1DeclarationPos, std::string::npos) << content();

  const auto optional1QualifierPos = find("{ title=\"Writable\" }", optional1DeclarationPos);
  ASSERT_NE(optional1QualifierPos, std::string::npos) << content();

  const auto optional1TypePos =
    find("[my_test_package.MyTestOptional](#my_test_packagemytestoptional)", optional1QualifierPos);
  ASSERT_NE(optional1TypePos, std::string::npos) << content();

  const auto optionalDefinitionPos = find("### my_test_package.MyTestOptional", optional1TypePos);
  ASSERT_NE(optionalDefinitionPos, std::string::npos) << content();

  EXPECT_NE(find("Optional Type: [f32](#f32)", optionalDefinitionPos), std::string::npos) << content();
}

/// @test
/// Check sen gen mkdocs generates a correct documentation file from STL file with a quantity property in the class
TEST_F(AMkdocsGenerator, StlQuantityProperty)
{
  generateStl(quantityProperty);

  const auto nodeTreeClassPos = find("[my_test_package.MyTestClass](#my_test_packagemytestclass)");
  ASSERT_NE(nodeTreeClassPos, std::string::npos) << content();

  const auto nodeTreeQuantityPos = find("[MyTestQuantity](#my_test_packagemytestquantity)", nodeTreeClassPos);
  ASSERT_NE(nodeTreeQuantityPos, std::string::npos) << content();

  const auto classDefinitionPos = find("### my_test_package.MyTestClass", nodeTreeQuantityPos);
  ASSERT_NE(classDefinitionPos, std::string::npos) << content();

  const auto propHeaderPos = find("**Properties**", classDefinitionPos);
  ASSERT_NE(propHeaderPos, std::string::npos) << content();

  const auto quantity1DeclarationPos = find("**quantity1**", propHeaderPos);
  ASSERT_NE(quantity1DeclarationPos, std::string::npos) << content();

  const auto quantity1QualifierPos = find("{ title=\"Writable\" }", quantity1DeclarationPos);
  ASSERT_NE(quantity1QualifierPos, std::string::npos) << content();

  const auto quantity1TypePos =
    find("[my_test_package.MyTestQuantity](#my_test_packagemytestquantity)", quantity1QualifierPos);
  ASSERT_NE(quantity1TypePos, std::string::npos) << content();

  const auto quantityDefinitionPos = find("### my_test_package.MyTestQuantity", quantity1TypePos);
  ASSERT_NE(quantityDefinitionPos, std::string::npos) << content();

  const auto quantityTypePos = find("Representation: f32", quantityDefinitionPos);
  ASSERT_NE(quantityTypePos, std::string::npos) << content();

  EXPECT_NE(find("Unit:  m", quantityTypePos), std::string::npos) << content();
}

/// @test
/// Check sen gen mkdocs generates a correct documentation file from STL file with comments
TEST_F(AMkdocsGenerator, StlComment)
{
  generateStl(comment);

  const auto nodeTreeQuantityPos =
    find("[MyTestQuantity](#my_test_packagemytestquantity \"This is my test quantity type\")");
  ASSERT_NE(nodeTreeQuantityPos, std::string::npos) << content();

  const auto quantityDefinitionPos = find("### my_test_package.MyTestQuantity", nodeTreeQuantityPos);
  ASSERT_NE(quantityDefinitionPos, std::string::npos) << content();

  const auto quantityTypePos = find("Representation: f32", quantityDefinitionPos);
  ASSERT_NE(quantityTypePos, std::string::npos) << content();

  EXPECT_NE(find("Unit:  m", quantityTypePos), std::string::npos) << content();
}

/// @test
/// Check sen gen mkdocs generates a correct documentation file from STL file with methods in the class
TEST_F(AMkdocsGenerator, StlMethods)
{
  generateStl(methods);

  const auto nodeTreeClassPos = find("[my_test_package.MyTestClass](#my_test_packagemytestclass)");
  ASSERT_NE(nodeTreeClassPos, std::string::npos) << content();

  const auto classDefinitionPos = find("### my_test_package.MyTestClass", nodeTreeClassPos);
  ASSERT_NE(classDefinitionPos, std::string::npos) << content();

  const auto propHeaderPos = find("**Properties**", classDefinitionPos);
  ASSERT_NE(propHeaderPos, std::string::npos) << content();

  const auto resultDeclarationPos = find("**result**", propHeaderPos);
  ASSERT_NE(resultDeclarationPos, std::string::npos) << content();

  const auto resultQualifierPos = find("{ title=\"Writable\" }", resultDeclarationPos);
  ASSERT_NE(resultQualifierPos, std::string::npos) << content();

  const auto resultTypePos = find("[i32](#i32)", resultQualifierPos);
  ASSERT_NE(resultTypePos, std::string::npos) << content();

  const auto methodsHeaderPos = find("**Methods**", resultTypePos);
  ASSERT_NE(methodsHeaderPos, std::string::npos) << content();

  const auto addMethodPos = find("**addNumbers**", methodsHeaderPos);
  ASSERT_NE(addMethodPos, std::string::npos) << content();

  const auto addMethodTypePos = find("[i32](#i32)", addMethodPos);
  ASSERT_NE(addMethodTypePos, std::string::npos) << content();

  const auto addMethodReturnTypePos = find("Returns: [i32](#i32)", addMethodTypePos);
  ASSERT_NE(addMethodReturnTypePos, std::string::npos) << content();

  const auto addMethodAPropPos = find("a | [i32](#i32)", addMethodReturnTypePos);
  ASSERT_NE(addMethodAPropPos, std::string::npos) << content();

  const auto addMethodBPropPos = find("b | [i32](#i32)", addMethodAPropPos);
  ASSERT_NE(addMethodBPropPos, std::string::npos) << content();

  const auto subtractMethodPos = find("**subtractNumbers**", addMethodBPropPos);
  ASSERT_NE(subtractMethodPos, std::string::npos) << content();

  const auto subtractMethodTypePos = find("[i32](#i32)", subtractMethodPos);
  ASSERT_NE(subtractMethodTypePos, std::string::npos) << content();

  const auto subtractMethodReturnTypePos = find("Returns: [i32](#i32)", subtractMethodTypePos);
  ASSERT_NE(subtractMethodReturnTypePos, std::string::npos) << content();

  const auto subtractMethodAPropPos = find("a | [i32](#i32)", subtractMethodReturnTypePos);
  ASSERT_NE(subtractMethodAPropPos, std::string::npos) << content();

  EXPECT_NE(find("b | [i32](#i32)", subtractMethodAPropPos), std::string::npos) << content();
}

/// @test
/// Check sen gen mkdocs generates a correct documentation file from STL file with events in the class
TEST_F(AMkdocsGenerator, StlEvents)
{
  generateStl(events);

  const auto nodeTreeClassPos = find("[my_test_package.MyTestClass](#my_test_packagemytestclass)");
  ASSERT_NE(nodeTreeClassPos, std::string::npos) << content();

  const auto classDefinitionPos = find("### my_test_package.MyTestClass", nodeTreeClassPos);
  ASSERT_NE(classDefinitionPos, std::string::npos) << content();

  const auto propHeaderPos = find("**Properties**", classDefinitionPos);
  ASSERT_NE(propHeaderPos, std::string::npos) << content();

  const auto resultDeclarationPos = find("**result**", propHeaderPos);
  ASSERT_NE(resultDeclarationPos, std::string::npos) << content();

  const auto resultQualifierPos = find("{ title=\"Writable\" }", resultDeclarationPos);
  ASSERT_NE(resultQualifierPos, std::string::npos) << content();

  const auto resultTypePos = find("[i32](#i32)", resultQualifierPos);
  ASSERT_NE(resultTypePos, std::string::npos) << content();

  const auto eventsHeaderPos = find("**Events**", resultTypePos);
  ASSERT_NE(eventsHeaderPos, std::string::npos) << content();

  const auto divisionByZeroEventPos = find("**divisionByZero**", eventsHeaderPos);
  ASSERT_NE(divisionByZeroEventPos, std::string::npos) << content();

  EXPECT_NE(find(":   Emitted when there's an attempt to divide by zero.", divisionByZeroEventPos), std::string::npos)
    << content();
}

/// @test
/// Check sen gen mkdocs generates a correct documentation file from STL file with inheritance
TEST_F(AMkdocsGenerator, StlInheritance)
{
  generateStl(inheritance);

  const auto nodeTreeBaseClassPos = find("[my_test_package.MyBaseTestClass](#my_test_packagemybasetestclass)");
  ASSERT_NE(nodeTreeBaseClassPos, std::string::npos) << content();

  const auto nodeTreeDerivedClassPos =
    find("[my_test_package.MyDerivedTestClass](#my_test_packagemyderivedtestclass)", nodeTreeBaseClassPos);
  ASSERT_NE(nodeTreeDerivedClassPos, std::string::npos) << content();

  const auto baseClassDefinitionPos = find("### my_test_package.MyBaseTestClass", nodeTreeDerivedClassPos);
  ASSERT_NE(baseClassDefinitionPos, std::string::npos) << content();

  const auto baseClassPropHeaderPos = find("**Properties**", baseClassDefinitionPos);
  ASSERT_NE(baseClassPropHeaderPos, std::string::npos) << content();

  const auto basePropPos = find("**baseProp**", baseClassPropHeaderPos);
  ASSERT_NE(basePropPos, std::string::npos) << content();

  const auto basePropQualifierPos = find("{ title=\"Confirmed\" }", basePropPos);
  ASSERT_NE(basePropQualifierPos, std::string::npos) << content();

  const auto basePropTypePos = find("[string](#string)", basePropQualifierPos);
  ASSERT_NE(basePropTypePos, std::string::npos) << content();

  const auto derivedClassDefinitionPos = find("### my_test_package.MyDerivedTestClass", basePropTypePos);
  ASSERT_NE(derivedClassDefinitionPos, std::string::npos) << content();

  const auto inheritancePos =
    find("Parent: [my_test_package.MyBaseTestClass](#my_test_packagemybasetestclass)", derivedClassDefinitionPos);
  ASSERT_NE(inheritancePos, std::string::npos) << content();

  const auto derivedClassPropHeaderPos = find("**Properties**", inheritancePos);
  ASSERT_NE(derivedClassPropHeaderPos, std::string::npos) << content();

  const auto derivedPropPos = find("**derivedProp**", derivedClassPropHeaderPos);
  ASSERT_NE(derivedPropPos, std::string::npos) << content();

  const auto derivedPropQualifierPos = find("{ title=\"Writable\" }", derivedPropPos);
  ASSERT_NE(derivedPropQualifierPos, std::string::npos) << content();

  EXPECT_NE(find("[f64](#f64)", derivedPropQualifierPos), std::string::npos) << content();
}

/// @test
/// Check sen gen mkdocs generates a correct node tree in the documentation file when multiple classes are defined
TEST_F(AMkdocsGenerator, StlNodeTree)
{
  generateStl(nodeTree);

  const auto nodeTreeBaseClassPos = find("├─┬─[my_test_package.MyBaseTestClass](#my_test_packagemybasetestclass)");
  ASSERT_NE(nodeTreeBaseClassPos, std::string::npos) << content();

  const auto nodeTreeDerivedClassPos =
    find("│ └───[my_test_package.MyDerivedTestClass](#my_test_packagemyderivedtestclass)", nodeTreeBaseClassPos);
  ASSERT_NE(nodeTreeDerivedClassPos, std::string::npos) << content();

  const auto nodeTreeAnotherBaseClassPos =
    find("└───[my_test_package.AnotherBaseTestClass](#my_test_packageanotherbasetestclass)", nodeTreeDerivedClassPos);
  ASSERT_NE(nodeTreeAnotherBaseClassPos, std::string::npos) << content();

  const auto baseClassDefinitionPos = find("### my_test_package.MyBaseTestClass", nodeTreeAnotherBaseClassPos);
  ASSERT_NE(baseClassDefinitionPos, std::string::npos) << content();

  const auto derivedClassDefinitionPos = find("### my_test_package.MyDerivedTestClass", baseClassDefinitionPos);
  ASSERT_NE(derivedClassDefinitionPos, std::string::npos) << content();

  const auto inheritancePos =
    find("Parent: [my_test_package.MyBaseTestClass](#my_test_packagemybasetestclass)", derivedClassDefinitionPos);
  ASSERT_NE(inheritancePos, std::string::npos) << content();

  EXPECT_NE(find("### my_test_package.AnotherBaseTestClass", inheritancePos), std::string::npos) << content();
}

/// @test
/// Check sen gen mkdocs generates a correct struct list in the documentation file when multiple structs are defined
TEST_F(AMkdocsGenerator, StlStructList)
{
  generateStl(structList);

  const auto structListFirstLinePos = find(
    "| [MyTestStruct](#my_test_packagemyteststruct) | "
    "[MyTestStruct2](#my_test_packagemyteststruct2)| "
    "[MyTestStruct3](#my_test_packagemyteststruct3)| "
    "[MyTestStruct4](#my_test_packagemyteststruct4)|");
  ASSERT_NE(structListFirstLinePos, std::string::npos) << content();

  const auto structListSecondLinePos =
    find("| [MyTestStruct5](#my_test_packagemyteststruct5)|", structListFirstLinePos);
  ASSERT_NE(structListSecondLinePos, std::string::npos) << content();

  const auto structDefinitionPos = find("### my_test_package.MyTestStruct", structListSecondLinePos);
  ASSERT_NE(structDefinitionPos, std::string::npos) << content();

  const auto struct2DefinitionPos = find("### my_test_package.MyTestStruct2", structDefinitionPos);
  ASSERT_NE(struct2DefinitionPos, std::string::npos) << content();

  const auto struct3DefinitionPos = find("### my_test_package.MyTestStruct3", struct2DefinitionPos);
  ASSERT_NE(struct3DefinitionPos, std::string::npos) << content();

  const auto struct4DefinitionPos = find("### my_test_package.MyTestStruct4", struct3DefinitionPos);
  ASSERT_NE(struct4DefinitionPos, std::string::npos) << content();

  EXPECT_NE(find("### my_test_package.MyTestStruct5", struct4DefinitionPos), std::string::npos) << content();
}
