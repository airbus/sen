#include "plantuml_fixture_test.h"

// sen
#include "sen/gen/plantuml.h"

// 3rd party
#include <gtest/gtest.h>

// std
#include <string>

constexpr auto empty = R"()";

constexpr auto errorFormat = R"(;)";

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

constexpr auto nonGenerableProperties = R"(package my_test_package;

sequence<i64, 100> MyTestSequence;
quantity<f32, m> MyTestQuantity;

class MyTestClass
{
  var quantity1 : MyTestQuantity          [writable];
  var sequence1 : MyTestSequence          [writable];
}
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

  fn divideNumbers(a: i32, b: i32) -> i32;

  event divisionByZero();
}
)";

constexpr auto inheritance = R"(package my_test_package;

class MyBaseTestClass
{
  var baseProp : string [writable, confirmed];
}

class MyDerivedTestClass : extends MyBaseTestClass
{
  var derivedProp: f64 [static];
}
)";

/// @test
/// Check sen gen uml generates a correct PlantUML from an empty STL file
TEST_F(APlantUMLGenerator, StlEmpty)
{
  generateStl(empty);

  const auto startPos = find("@startuml");
  ASSERT_NE(startPos, std::string::npos) << content();

  EXPECT_NE(find("@enduml", startPos), std::string::npos) << content();
}

/// @test
/// Check sen gen uml does not generate a PlantUML from an invalid STL file
TEST_F(APlantUMLGenerator, StlErrorFormat) { ASSERT_ANY_THROW(generateStl(errorFormat)); }

/// @test
/// Check sen gen uml generates a correct PlantUML from STL file with empty class definition
TEST_F(APlantUMLGenerator, StlEmptyClass)
{
  generateStl(emptyClass);

  const auto startPos = find("@startuml");
  ASSERT_NE(startPos, std::string::npos) << content();

  const auto packagePos = find("package my_test_package <<Folder>>", startPos);
  ASSERT_NE(packagePos, std::string::npos) << content();

  const auto classPos = find("class MyTestClass", packagePos);
  ASSERT_NE(classPos, std::string::npos) << content();

  EXPECT_NE(find("@enduml", classPos), std::string::npos) << content();
}

/// @test
/// Check sen gen uml generates a correct PlantUML from STL file with basic properties in the class
TEST_F(APlantUMLGenerator, StlBasicProperties)
{
  generateStl(basicProperties);

  const auto startPos = find("@startuml");
  ASSERT_NE(startPos, std::string::npos) << content();

  const auto packagePos = find("package my_test_package <<Folder>>", startPos);
  ASSERT_NE(packagePos, std::string::npos) << content();

  const auto classPos = find("class MyTestClass", packagePos);
  ASSERT_NE(classPos, std::string::npos) << content();

  const auto prop1Pos = find("+ prop1 : string", classPos);
  ASSERT_NE(prop1Pos, std::string::npos) << content();

  const auto prop2Pos = find("+ prop2 : i32", prop1Pos);
  ASSERT_NE(prop2Pos, std::string::npos) << content();

  EXPECT_NE(find("@enduml", prop2Pos), std::string::npos) << content();
}

/// @test
/// Check the generated PlantUML places a struct, its properties and a class that uses it
TEST_F(APlantUMLGenerator, StlStructProperty)
{
  generateStl(structProperty);

  const auto startPos = find("@startuml");
  ASSERT_NE(startPos, std::string::npos) << content();

  const auto packagePos = find("package my_test_package <<Folder>>", startPos);
  ASSERT_NE(packagePos, std::string::npos) << content();

  const auto structPos = find("class MyTestStruct", packagePos);
  ASSERT_NE(structPos, std::string::npos) << content();

  const auto prop1StructPos = find("+ f32 x", structPos);
  ASSERT_NE(prop1StructPos, std::string::npos) << content();

  const auto prop2StructPos = find("+ f64 y", prop1StructPos);
  ASSERT_NE(prop2StructPos, std::string::npos) << content();

  const auto classPos = find("class MyTestClass", prop2StructPos);
  ASSERT_NE(classPos, std::string::npos) << content();

  const auto propClassPos = find("+ struct1 : MyTestStruct", classPos);
  ASSERT_NE(propClassPos, std::string::npos) << content();

  EXPECT_NE(find("@enduml", propClassPos), std::string::npos) << content();
}

/// @test
/// Check the generated PlantUML places an enumeration, its enumerators and a class that uses it
TEST_F(APlantUMLGenerator, StlEnumProperty)
{
  generateStl(enumProperty);

  const auto startPos = find("@startuml");
  ASSERT_NE(startPos, std::string::npos) << content();

  const auto packagePos = find("package my_test_package <<Folder>>", startPos);
  ASSERT_NE(packagePos, std::string::npos) << content();

  const auto enumPos = find("enum MyTestEnum", packagePos);
  ASSERT_NE(enumPos, std::string::npos) << content();

  const auto value1EnumPos = find("first", enumPos);
  ASSERT_NE(value1EnumPos, std::string::npos) << content();

  const auto value2EnumPos = find("second", value1EnumPos);
  ASSERT_NE(value2EnumPos, std::string::npos) << content();

  const auto value3EnumPos = find("third", value2EnumPos);
  ASSERT_NE(value3EnumPos, std::string::npos) << content();

  const auto classPos = find("class MyTestClass", value3EnumPos);
  ASSERT_NE(classPos, std::string::npos) << content();

  const auto propClassPos = find("+ enum1 : MyTestEnum", classPos);
  ASSERT_NE(propClassPos, std::string::npos) << content();

  EXPECT_NE(find("@enduml", propClassPos), std::string::npos) << content();
}

/// @test
/// Check sen gen uml generates a correct PlantUML from STL file with a variant property in the class
TEST_F(APlantUMLGenerator, StlVariantProperty)
{
  generateStl(variantProperty);

  const auto startPos = find("@startuml");
  ASSERT_NE(startPos, std::string::npos) << content();

  const auto packagePos = find("package my_test_package <<Folder>>", startPos);
  ASSERT_NE(packagePos, std::string::npos) << content();

  const auto variantPos = find("class MyTestVariant << (V,orchid) Variant >>", packagePos);
  ASSERT_NE(variantPos, std::string::npos) << content();

  const auto type1VariantPos = find("i32", variantPos);
  ASSERT_NE(type1VariantPos, std::string::npos) << content();

  const auto type2VariantPos = find("bool", type1VariantPos);
  ASSERT_NE(type2VariantPos, std::string::npos) << content();

  const auto type3VariantPos = find("string", type2VariantPos);
  ASSERT_NE(type3VariantPos, std::string::npos) << content();

  const auto classPos = find("class MyTestClass", type3VariantPos);
  ASSERT_NE(classPos, std::string::npos) << content();

  const auto propClassPos = find("+ variant1 : MyTestVariant", classPos);
  ASSERT_NE(propClassPos, std::string::npos) << content();

  EXPECT_NE(find("@enduml", propClassPos), std::string::npos) << content();
}

/// @test
/// Check sen gen uml generates a correct PlantUML from STL file where the class contains properties with non-templated
/// types
TEST_F(APlantUMLGenerator, StlNonGenerableProperties)
{
  generateStl(nonGenerableProperties);

  const auto startPos = find("@startuml");
  ASSERT_NE(startPos, std::string::npos) << content();

  const auto packagePos = find("package my_test_package <<Folder>>", startPos);
  ASSERT_NE(packagePos, std::string::npos) << content();

  const auto classPos = find("class MyTestClass", packagePos);
  ASSERT_NE(classPos, std::string::npos) << content();

  const auto propQuantityPos = find("+ quantity1 : MyTestQuantity", classPos);
  ASSERT_NE(propQuantityPos, std::string::npos) << content();

  const auto propSequencePos = find("+ sequence1 : MyTestSequence", propQuantityPos);
  ASSERT_NE(propSequencePos, std::string::npos) << content();

  EXPECT_NE(find("@enduml", propSequencePos), std::string::npos) << content();
}

/// @test
/// Check sen gen uml generates a correct PlantUML from STL file with methods in the class
TEST_F(APlantUMLGenerator, StlMethods)
{
  generateStl(methods);

  const auto startPos = find("@startuml");
  ASSERT_NE(startPos, std::string::npos) << content();

  const auto packagePos = find("package my_test_package <<Folder>>", startPos);
  ASSERT_NE(packagePos, std::string::npos) << content();

  const auto classPos = find("class MyTestClass", packagePos);
  ASSERT_NE(classPos, std::string::npos) << content();

  const auto propPos = find("+ result : i32", classPos);
  ASSERT_NE(propPos, std::string::npos) << content();

  const auto methodHeaderPos = find("__ methods __", propPos);
  ASSERT_NE(methodHeaderPos, std::string::npos) << content();

  const auto addMethodPos = find("+ addNumbers(a, b) : i32", methodHeaderPos);
  ASSERT_NE(addMethodPos, std::string::npos) << content();

  const auto subtractMethodPos = find("+ subtractNumbers(a, b) : i32", addMethodPos);
  ASSERT_NE(subtractMethodPos, std::string::npos) << content();

  EXPECT_NE(find("@enduml", subtractMethodPos), std::string::npos) << content();
}

/// @test
/// Check sen gen uml generates a correct PlantUML from STL file with events in the class
TEST_F(APlantUMLGenerator, StlEvents)
{
  generateStl(events);

  const auto startPos = find("@startuml");
  ASSERT_NE(startPos, std::string::npos) << content();

  const auto packagePos = find("package my_test_package <<Folder>>", startPos);
  ASSERT_NE(packagePos, std::string::npos) << content();

  const auto classPos = find("class MyTestClass", packagePos);
  ASSERT_NE(classPos, std::string::npos) << content();

  const auto propPos = find("+ result : i32", classPos);
  ASSERT_NE(propPos, std::string::npos) << content();

  const auto methodHeaderPos = find("__ methods __", propPos);
  ASSERT_NE(methodHeaderPos, std::string::npos) << content();

  const auto methodPos = find("+ divideNumbers(a, b) : i32", methodHeaderPos);
  ASSERT_NE(methodPos, std::string::npos) << content();

  const auto eventHeaderPos = find("__ events __", methodPos);
  ASSERT_NE(eventHeaderPos, std::string::npos) << content();

  const auto eventPos = find("+ divisionByZero()", eventHeaderPos);
  ASSERT_NE(eventPos, std::string::npos) << content();

  EXPECT_NE(find("@enduml", eventPos), std::string::npos) << content();
}

/// @test
/// Check sen gen uml generates a correct PlantUML from STL file with inheritance
TEST_F(APlantUMLGenerator, StlInheritance)
{
  generateStl(inheritance);

  const auto startPos = find("@startuml");
  ASSERT_NE(startPos, std::string::npos) << content();

  const auto packagePos = find("package my_test_package <<Folder>>", startPos);
  ASSERT_NE(packagePos, std::string::npos) << content();

  const auto baseClassPos = find("class MyBaseTestClass", packagePos);
  ASSERT_NE(baseClassPos, std::string::npos) << content();

  const auto baseClassPropPos = find("+ baseProp : string", baseClassPos);
  ASSERT_NE(baseClassPropPos, std::string::npos) << content();

  const auto derivedClassPos = find("class MyDerivedTestClass", baseClassPropPos);
  ASSERT_NE(derivedClassPos, std::string::npos) << content();

  const auto derivedClassPropPos = find("+ derivedProp : f64", derivedClassPos);
  ASSERT_NE(derivedClassPropPos, std::string::npos) << content();

  const auto inheritanceRelation = find("MyBaseTestClass <|--- MyDerivedTestClass", derivedClassPropPos);
  ASSERT_NE(inheritanceRelation, std::string::npos) << content();

  EXPECT_NE(find("@enduml", inheritanceRelation), std::string::npos) << content();
}

/// @test
/// Check sen gen uml only generates PlantUML classes when the onlyClasses mode is used
TEST_F(APlantUMLGenerator, StlOnlyClasses)
{
  generateStl(structProperty, sen::gen::PlantUMLGenerationMode::onlyClasses);

  EXPECT_EQ(find("class MyTestStruct"), std::string::npos) << content();
  EXPECT_EQ(find("+ f32 x"), std::string::npos) << content();
  EXPECT_EQ(find("+ f64 y"), std::string::npos) << content();

  const auto startPos = find("@startuml");
  ASSERT_NE(startPos, std::string::npos) << content();

  const auto packagePos = find("package my_test_package <<Folder>>", startPos);
  ASSERT_NE(packagePos, std::string::npos) << content();

  const auto classPos = find("class MyTestClass", packagePos);
  ASSERT_NE(classPos, std::string::npos) << content();

  const auto propPos = find("+ struct1 : MyTestStruct", classPos);
  ASSERT_NE(propPos, std::string::npos) << content();

  EXPECT_NE(find("@enduml", propPos), std::string::npos) << content();
}

/// @test
/// Check sen gen uml only generates PlantUML special types definition when the onlyBasicTypes mode is used
TEST_F(APlantUMLGenerator, StlOnlyBasicTypes)
{
  generateStl(structProperty, sen::gen::PlantUMLGenerationMode::onlyBasicTypes);

  EXPECT_EQ(find("MyTestClass"), std::string::npos) << content();
  EXPECT_EQ(find("+ struct1 : MyTestStruct"), std::string::npos) << content();

  const auto startPos = find("@startuml");
  ASSERT_NE(startPos, std::string::npos) << content();

  const auto packagePos = find("package my_test_package <<Folder>>", startPos);
  ASSERT_NE(packagePos, std::string::npos) << content();

  const auto structPos = find("class MyTestStruct", packagePos);
  ASSERT_NE(structPos, std::string::npos) << content();

  const auto prop1StructPos = find("+ f32 x", structPos);
  ASSERT_NE(prop1StructPos, std::string::npos) << content();

  const auto prop2StructPos = find("+ f64 y", prop1StructPos);
  ASSERT_NE(prop2StructPos, std::string::npos) << content();

  EXPECT_NE(find("@enduml", prop2StructPos), std::string::npos) << content();
}

/// @test
/// Check sen gen uml does not generate PlantUML enums when the noEnumerators mode is used
TEST_F(APlantUMLGenerator, StlNoEnumerators)
{
  generateStl(enumProperty, sen::gen::PlantUMLGenerationMode::all, sen::gen::PlantUMLEnumMode::noEnumerators);

  const auto startPos = find("@startuml");
  ASSERT_NE(startPos, std::string::npos) << content();

  const auto packagePos = find("package my_test_package <<Folder>>", startPos);
  ASSERT_NE(packagePos, std::string::npos) << content();

  const auto classPos = find("class MyTestClass", packagePos);
  ASSERT_NE(classPos, std::string::npos) << content();

  const auto propPos = find("+ enum1 : MyTestEnum", classPos);
  ASSERT_NE(propPos, std::string::npos) << content();

  EXPECT_NE(find("@enduml", propPos), std::string::npos) << content();
}
