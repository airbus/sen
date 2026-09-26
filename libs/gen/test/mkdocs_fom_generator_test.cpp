#include "mkdocs_fixture_test.h"

// 3rd party
#include <gtest/gtest.h>

// std
#include <filesystem>
#include <string>
#include <vector>

/// @test
/// Check sen gen mkdocs generates a correct documentation file from a minimal FOM file
TEST_F(AMkdocsGenerator, FomEmpty)
{
  const std::vector<std::filesystem::path> paths {archivePath() / "mkdocs" / "empty"};
  ASSERT_TRUE(std::filesystem::exists(paths.front())) << paths.front();

  generateFom(paths);

  EXPECT_FALSE(content().empty()) << content();
}

/// @test
/// Check sen gen mkdocs does not generate a documentation file from an invalid FOM file
TEST_F(AMkdocsGenerator, FomErrorFormat)
{
  const std::vector<std::filesystem::path> paths {archivePath() / "mkdocs" / "error_format"};
  ASSERT_TRUE(std::filesystem::exists(paths.front())) << paths.front();

  ASSERT_ANY_THROW(generateFom(paths));
}

/// @test
/// Check sen gen mkdocs generates a correct documentation file from FOM file with empty class definition
TEST_F(AMkdocsGenerator, FomEmptyClass)
{
  const std::vector<std::filesystem::path> paths {archivePath() / "mkdocs" / "empty_class"};
  ASSERT_TRUE(std::filesystem::exists(paths.front())) << paths.front();

  generateFom(paths);

  const auto nodeTreeClassPos = find("[empty_class.MyTestClass](#empty_classmytestclass)");
  ASSERT_NE(nodeTreeClassPos, std::string::npos) << content();

  const auto classDefinitionPos = find("### empty_class.MyTestClass", nodeTreeClassPos);
  ASSERT_NE(classDefinitionPos, std::string::npos) << content();

  EXPECT_NE(find("Parent: [hla.ObjectRoot](#hlaobjectroot)", classDefinitionPos), std::string::npos) << content();
}

/// @test
/// Check sen gen mkdocs generates a correct documentation file from FOM file with basic properties in the class
TEST_F(AMkdocsGenerator, FomBasicProperties)
{
  const std::vector<std::filesystem::path> paths {archivePath() / "mkdocs" / "basic_properties"};
  ASSERT_TRUE(std::filesystem::exists(paths.front())) << paths.front();

  generateFom(paths);

  const auto nodeTreeClassPos = find("[basic_properties.MyTestClass](#basic_propertiesmytestclass)");
  ASSERT_NE(nodeTreeClassPos, std::string::npos) << content();

  const auto classDefinitionPos = find("### basic_properties.MyTestClass", nodeTreeClassPos);
  ASSERT_NE(classDefinitionPos, std::string::npos) << content();

  const auto inheritancePos = find("Parent: [hla.ObjectRoot](#hlaobjectroot)", classDefinitionPos);
  ASSERT_NE(inheritancePos, std::string::npos) << content();

  const auto propHeaderPos = find("**Properties**", inheritancePos);
  ASSERT_NE(propHeaderPos, std::string::npos) << content();

  const auto propDefinitionPos = find("**prop1**", propHeaderPos);
  ASSERT_NE(propDefinitionPos, std::string::npos) << content();

  EXPECT_NE(find("[basic_properties.MyTestInt](#basic_propertiesmytestint)", propDefinitionPos), std::string::npos)
    << content();
}

/// @test
/// Check sen gen mkdocs generates a correct documentation file from FOM file with a struct property in the class
TEST_F(AMkdocsGenerator, FomStructProperty)
{
  const std::vector<std::filesystem::path> paths {archivePath() / "mkdocs" / "struct_property"};
  ASSERT_TRUE(std::filesystem::exists(paths.front())) << paths.front();

  generateFom(paths);

  const auto nodeTreeClassPos = find("[struct_property.MyTestClass](#struct_propertymytestclass)");
  ASSERT_NE(nodeTreeClassPos, std::string::npos) << content();

  const auto structDeclarationPos = find("struct_property", nodeTreeClassPos);
  ASSERT_NE(structDeclarationPos, std::string::npos) << content();

  const auto structTypePos = find("[MyTestStruct](#struct_propertymyteststruct)", structDeclarationPos);
  ASSERT_NE(structTypePos, std::string::npos) << content();

  const auto aliasDeclarationPos = find("struct_property", structTypePos);
  ASSERT_NE(aliasDeclarationPos, std::string::npos) << content();

  const auto aliasContentPos =
    find("[MyTestInt32](#struct_propertymytestint32)| [MyTestInt16](#struct_propertymytestint16)", aliasDeclarationPos);
  ASSERT_NE(aliasContentPos, std::string::npos) << content();

  const auto classDefinitionPos = find("### struct_property.MyTestClass", aliasContentPos);
  ASSERT_NE(classDefinitionPos, std::string::npos) << content();

  const auto inheritancePos = find("Parent: [hla.ObjectRoot](#hlaobjectroot)", classDefinitionPos);
  ASSERT_NE(inheritancePos, std::string::npos) << content();

  const auto propHeaderPos = find("**Properties**", inheritancePos);
  ASSERT_NE(propHeaderPos, std::string::npos) << content();

  const auto propDefinitionPos = find("**prop1**", propHeaderPos);
  ASSERT_NE(propDefinitionPos, std::string::npos) << content();

  const auto propTypePos = find("[struct_property.MyTestStruct](#struct_propertymyteststruct)", propDefinitionPos);
  ASSERT_NE(propTypePos, std::string::npos) << content();

  const auto structDefinitionPos = find("### struct_property.MyTestStruct", propTypePos);
  ASSERT_NE(structDefinitionPos, std::string::npos) << content();

  const auto structField1Pos =
    find("**field1** | [struct_property.MyTestInt32](#struct_propertymytestint32)", structDefinitionPos);
  ASSERT_NE(structField1Pos, std::string::npos) << content();

  EXPECT_NE(find("**field2** | [struct_property.MyTestInt16](#struct_propertymytestint16)", structField1Pos),
            std::string::npos)
    << content();
}

/// @test
/// Check sen gen mkdocs generates a correct documentation file from FOM file with an enum property in the class
TEST_F(AMkdocsGenerator, FomEnumProperty)
{
  const std::vector<std::filesystem::path> paths {archivePath() / "mkdocs" / "enum_property"};
  ASSERT_TRUE(std::filesystem::exists(paths.front())) << paths.front();

  generateFom(paths);

  const auto nodeTreeClassPos = find("[enum_property.MyTestClass](#enum_propertymytestclass)");
  ASSERT_NE(nodeTreeClassPos, std::string::npos) << content();

  const auto enumDeclarationPos = find("enum_property", nodeTreeClassPos);
  ASSERT_NE(enumDeclarationPos, std::string::npos) << content();

  const auto enumTypePos = find("[MyTestEnum](#enum_propertymytestenum)", enumDeclarationPos);
  ASSERT_NE(enumTypePos, std::string::npos) << content();

  const auto classDefinitionPos = find("### enum_property.MyTestClass", enumTypePos);
  ASSERT_NE(classDefinitionPos, std::string::npos) << content();

  const auto inheritancePos = find("Parent: [hla.ObjectRoot](#hlaobjectroot)", classDefinitionPos);
  ASSERT_NE(inheritancePos, std::string::npos) << content();

  const auto propHeaderPos = find("**Properties**", inheritancePos);
  ASSERT_NE(propHeaderPos, std::string::npos) << content();

  const auto propDefinitionPos = find("**prop1**", propHeaderPos);
  ASSERT_NE(propDefinitionPos, std::string::npos) << content();

  const auto propTypePos = find("[enum_property.MyTestEnum](#enum_propertymytestenum)", propDefinitionPos);
  ASSERT_NE(propTypePos, std::string::npos) << content();

  const auto enumDefinitionPos = find("### enum_property.MyTestEnum", propTypePos);
  ASSERT_NE(enumDefinitionPos, std::string::npos) << content();

  const auto enumRepresentationPos = find("Representation: [i32](#i32)", enumDefinitionPos);
  ASSERT_NE(enumRepresentationPos, std::string::npos) << content();

  const auto enumValue1Pos = find("first | 1 |", enumRepresentationPos);
  ASSERT_NE(enumValue1Pos, std::string::npos) << content();

  const auto enumValue2Pos = find("second | 2 |", enumValue1Pos);
  ASSERT_NE(enumValue2Pos, std::string::npos) << content();

  EXPECT_NE(find("third | 3 |", enumValue2Pos), std::string::npos) << content();
}
