// === plantuml_fom_generator_test.cpp =================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "plantuml_fixture_test.h"

// 3rd party
#include <gtest/gtest.h>

// std
#include <filesystem>
#include <string>
#include <vector>

/// @test
/// Renders a minimal FOM as a PlantUML diagram framed by startuml and enduml, holding the hla
/// package with class ObjectRoot and its rtiId string property.
TEST_F(APlantUMLGenerator, FomEmpty)
{
  const std::vector<std::filesystem::path> paths {dataDirectoryPath() / "plantuml" / "empty"};
  ASSERT_TRUE(std::filesystem::exists(paths.front())) << paths.front();

  generateFom(paths);

  const auto startPos = find("@startuml");
  ASSERT_NE(startPos, std::string::npos) << content();

  const auto packagePos = find("package hla <<Folder>>", startPos);
  ASSERT_NE(packagePos, std::string::npos) << content();

  const auto classPos = find("class ObjectRoot", packagePos);
  ASSERT_NE(classPos, std::string::npos) << content();

  const auto propPos = find("+ rtiId : string", classPos);
  ASSERT_NE(propPos, std::string::npos) << content();

  EXPECT_NE(find("@enduml", propPos), std::string::npos) << content();
}

/// @test
/// Throws instead of producing a diagram when the FOM file is malformed.
TEST_F(APlantUMLGenerator, FomErrorFormat)
{
  const std::vector<std::filesystem::path> paths {dataDirectoryPath() / "plantuml" / "error_format"};
  ASSERT_TRUE(std::filesystem::exists(paths.front())) << paths.front();

  ASSERT_ANY_THROW(generateFom(paths));
}

/// @test
/// Renders a FOM class that declares nothing as a PlantUML class in its own package folder,
/// with an inheritance arrow from ObjectRoot.
TEST_F(APlantUMLGenerator, FomEmptyClass)
{
  const std::vector<std::filesystem::path> paths {dataDirectoryPath() / "plantuml" / "empty_class"};
  ASSERT_TRUE(std::filesystem::exists(paths.front())) << paths.front();

  generateFom(paths);

  const auto startPos = find("@startuml");
  ASSERT_NE(startPos, std::string::npos) << content();

  const auto hlaPackagePos = find("package hla <<Folder>>", startPos);
  ASSERT_NE(hlaPackagePos, std::string::npos) << content();

  const auto hlaClassPos = find("class ObjectRoot", hlaPackagePos);
  ASSERT_NE(hlaClassPos, std::string::npos) << content();

  const auto hlaPropPos = find("+ rtiId : string", hlaClassPos);
  ASSERT_NE(hlaPropPos, std::string::npos) << content();

  const auto userCreatedPackagePos = find("package empty_class <<Folder>>", hlaPropPos);
  ASSERT_NE(userCreatedPackagePos, std::string::npos) << content();

  const auto userCreatedClassPos = find("class MyTestClass", userCreatedPackagePos);
  ASSERT_NE(userCreatedClassPos, std::string::npos) << content();

  const auto inheritanceRelation = find("ObjectRoot <|--- MyTestClass", userCreatedClassPos);
  ASSERT_NE(inheritanceRelation, std::string::npos) << content();

  EXPECT_NE(find("@enduml", inheritanceRelation), std::string::npos) << content();
}

/// @test
/// Renders a FOM class property as a typed PlantUML member, prop1 of MyTestInt, alongside the
/// inheritance from ObjectRoot.
TEST_F(APlantUMLGenerator, FomBasicProperties)
{
  const std::vector<std::filesystem::path> paths {dataDirectoryPath() / "plantuml" / "basic_properties"};
  ASSERT_TRUE(std::filesystem::exists(paths.front())) << paths.front();

  generateFom(paths);

  const auto startPos = find("@startuml");
  ASSERT_NE(startPos, std::string::npos) << content();

  const auto hlaPackagePos = find("package hla <<Folder>>", startPos);
  ASSERT_NE(hlaPackagePos, std::string::npos) << content();

  const auto hlaClassPos = find("class ObjectRoot", hlaPackagePos);
  ASSERT_NE(hlaClassPos, std::string::npos) << content();

  const auto hlaPropPos = find("+ rtiId : string", hlaClassPos);
  ASSERT_NE(hlaPropPos, std::string::npos) << content();

  const auto userCreatedPackagePos = find("package basic_properties <<Folder>>", hlaPropPos);
  ASSERT_NE(userCreatedPackagePos, std::string::npos) << content();

  const auto userCreatedClassPos = find("class MyTestClass", userCreatedPackagePos);
  ASSERT_NE(userCreatedClassPos, std::string::npos) << content();

  const auto userCreatedPropPos = find("+ prop1 : MyTestInt", userCreatedClassPos);
  ASSERT_NE(userCreatedPropPos, std::string::npos) << content();

  const auto inheritanceRelation = find("ObjectRoot <|--- MyTestClass", userCreatedPropPos);
  ASSERT_NE(inheritanceRelation, std::string::npos) << content();

  EXPECT_NE(find("@enduml", inheritanceRelation), std::string::npos) << content();
}

/// @test
/// Renders a FOM struct as a stereotyped PlantUML class listing both of its fields, with the
/// class property typed by the struct and the inheritance from ObjectRoot.
TEST_F(APlantUMLGenerator, FomStructProperty)
{
  const std::vector<std::filesystem::path> paths {dataDirectoryPath() / "plantuml" / "struct_property"};
  ASSERT_TRUE(std::filesystem::exists(paths.front())) << paths.front();

  generateFom(paths);

  const auto startPos = find("@startuml");
  ASSERT_NE(startPos, std::string::npos) << content();

  const auto hlaPackagePos = find("package hla <<Folder>>", startPos);
  ASSERT_NE(hlaPackagePos, std::string::npos) << content();

  const auto hlaClassPos = find("class ObjectRoot", hlaPackagePos);
  ASSERT_NE(hlaClassPos, std::string::npos) << content();

  const auto hlaPropPos = find("+ rtiId : string", hlaClassPos);
  ASSERT_NE(hlaPropPos, std::string::npos) << content();

  const auto userCreatedPackagePos = find("package struct_property <<Folder>>", hlaPropPos);
  ASSERT_NE(userCreatedPackagePos, std::string::npos) << content();

  const auto structPos = find("class MyTestStruct << (S,#FF7700) Struct >>", userCreatedPackagePos);
  ASSERT_NE(structPos, std::string::npos) << content();

  const auto prop1StructPos = find("+ MyTestInt32 field1", structPos);
  ASSERT_NE(prop1StructPos, std::string::npos) << content();

  const auto prop2StructPos = find("+ MyTestInt16 field2", prop1StructPos);
  ASSERT_NE(prop2StructPos, std::string::npos) << content();

  const auto secondPackagePos = find("package struct_property <<Folder>>", prop2StructPos);
  ASSERT_NE(secondPackagePos, std::string::npos) << content();

  const auto userCreatedClassPos = find("class MyTestClass", secondPackagePos);
  ASSERT_NE(userCreatedClassPos, std::string::npos) << content();

  const auto userCreatedPropPos = find("+ prop1 : MyTestStruct", userCreatedClassPos);
  ASSERT_NE(userCreatedPropPos, std::string::npos) << content();

  const auto inheritanceRelation = find("ObjectRoot <|--- MyTestClass", userCreatedPropPos);
  ASSERT_NE(inheritanceRelation, std::string::npos) << content();

  EXPECT_NE(find("@enduml", inheritanceRelation), std::string::npos) << content();
}

/// @test
/// Renders a FOM enum as a PlantUML enum listing all three enumerators, with the class property
/// typed by the enum and the inheritance from ObjectRoot.
TEST_F(APlantUMLGenerator, FomEnumProperty)
{
  const std::vector<std::filesystem::path> paths {dataDirectoryPath() / "plantuml" / "enum_property"};
  ASSERT_TRUE(std::filesystem::exists(paths.front())) << paths.front();

  generateFom(paths);

  const auto startPos = find("@startuml");
  ASSERT_NE(startPos, std::string::npos) << content();

  const auto hlaPackagePos = find("package hla <<Folder>>", startPos);
  ASSERT_NE(hlaPackagePos, std::string::npos) << content();

  const auto hlaClassPos = find("class ObjectRoot", hlaPackagePos);
  ASSERT_NE(hlaClassPos, std::string::npos) << content();

  const auto hlaPropPos = find("+ rtiId : string", hlaClassPos);
  ASSERT_NE(hlaPropPos, std::string::npos) << content();

  const auto userCreatedPackagePos = find("package enum_property <<Folder>>", hlaPropPos);
  ASSERT_NE(userCreatedPackagePos, std::string::npos) << content();

  const auto enumPos = find("enum MyTestEnum", userCreatedPackagePos);
  ASSERT_NE(enumPos, std::string::npos) << content();

  const auto prop1EnumPos = find("first", enumPos);
  ASSERT_NE(prop1EnumPos, std::string::npos) << content();

  const auto prop2EnumPos = find("second", prop1EnumPos);
  ASSERT_NE(prop2EnumPos, std::string::npos) << content();

  const auto prop3EnumPos = find("third", prop2EnumPos);
  ASSERT_NE(prop3EnumPos, std::string::npos) << content();

  const auto secondPackagePos = find("package enum_property <<Folder>>", prop3EnumPos);
  ASSERT_NE(secondPackagePos, std::string::npos) << content();

  const auto userCreatedClassPos = find("class MyTestClass", secondPackagePos);
  ASSERT_NE(userCreatedClassPos, std::string::npos) << content();

  const auto userCreatedPropPos = find("+ prop1 : MyTestEnum", userCreatedClassPos);
  ASSERT_NE(userCreatedPropPos, std::string::npos) << content();

  const auto inheritanceRelation = find("ObjectRoot <|--- MyTestClass", userCreatedPropPos);
  ASSERT_NE(inheritanceRelation, std::string::npos) << content();

  EXPECT_NE(find("@enduml", inheritanceRelation), std::string::npos) << content();
}
