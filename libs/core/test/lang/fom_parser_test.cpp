// === fom_parser_test.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/lang/fom_parser.h"
#include "sen/core/lang/stl_resolver.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/variant_type.h"

// google test
#include <gtest/gtest.h>

// std
#include <algorithm>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{

/// A function rather than a namespace-scope object, which has a static initialisation order.
std::filesystem::path fomData() { return std::filesystem::path {FOM_TEST_DATA_DIR}; }

class AFomParser: public ::testing::Test
{
protected:
  /// Reads the base module, with the named extension files beside it.
  void read(const std::vector<std::string>& extensionNames = {})
  {
    std::vector<std::filesystem::path> extensions;
    extensions.reserve(extensionNames.size());
    for (const auto& name: extensionNames)
    {
      extensions.push_back(fomData() / "extensions" / name);
    }

    context = sen::lang::parseFomDocuments({fomData() / "base"}, extensions, {}, sen::lang::TypeSettings {});
  }

  /// Reads one module directory on its own, for the cases that need no extension.
  void readModule(const std::string& directory)
  {
    context = sen::lang::parseFomDocuments({fomData() / directory}, {}, {}, sen::lang::TypeSettings {});
  }

  /// Reads one module directory with a mapping file, which decides the events and methods a class
  /// gets. Nothing else here passes one.
  void readWithMapping(const std::string& directory, const std::string& mapping)
  {
    context =
      sen::lang::parseFomDocuments({fomData() / directory}, {}, {fomData() / mapping}, sen::lang::TypeSettings {});
  }

  /// Reads one module directory with both a mapping file and extensions beside it.
  void readWithMappingAndExtensions(const std::string& directory,
                                    const std::string& mapping,
                                    const std::vector<std::string>& extensionNames)
  {
    std::vector<std::filesystem::path> extensions;
    extensions.reserve(extensionNames.size());
    for (const auto& name: extensionNames)
    {
      extensions.push_back(fomData() / "extensions" / name);
    }

    context = sen::lang::parseFomDocuments(
      {fomData() / directory}, extensions, {fomData() / mapping}, sen::lang::TypeSettings {});
  }

  /// Reads two module directories, for the cases about how modules compose.
  void readModules(const std::string& first, const std::string& second)
  {
    context = sen::lang::parseFomDocuments({fomData() / first, fomData() / second}, {}, {}, sen::lang::TypeSettings {});
  }

  [[nodiscard]] const sen::ClassType* classAt(std::string_view qualifiedName) const
  {
    auto handle = find(qualifiedName);
    if (!handle)
    {
      return nullptr;
    }
    auto asClass = sen::dynamicTypeHandleCast<const sen::ClassType>(handle.value());
    return asClass ? asClass.value().type() : nullptr;
  }

  [[nodiscard]] sen::MaybeConstTypeHandle<> find(std::string_view qualifiedName) const
  {
    for (const auto& typeSet: context)
    {
      for (const auto& type: typeSet.types)
      {
        if (type->getQualifiedName() == qualifiedName)
        {
          return type;
        }
      }
    }
    return std::nullopt;
  }

  /// The type set a type landed in, which decides the file it is generated into.
  [[nodiscard]] std::string fileOf(std::string_view qualifiedName) const
  {
    for (const auto& typeSet: context)
    {
      for (const auto& type: typeSet.types)
      {
        if (type->getQualifiedName() == qualifiedName)
        {
          return typeSet.fileName;
        }
      }
    }
    return {};
  }

  [[nodiscard]] const sen::ClassType* vehicle() const
  {
    auto handle = find("base.Vehicle");
    if (!handle)
    {
      return nullptr;
    }
    auto asClass = sen::dynamicTypeHandleCast<const sen::ClassType>(handle.value());
    return asClass ? asClass.value().type() : nullptr;
  }

  [[nodiscard]] static const sen::Property* property(const sen::ClassType& type, std::string_view name)
  {
    const auto properties = type.getProperties(sen::ClassType::SearchMode::doNotIncludeParents);
    const auto itr =
      std::find_if(properties.begin(), properties.end(), [name](const auto& elem) { return elem->getName() == name; });
    return itr != properties.end() ? itr->get() : nullptr;
  }

  sen::lang::TypeSetContext context;  // NOLINT(misc-non-private-member-variables-in-classes)
};

//--------------------------------------------------------------------------------------------------------------
// What the module declares on its own
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A module read on its own yields the class and the one property it declares.
TEST_F(AFomParser, readsTheModuleWithNoExtensions)
{
  read();

  ASSERT_NE(vehicle(), nullptr);
  EXPECT_EQ(vehicle()->getProperties(sen::ClassType::SearchMode::doNotIncludeParents).size(), 1U);
  EXPECT_NE(property(*vehicle(), "speed"), nullptr);
}

//--------------------------------------------------------------------------------------------------------------
// Adding a member
//--------------------------------------------------------------------------------------------------------------

/// @test
/// An extension naming a class the module declares adds a property to it.
TEST_F(AFomParser, addsAPropertyToAClass)
{
  read({"property.xml"});

  ASSERT_NE(vehicle(), nullptr);
  const auto* added = property(*vehicle(), "callsign");
  ASSERT_NE(added, nullptr);
  EXPECT_EQ(added->getDescription(), "What it is called.");
  EXPECT_EQ(added->getCategory(), sen::PropertyCategory::dynamicRO);
  EXPECT_EQ(added->getTransportMode(), sen::TransportMode::confirmed);
  // Knots, not the Metres every other member uses, so this distinguishes the contributed
  // dataType being resolved from every type in the fixture happening to be the same one.
  EXPECT_EQ(added->getType()->getName(), "Knots");
}

/// @test
/// A contributed property does not move the class out of its own module's package.
TEST_F(AFomParser, keepsTheClassWhereItsOwnModuleDeclaredIt)
{
  read({"property.xml"});

  // The count is here because the file assertion alone holds whether or not anything merged.
  EXPECT_EQ(fileOf("base.Vehicle"), "site.xml");
  ASSERT_NE(vehicle(), nullptr);
  EXPECT_EQ(vehicle()->getProperties(sen::ClassType::SearchMode::doNotIncludeParents).size(), 2U);
}

/// @test
/// A contributed property is appended, so the module's own properties keep their order.
TEST_F(AFomParser, addsTheContributedPropertyAfterTheOnesTheModuleDeclares)
{
  read({"property.xml"});

  ASSERT_NE(vehicle(), nullptr);
  const auto properties = vehicle()->getProperties(sen::ClassType::SearchMode::doNotIncludeParents);
  ASSERT_EQ(properties.size(), 2U);
  EXPECT_EQ(properties[0]->getName(), "speed");
  EXPECT_EQ(properties[1]->getName(), "callsign");
}

/// @test
/// An extension adds an enumerator to an enumeration a module declares.
TEST_F(AFomParser, addsAnEnumeratorToAnEnumeration)
{
  read({"enumerator.xml"});

  auto handle = find("base.Colour");
  ASSERT_TRUE(handle.has_value());
  auto asEnum = sen::dynamicTypeHandleCast<const sen::EnumType>(handle.value());
  ASSERT_TRUE(asEnum.has_value());

  EXPECT_EQ(asEnum.value()->getEnums().size(), 2U);
  const auto* added = asEnum.value()->getEnumFromName("blue");
  ASSERT_NE(added, nullptr);
  EXPECT_EQ(added->key, 2U);
}

/// @test
/// An extension adds a field to a fixed record a module declares.
TEST_F(AFomParser, addsAFieldToAFixedRecord)
{
  read({"field.xml"});

  auto handle = find("base.PositionStruct");
  ASSERT_TRUE(handle.has_value());
  auto asStruct = sen::dynamicTypeHandleCast<const sen::StructType>(handle.value());
  ASSERT_TRUE(asStruct.has_value());

  const auto fields = asStruct.value()->getFields();
  ASSERT_EQ(fields.size(), 2U);
  EXPECT_EQ(fields[0].name, "x");
  EXPECT_EQ(fields[1].name, "y");
  EXPECT_EQ(fields[1].description, "Up.");
}

/// @test
/// An extension adds an alternative to a variant record a module declares.
TEST_F(AFomParser, addsAnAlternativeToAVariantRecord)
{
  read({"alternative.xml"});

  auto handle = find("base.ShapeStruct");
  ASSERT_TRUE(handle.has_value());
  auto asVariant = sen::dynamicTypeHandleCast<const sen::VariantType>(handle.value());
  ASSERT_TRUE(asVariant.has_value());

  const auto fields = asVariant.value()->getFields();
  ASSERT_EQ(fields.size(), 2U);

  // The keys count on from the alternatives the module declares, so a contributed one does not
  // take a key an existing alternative already uses.
  EXPECT_EQ(fields[0].key, 0U);
  EXPECT_EQ(fields[1].key, 1U);
  EXPECT_EQ(fields[1].description, "A line.");
}

/// @test
/// One run of four extensions adds a property, an enumerator, a field and an alternative together.
TEST_F(AFomParser, addsMembersOfEveryKindAtOnce)
{
  read({"property.xml", "enumerator.xml", "field.xml", "alternative.xml"});

  ASSERT_NE(vehicle(), nullptr);
  EXPECT_EQ(vehicle()->getProperties(sen::ClassType::SearchMode::doNotIncludeParents).size(), 2U);

  auto colour = sen::dynamicTypeHandleCast<const sen::EnumType>(find("base.Colour").value());
  auto position = sen::dynamicTypeHandleCast<const sen::StructType>(find("base.PositionStruct").value());
  auto shape = sen::dynamicTypeHandleCast<const sen::VariantType>(find("base.ShapeStruct").value());
  ASSERT_TRUE(colour.has_value() && position.has_value() && shape.has_value());

  EXPECT_EQ(colour.value()->getEnums().size(), 2U);
  EXPECT_EQ(position.value()->getFields().size(), 2U);
  EXPECT_EQ(shape.value()->getFields().size(), 2U);
}

//--------------------------------------------------------------------------------------------------------------
// A static attribute, which the constructor takes
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A contributed Static attribute becomes the last constructor parameter, so the existing ones keep their
/// positions.
TEST_F(AFomParser, putsAContributedStaticPropertyLastInTheConstructor)
{
  read({"property_static.xml"});

  ASSERT_NE(vehicle(), nullptr);
  const auto* added = property(*vehicle(), "serial");
  ASSERT_NE(added, nullptr);
  EXPECT_EQ(added->getCategory(), sen::PropertyCategory::staticRW);

  // The generated constructor takes every static read-write property, so a contributed one
  // appends a parameter rather than moving the existing ones.
  const auto* constructor = vehicle()->getConstructor();
  ASSERT_NE(constructor, nullptr);
  const auto args = constructor->getArgs();
  ASSERT_FALSE(args.empty());
  EXPECT_EQ(args[args.size() - 1U].name, "serial");
}

//--------------------------------------------------------------------------------------------------------------
// A type an extension introduces
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A type an extension declares and nothing else does joins the package of the module whose member needs it.
TEST_F(AFomParser, adoptsATypeAnExtensionIntroduces)
{
  read({"introduced_type.xml"});

  // The type takes the package of the module that referenced it, because the generator emits
  // one file per module and a set of its own would have to import that module and be imported
  // by it.
  ASSERT_TRUE(find("base.TrackStruct").has_value());
  EXPECT_EQ(fileOf("base.TrackStruct"), "site.xml");

  ASSERT_NE(vehicle(), nullptr);
  const auto* added = property(*vehicle(), "track");
  ASSERT_NE(added, nullptr);
  EXPECT_EQ(added->getType()->getName(), "TrackStruct");
}

//--------------------------------------------------------------------------------------------------------------
// What is kept once, and what is refused
//--------------------------------------------------------------------------------------------------------------

/// @test
/// Two extensions declaring the same member identically add it once rather than twice.
TEST_F(AFomParser, keepsAnEquivalentRepeatOnce)
{
  read({"property_repeat.xml"});

  // The file repeats Speed exactly and adds Owner beside it. Two properties, not three: the
  // repeat is kept once. The second property is there so that a reader which merged nothing
  // at all would fail this rather than pass it.
  ASSERT_NE(vehicle(), nullptr);
  EXPECT_EQ(vehicle()->getProperties(sen::ClassType::SearchMode::doNotIncludeParents).size(), 2U);
  EXPECT_NE(property(*vehicle(), "owner"), nullptr);
}

/// @test
/// Two declarations of one property with different types are refused, naming both files.
TEST_F(AFomParser, refusesTwoDeclarationsOfOnePropertyThatDisagree)
{
  try
  {
    read({"property_clash.xml"});
    FAIL() << "a property declared twice with a different type was accepted";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("speed"), std::string::npos) << what;
    EXPECT_NE(what.find("do not agree"), std::string::npos) << what;

    // Both files are named, because either one could be the one to change.
    EXPECT_NE(what.find("site.xml"), std::string::npos) << what;
    EXPECT_NE(what.find("property_clash.xml"), std::string::npos) << what;
  }
}

/// @test
/// Two declarations of one enumerator with different values are refused.
TEST_F(AFomParser, refusesTwoDeclarationsOfOneEnumeratorThatDisagree)
{
  try
  {
    read({"enumerator_clash.xml"});
    FAIL() << "an enumerator declared twice with a different value was accepted";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("red"), std::string::npos) << what;
    EXPECT_NE(what.find("do not agree"), std::string::npos) << what;
  }
}

/// @test
/// Two declarations of one enumeration with different representations are refused.
TEST_F(AFomParser, refusesADeclarationThatDisagreesAboutTheTypeHeader)
{
  try
  {
    read({"header_clash.xml"});
    FAIL() << "an enumeration declared twice with a different representation was accepted";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("Colour"), std::string::npos) << what;
    EXPECT_NE(what.find("representation"), std::string::npos) << what;
  }
}

/// @test
/// A file named as an extension that is not an object model is refused rather than ignored.
TEST_F(AFomParser, refusesAnExtensionWhoseRootIsNotAnObjectModel)
{
  try
  {
    read({"not_a_model.xml"});
    FAIL() << "a file that is not an object model was accepted as an extension";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("not_a_model.xml"), std::string::npos) << what;
    EXPECT_NE(what.find("objectModel"), std::string::npos) << what;
  }
}

/// @test
/// A FOM declaring a namespace from a later standard is refused, quoting the namespace it found.
TEST_F(AFomParser, refusesAFomNamingALaterStandard)
{
  try
  {
    readModule("later_dif");
    FAIL() << "a FOM naming a later standard was accepted";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("site.xml"), std::string::npos) << what;

    // The namespace found is quoted back, because the reader keeps no list of other versions and
    // the one in the file is the only thing that tells the user which standard they have.
    EXPECT_NE(what.find("IEEE1516-2025"), std::string::npos) << what;
    EXPECT_NE(what.find("IEEE1516-2010"), std::string::npos) << what;
  }
}

/// @test
/// A FOM with no xmlns, the shape standards before 2010 used, is refused.
TEST_F(AFomParser, refusesAFomThatDeclaresNoNamespace)
{
  try
  {
    readModule("earlier_dif");
    FAIL() << "a FOM with no namespace was accepted";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("site.xml"), std::string::npos) << what;
    EXPECT_NE(what.find("no xmlns"), std::string::npos) << what;
    EXPECT_NE(what.find("IEEE1516-2010"), std::string::npos) << what;
  }
}

/// @test
/// A 2010 FOM whose objectModel is namespace-prefixed is refused rather than silently skipped.
TEST_F(AFomParser, refusesAFomWhoseObjectModelCarriesANamespacePrefix)
{
  // The file is the 2010 DIF and would otherwise be readable. Without this the reader finds no
  // objectModel and a module is skipped in silence.
  try
  {
    readModule("prefixed_dif");
    FAIL() << "a FOM with a prefixed objectModel was accepted";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("site.xml"), std::string::npos) << what;
    EXPECT_NE(what.find("prefix"), std::string::npos) << what;
    EXPECT_NE(what.find("hla"), std::string::npos) << what;
  }
}

/// @test
/// The version check applies to a file named as an extension, not only to a module.
TEST_F(AFomParser, refusesAnExtensionNamingALaterStandard)
{
  try
  {
    read({"later_dif.xml"});
    FAIL() << "an extension naming a later standard was accepted";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("later_dif.xml"), std::string::npos) << what;
    EXPECT_NE(what.find("IEEE1516-2025"), std::string::npos) << what;
  }
}

/// @test
/// An extension naming a class no module declares contributes nothing, and its other contributions still land.
TEST_F(AFomParser, ignoresAnExtensionForAClassNoModuleDeclares)
{
  // Nothing owns Aircraft, so nothing adopts its members: a contribution adds to a complete
  // definition and there is none. The same file contributes to Vehicle, which does land, so
  // this is the reader deciding and not the file going unread.
  read({"unknown_class.xml"});

  EXPECT_FALSE(find("base.Aircraft").has_value());
  ASSERT_NE(vehicle(), nullptr);
  EXPECT_NE(property(*vehicle(), "tail"), nullptr);
  EXPECT_EQ(vehicle()->getProperties(sen::ClassType::SearchMode::doNotIncludeParents).size(), 2U);
}

//--------------------------------------------------------------------------------------------------------------
// How modules compose, which adding members must not change
//--------------------------------------------------------------------------------------------------------------

/// @test
/// Two modules in different packages each declaring one class keep their own.
TEST_F(AFomParser, keepsTwoModulesDeclaringOneClassApart)
{
  // Each package gets its own class with its own members. Merging across packages is a change to
  // how modules compose, and apps/cli_gen/test/test18 and test2 depend on them not merging.
  readModules("two_modules/modulea", "two_modules/moduleb");

  const auto* first = classAt("modulea.Vehicle");
  const auto* second = classAt("moduleb.Vehicle");
  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);

  EXPECT_NE(property(*first, "speed"), nullptr);
  EXPECT_EQ(property(*first, "weight"), nullptr);
  EXPECT_EQ(property(*second, "speed"), nullptr);
  EXPECT_NE(property(*second, "weight"), nullptr);
}

/// @test
/// Two files of one package declaring one class yield a single class.
TEST_F(AFomParser, buildsOneClassWhenTwoFilesOfOnePackageDeclareIt)
{
  // Both files would otherwise build it, and the generated file of each would define the same
  // names in the same namespace, which does not compile. The declaration carrying semantics owns
  // it, and the other one's attribute is added to it rather than dropped.
  readModule("one_package");

  std::size_t declaring = 0;
  for (const auto& typeSet: context)
  {
    for (const auto& type: typeSet.types)
    {
      if (type->getQualifiedName() == "one_package.Vehicle")
      {
        ++declaring;
      }
    }
  }
  EXPECT_EQ(declaring, 1U);

  const auto* vehicle = classAt("one_package.Vehicle");
  ASSERT_NE(vehicle, nullptr);
  EXPECT_NE(property(*vehicle, "speed"), nullptr);
  EXPECT_NE(property(*vehicle, "weight"), nullptr);
}

/// @test
/// The same holds when the second file nests a child under the shared class.
TEST_F(AFomParser, buildsOneClassWhenTheSecondFileAlsoNestsAChild)
{
  // A child reaches its parent through the resolver rather than the class loop, so skipping the
  // loop alone left the second file building the class anyway.
  readModule("one_package_nested");

  std::size_t declaring = 0;
  for (const auto& typeSet: context)
  {
    for (const auto& type: typeSet.types)
    {
      if (type->getQualifiedName() == "one_package_nested.Vehicle")
      {
        ++declaring;
      }
    }
  }
  EXPECT_EQ(declaring, 1U);

  // Each file's own child is there, and both extend the one class.
  EXPECT_TRUE(find("one_package_nested.Car").has_value());
  EXPECT_TRUE(find("one_package_nested.Truck").has_value());
}

/// @test
/// An enumerator value padded with spaces or carrying a leading plus is read as its number.
TEST_F(AFomParser, readsAnEnumeratorValueWrittenWithSpacesOrASign)
{
  // Whitespace and a leading sign are how the XML was written, not part of the value.
  readModule("spaced_enumerator");

  auto kind = sen::dynamicTypeHandleCast<const sen::EnumType>(find("spaced_enumerator.Kind").value());
  ASSERT_TRUE(kind.has_value());

  const auto* spaced = kind.value()->getEnumFromName("spaced");
  const auto* signedKey = kind.value()->getEnumFromName("signed");
  ASSERT_NE(spaced, nullptr);
  ASSERT_NE(signedKey, nullptr);
  EXPECT_EQ(spaced->key, 1U);
  EXPECT_EQ(signedKey->key, 2U);
}

/// @test
/// Two files of one package declaring one record yield a single record.
TEST_F(AFomParser, buildsOneRecordWhenTwoFilesOfOnePackageDeclareIt)
{
  // Same hazard as the class: two documents each building it put the same struct name into one
  // namespace twice, and the generated package does not compile.
  readModule("one_package_record");

  std::size_t declaring = 0;
  for (const auto& typeSet: context)
  {
    for (const auto& type: typeSet.types)
    {
      if (type->getQualifiedName() == "one_package_record.PositionStruct")
      {
        ++declaring;
      }
    }
  }
  EXPECT_EQ(declaring, 1U);
}

/// @test
/// A non-module XML in a module directory produces no type set, so nothing generates a file for it.
TEST_F(AFomParser, givesANonModuleXmlNoTypeSetOfItsOwn)
{
  // It owns nothing, so it generates no file. Without this the back ends write an empty header
  // and source for every stray XML in a module directory.
  readModule("with_mapping");

  for (const auto& typeSet: context)
  {
    EXPECT_NE(typeSet.fileName, "mapping.xml");
    EXPECT_NE(typeSet.fileName, "notafom.xml");
  }
}

/// @test
/// An attribute declared on HLAobjectRoot itself is read.
TEST_F(AFomParser, readsAModuleThatPutsAnAttributeOnTheObjectRoot)
{
  // The MIM declares one there. The root is not a class, so it is not built as one, and the
  // classes under it are unaffected.
  readModule("object_root");

  EXPECT_FALSE(find("object_root.ObjectRoot").has_value());
  const auto* vehicle = classAt("object_root.Vehicle");
  ASSERT_NE(vehicle, nullptr);
  EXPECT_NE(property(*vehicle, "speed"), nullptr);
}

/// @test
/// A mapping or a schema beside the modules is passed over without a refusal.
TEST_F(AFomParser, leavesANonModuleXmlInAModuleDirectoryAlone)
{
  // A mapping or a schema may sit beside the modules. Only a file named as an extension is
  // refused for not being an object model.
  readModule("with_mapping");

  EXPECT_TRUE(find("with_mapping.Vehicle").has_value());
}

//--------------------------------------------------------------------------------------------------------------
// Mappings
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A mapped event becomes an event on the class, without the parameters the mapping ignores.
TEST_F(AFomParser, bindsAMappedEventToTheClass)
{
  readWithMapping("mapped", "mapped_map.xml");

  const auto* vehicle = classAt("mapped.Vehicle");
  ASSERT_NE(vehicle, nullptr);

  const auto* collision = vehicle->searchEventByName("collision");
  ASSERT_NE(collision, nullptr);

  // One of the interaction's two parameters is ignored by the mapping, so only the other arrives.
  ASSERT_EQ(collision->getArgs().size(), 1U);
  EXPECT_EQ(collision->getArgs()[0].name, "closingSpeed");
}

/// @test
/// An event mapped with pack takes one struct argument instead of a parameter list.
TEST_F(AFomParser, packsAMappedEventIntoASingleArgument)
{
  readWithMapping("mapped", "mapped_map.xml");

  const auto* vehicle = classAt("mapped.Vehicle");
  ASSERT_NE(vehicle, nullptr);

  const auto* signal = vehicle->searchEventByName("signal");
  ASSERT_NE(signal, nullptr);

  // pack="true" replaces the parameter list with one struct holding it.
  ASSERT_EQ(signal->getArgs().size(), 1U);
  EXPECT_TRUE(signal->getArgs()[0].type->isStructType());
}

/// @test
/// A property the mapping marks writable becomes read-write; one it does not mention keeps what the FOM
/// implies.
TEST_F(AFomParser, marksAMappedPropertyWritable)
{
  readWithMapping("mapped", "mapped_map.xml");

  const auto* vehicle = classAt("mapped.Vehicle");
  ASSERT_NE(vehicle, nullptr);

  const auto* speed = property(*vehicle, "speed");
  ASSERT_NE(speed, nullptr);
  EXPECT_EQ(speed->getCategory(), sen::PropertyCategory::dynamicRW);

  // An attribute the mapping says nothing about keeps the category the FOM implies.
  const auto* beacon = property(*vehicle, "beacon");
  ASSERT_NE(beacon, nullptr);
  EXPECT_EQ(beacon->getCategory(), sen::PropertyCategory::dynamicRO);
}

/// @test
/// A mapped method returns a struct built from the interaction its return names, and inherits its parent
/// interaction's parameters.
TEST_F(AFomParser, bindsAMappedMethodWhoseReturnIsAnotherInteraction)
{
  readWithMapping("mapped", "mapped_map.xml");

  const auto* vehicle = classAt("mapped.Vehicle");
  ASSERT_NE(vehicle, nullptr);

  const auto* method = vehicle->searchMethodByName("vehicleTransaction");
  ASSERT_NE(method, nullptr);

  // The parent interaction's parameter comes first, then the nested one's, less the ignored one.
  // Reaching the parent means walking up the interaction hierarchy.
  ASSERT_EQ(method->getArgs().size(), 2U);
  EXPECT_EQ(method->getArgs()[0].name, "requestedAt");
  EXPECT_EQ(method->getArgs()[1].name, "distance");

  // The return comes from the interaction the mapping names, as a struct of its parameters less
  // the one it ignores.
  EXPECT_TRUE(method->getReturnType()->isStructType());
}

/// @test
/// A mapped method with a dataType return and local set returns that type and is local only.
TEST_F(AFomParser, bindsAMappedMethodWhoseReturnIsADataType)
{
  readWithMapping("mapped", "mapped_map.xml");

  const auto* vehicle = classAt("mapped.Vehicle");
  ASSERT_NE(vehicle, nullptr);

  const auto* method = vehicle->searchMethodByName("report");
  ASSERT_NE(method, nullptr);
  EXPECT_FALSE(method->getReturnType()->isVoidType());
  EXPECT_TRUE(method->getLocalOnly());
}

/// @test
/// Best effort reads as multicast for an event and unicast for a method, from the same DIF value.
TEST_F(AFomParser, takesTransportFromTheInteractionItMaps)
{
  readWithMapping("mapped", "mapped_map.xml");

  const auto* vehicle = classAt("mapped.Vehicle");
  ASSERT_NE(vehicle, nullptr);

  // An event and a method read the same two DIF values into different modes: best effort is a
  // multicast event but a unicast method.
  const auto* collision = vehicle->searchEventByName("collision");
  ASSERT_NE(collision, nullptr);
  EXPECT_EQ(collision->getTransportMode(), sen::TransportMode::confirmed);

  const auto* signal = vehicle->searchEventByName("signal");
  ASSERT_NE(signal, nullptr);
  EXPECT_EQ(signal->getTransportMode(), sen::TransportMode::multicast);

  const auto* method = vehicle->searchMethodByName("vehicleTransaction");
  ASSERT_NE(method, nullptr);
  EXPECT_EQ(method->getTransportMode(), sen::TransportMode::unicast);
}

/// @test
/// A property's transport comes from its attribute's transportation.
TEST_F(AFomParser, takesTransportFromTheAttribute)
{
  readWithMapping("mapped", "mapped_map.xml");

  const auto* vehicle = classAt("mapped.Vehicle");
  ASSERT_NE(vehicle, nullptr);

  const auto* speed = property(*vehicle, "speed");
  ASSERT_NE(speed, nullptr);
  EXPECT_EQ(speed->getTransportMode(), sen::TransportMode::confirmed);

  const auto* beacon = property(*vehicle, "beacon");
  ASSERT_NE(beacon, nullptr);
  EXPECT_EQ(beacon->getTransportMode(), sen::TransportMode::multicast);
}

/// @test
/// An attribute whose semantics begin with Optional becomes an optional type.
TEST_F(AFomParser, readsAnAttributeMarkedOptionalAsAnOptionalType)
{
  readWithMapping("mapped", "mapped_map.xml");

  const auto* vehicle = classAt("mapped.Vehicle");
  ASSERT_NE(vehicle, nullptr);

  // The DIF has no optional, so the reader takes semantics beginning "Optional" as one.
  const auto* cargo = property(*vehicle, "cargo");
  ASSERT_NE(cargo, nullptr);
  EXPECT_TRUE(cargo->getType()->isOptionalType());
}

/// @test
/// A mapping naming an interaction no module declares is refused, naming the interaction and the class.
TEST_F(AFomParser, refusesAMappingForAnInteractionNoModuleDeclares)
{
  try
  {
    readWithMapping("mapped", "unmapped_interaction_map.xml");
    FAIL() << "a mapping naming an interaction no module declares was accepted";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();

    // Both halves are needed to find it: the mapping file names the interaction, and the class is
    // where the mapping sits.
    EXPECT_NE(what.find("Transaction.NoSuchTransaction"), std::string::npos) << what;
    EXPECT_NE(what.find("Vehicle"), std::string::npos) << what;
  }
}

//--------------------------------------------------------------------------------------------------------------
// The union over several modules
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A module in another package does not add its members to a class this one defines. The boundary
/// is the package: within one, files merge; across, each keeps its own.
TEST_F(AFomParser, doesNotTakeMembersFromAModuleOfAnotherPackage)
{
  readModules("union_a", "union_b");

  const auto* defining = classAt("union_a.Vehicle");
  ASSERT_NE(defining, nullptr);
  EXPECT_NE(property(*defining, "speed"), nullptr);
  EXPECT_EQ(property(*defining, "weight"), nullptr);

  // union_b carries an attribute and no semantics, so it is a contribution. Across packages it is
  // not applied, and the attribute stays in a class of union_b's own.
  const auto* other = classAt("union_b.Vehicle");
  ASSERT_NE(other, nullptr);
  EXPECT_NE(property(*other, "weight"), nullptr);
}

/// @test
/// The defining declaration owns the class even when it is read last, so the model does not depend
/// on the order the files come back in.
TEST_F(AFomParser, takesTheOwnerFromSemanticsRatherThanReadOrder)
{
  readModule("late_definition");

  const auto* vehicle = classAt("late_definition.Vehicle");
  ASSERT_NE(vehicle, nullptr);
  EXPECT_NE(property(*vehicle, "speed"), nullptr);
  EXPECT_NE(property(*vehicle, "weight"), nullptr);

  // aaa.xml is read first and carries no semantics, so this description can only have come from
  // zzz.xml being chosen as the defining declaration.
  EXPECT_EQ(vehicle->getDescription(), "The defining declaration, read last.");
}

//--------------------------------------------------------------------------------------------------------------
// Interaction parameters
//--------------------------------------------------------------------------------------------------------------

/// @test
/// An extension adds a parameter to an interaction the module declares, and it arrives after the
/// parameters the module declares.
TEST_F(AFomParser, addsAParameterToAnInteraction)
{
  readWithMappingAndExtensions("mapped", "mapped_map.xml", {"collision_param.xml"});

  const auto* vehicle = classAt("mapped.Vehicle");
  ASSERT_NE(vehicle, nullptr);

  const auto* collision = vehicle->searchEventByName("collision");
  ASSERT_NE(collision, nullptr);

  // closingSpeed is the module's, eventIdentifier is ignored by the mapping, and the contributed
  // one comes last so the existing positions do not move.
  ASSERT_EQ(collision->getArgs().size(), 2U);
  EXPECT_EQ(collision->getArgs()[0].name, "closingSpeed");
  EXPECT_EQ(collision->getArgs()[1].name, "siteImpactForce");
}

/// @test
/// A packed callable that ignores a parameter leaves it out, and the struct it carries keeps the
/// interaction's name. The one holding every declared parameter takes a Full suffix.
TEST_F(AFomParser, leavesAnIgnoredParameterOutOfAPackedCallable)
{
  readWithMapping("mapped", "mapped_map.xml");

  // What the event carries, under the interaction's own name: the ignored parameter is gone.
  auto carried = find("mapped.Signal");
  ASSERT_TRUE(carried.has_value());
  auto asCarried = sen::dynamicTypeHandleCast<const sen::StructType>(carried.value());
  ASSERT_TRUE(asCarried.has_value());
  ASSERT_EQ(asCarried.value()->getFields().size(), 1U);
  EXPECT_EQ(asCarried.value()->getFields()[0].name, "strength");

  // And every parameter the FOM declares, kept under the suffixed name.
  auto complete = find("mapped.SignalFull");
  ASSERT_TRUE(complete.has_value());
  auto asComplete = sen::dynamicTypeHandleCast<const sen::StructType>(complete.value());
  ASSERT_TRUE(asComplete.has_value());
  EXPECT_EQ(asComplete.value()->getFields().size(), 2U);

  const auto* signal = classAt("mapped.Vehicle")->searchEventByName("signal");
  ASSERT_NE(signal, nullptr);
  ASSERT_EQ(signal->getArgs().size(), 1U);
  EXPECT_EQ(signal->getArgs()[0].type->getName(), "Signal");

  // An interaction no mapping packs keeps its single struct and no suffixed twin.
  EXPECT_TRUE(find("mapped.Report").has_value());
  EXPECT_FALSE(find("mapped.ReportFull").has_value());
}

//--------------------------------------------------------------------------------------------------------------
// Types the standard names
//--------------------------------------------------------------------------------------------------------------

/// @test
/// The standard's own type names resolve to Sen's types rather than to types of the module.
TEST_F(AFomParser, mapsTheTypeNamesTheStandardDefines)
{
  readModule("hla_types");

  const auto* reading = classAt("hla_types.Reading");
  ASSERT_NE(reading, nullptr);

  // Each of these is a name from the standard that Sen answers with one of its own types rather
  // than a type of the module's.
  for (const auto* name:
       {"letter", "wideLetter", "raw", "tally", "elapsed", "elapsedFine", "position", "whenWhole", "whenReal"})
  {
    const auto* attribute = property(*reading, name);
    ASSERT_NE(attribute, nullptr) << name;
    EXPECT_FALSE(attribute->getType()->isCustomType()) << name;
  }
}

/// @test
/// A simple data type with a unit the unit map knows becomes a quantity.
TEST_F(AFomParser, readsASimpleDataWithAUnitAsAQuantity)
{
  readModule("quantity");

  const auto* vehicle = classAt("quantity.Vehicle");
  ASSERT_NE(vehicle, nullptr);

  // A unit the unit map knows makes a quantity. Without one the same declaration is an alias.
  const auto* speed = property(*vehicle, "speed");
  ASSERT_NE(speed, nullptr);
  EXPECT_TRUE(speed->getType()->isQuantityType());
}

/// @test
/// A record that includes another takes it as its parent and keeps only its own fields.
TEST_F(AFomParser, readsARecordThatIncludesAnother)
{
  readModule("record_include");

  auto handle = find("record_include.OrientedPositionStruct");
  ASSERT_TRUE(handle.has_value());
  auto asStruct = sen::dynamicTypeHandleCast<const sen::StructType>(handle.value());
  ASSERT_TRUE(asStruct.has_value());

  // The included record becomes the parent, which is the one shape of record inheritance Sen has.
  const auto parent = asStruct.value()->getParent();
  ASSERT_TRUE(parent.has_value());
  EXPECT_EQ(parent.value()->getName(), "PositionStruct");

  const auto fields = asStruct.value()->getFields();
  ASSERT_EQ(fields.size(), 1U);
  EXPECT_EQ(fields[0].name, "heading");
}

/// @test
/// A mapping boolean that is neither true nor false is refused, naming the attribute.
TEST_F(AFomParser, refusesAMappingValueThatIsNeitherTrueNorFalse)
{
  try
  {
    readWithMapping("mapped", "bad_boolean_map.xml");
    FAIL() << "a mapping with a non-boolean writable was accepted";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("maybe"), std::string::npos) << what;

    // Which attribute it was, so it can be found in the mapping file.
    EXPECT_NE(what.find("writable"), std::string::npos) << what;
  }
}

//--------------------------------------------------------------------------------------------------------------
// Dependencies between modules
//--------------------------------------------------------------------------------------------------------------

/// @test
/// An interaction in a module named as a dependency is found by following the reference.
TEST_F(AFomParser, findsAMappedInteractionInADependency)
{
  // The class is in one module and the interaction it is mapped onto is in the module that one
  // names as a dependency, so the only way to the interaction is through the reference.
  readWithMapping("deps", "deps_map.xml");

  const auto* station = classAt("deps.Station");
  ASSERT_NE(station, nullptr);

  const auto* handover = station->searchEventByName("handover");
  ASSERT_NE(handover, nullptr);
  ASSERT_EQ(handover->getArgs().size(), 1U);
  EXPECT_EQ(handover->getArgs()[0].name, "recipient");
}

/// @test
/// A dependency reference no module in the set satisfies is refused, naming both.
TEST_F(AFomParser, refusesADependencyNoModuleProvides)
{
  try
  {
    readModule("bad_dep");
    FAIL() << "a reference to a module that is not in the set was accepted";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("A Module That Is Not Here"), std::string::npos) << what;
    EXPECT_NE(what.find("site.xml"), std::string::npos) << what;
  }
}

//--------------------------------------------------------------------------------------------------------------
// Values the DIF constrains
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A misspelt dataType is refused, naming the type and the file that referenced it.
TEST_F(AFomParser, refusesADataTypeNoModuleDeclares)
{
  // A misspelt dataType is the commonest way here, so the message has to name both the name that
  // was not found and the file that asked for it.
  try
  {
    readModule("unknown_type");
    FAIL() << "an attribute naming a type no module declares was accepted";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("PostionStruct"), std::string::npos) << what;
    EXPECT_NE(what.find("site.xml"), std::string::npos) << what;
  }
}

/// @test
/// A record with a field of its own type is refused as a circular reference.
TEST_F(AFomParser, refusesARecordThatNamesItself)
{
  try
  {
    readModule("circular_record");
    FAIL() << "a record with a field of its own type was accepted";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("ChainStruct"), std::string::npos) << what;
    EXPECT_NE(what.find("circular"), std::string::npos) << what;
    EXPECT_NE(what.find("site.xml"), std::string::npos) << what;
  }
}

/// @test
/// A basic data representation with an encoding the reader has no type for is refused.
TEST_F(AFomParser, refusesABasicDataEncodingItHasNoTypeFor)
{
  try
  {
    readModule("bad_encoding");
    FAIL() << "a basic data representation with an unknown encoding was accepted";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("36-bit gray code"), std::string::npos) << what;
    EXPECT_NE(what.find("SiteUnsigned8"), std::string::npos) << what;
  }
}

/// @test
/// An attribute with a transportation outside the DIF's two is refused.
TEST_F(AFomParser, refusesATransportationTheDifDoesNotDefine)
{
  try
  {
    readModule("bad_transport");
    FAIL() << "an attribute with an unknown transportation was accepted";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("HLAwhenever"), std::string::npos) << what;
  }
}

/// @test
/// A Static attribute with a sharing mode outside the DIF's four is refused.
TEST_F(AFomParser, refusesASharingModeTheDifDoesNotDefine)
{
  try
  {
    readModule("bad_sharing");
    FAIL() << "an attribute with an unknown sharing mode was accepted";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("PublishOnAlternateTuesdays"), std::string::npos) << what;
  }
}

//--------------------------------------------------------------------------------------------------------------
// Enumerator values
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A negative enumerator value is kept as the unsigned pattern of its two's complement.
TEST_F(AFomParser, keepsANegativeEnumeratorValueAsItsUnsignedPattern)
{
  // FOMs write `Other = -1`, and an enumerator key is unsigned here.
  readModule("negative_enumerator");

  auto kind = sen::dynamicTypeHandleCast<const sen::EnumType>(find("negative_enumerator.Kind").value());
  ASSERT_TRUE(kind.has_value());

  const auto* other = kind.value()->getEnumFromName("other");
  ASSERT_NE(other, nullptr);
  EXPECT_EQ(other->key, 4294967295U);
}

/// @test
/// An enumerator value outside its representation's range is refused.
TEST_F(AFomParser, refusesAnEnumeratorValueItsRepresentationCannotHold)
{
  try
  {
    readModule("huge_enumerator");
    FAIL() << "an enumerator value above the 32-bit range was accepted";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("4294967296"), std::string::npos) << what;
    EXPECT_NE(what.find("HLAinteger32BE"), std::string::npos) << what;
  }
}

/// @test
/// The range check is against the declared representation, not against 32 bits.
TEST_F(AFomParser, refusesAnEnumeratorThatOverflowsASmallRepresentation)
{
  // 300 fits in 32 bits and not in an octet. Bounding only against 32 bits let it reach the
  // generated code as an out-of-range enumerator, which the compiler then refused.
  try
  {
    readModule("octet_enumerator");
    FAIL() << "an octet enumeration took a value of 300";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("300"), std::string::npos) << what;
    EXPECT_NE(what.find("HLAoctet"), std::string::npos) << what;
  }
}

//--------------------------------------------------------------------------------------------------------------
// More than one extension, and the order they are given in
//--------------------------------------------------------------------------------------------------------------

/// @test
/// Two extensions each adding a property to one class both land.
TEST_F(AFomParser, takesPropertiesFromTwoExtensionsForOneClass)
{
  read({"property.xml", "property_second.xml"});

  ASSERT_NE(vehicle(), nullptr);
  const auto properties = vehicle()->getProperties(sen::ClassType::SearchMode::doNotIncludeParents);
  ASSERT_EQ(properties.size(), 3U);
  EXPECT_EQ(properties[0]->getName(), "speed");
  EXPECT_EQ(properties[1]->getName(), "callsign");
  EXPECT_EQ(properties[2]->getName(), "driver");
}

/// @test
/// Contributed fields follow the order the extensions were given on the command line.
TEST_F(AFomParser, ordersContributedFieldsByTheOrderTheExtensionsWereGiven)
{
  // A struct is serialized in field order, so the order the extensions are given in decides the
  // bytes on the wire. Pinned in both directions rather than left for a user to discover.
  read({"field.xml", "field_second.xml"});

  auto position = sen::dynamicTypeHandleCast<const sen::StructType>(find("base.PositionStruct").value());
  ASSERT_TRUE(position.has_value());
  const auto fields = position.value()->getFields();
  ASSERT_EQ(fields.size(), 3U);
  EXPECT_EQ(fields[1].name, "y");
  EXPECT_EQ(fields[2].name, "z");
}

/// @test
/// The same two extensions in the other order give the other field order.
TEST_F(AFomParser, ordersContributedFieldsTheOtherWayWhenGivenTheOtherWay)
{
  read({"field_second.xml", "field.xml"});

  auto position = sen::dynamicTypeHandleCast<const sen::StructType>(find("base.PositionStruct").value());
  ASSERT_TRUE(position.has_value());
  const auto fields = position.value()->getFields();
  ASSERT_EQ(fields.size(), 3U);
  EXPECT_EQ(fields[1].name, "z");
  EXPECT_EQ(fields[2].name, "y");
}

/// @test
/// A clash between two extensions names both files, since either could be the one to change.
TEST_F(AFomParser, namesBothExtensionsWhenTwoOfThemDisagree)
{
  // The base declares no Tag, so naming the document being built would name a file that never
  // mentions the member. The two files that disagree are the two extensions.
  try
  {
    read({"clash_a.xml", "clash_b.xml"});
    FAIL() << "two extensions declared one property with different types and were accepted";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("tag"), std::string::npos) << what;
    EXPECT_NE(what.find("clash_a.xml"), std::string::npos) << what;
    EXPECT_NE(what.find("clash_b.xml"), std::string::npos) << what;
    EXPECT_EQ(what.find("site.xml"), std::string::npos) << what;
  }
}

/// @test
/// Two extensions naming one variant alternative are refused, because an alternative is keyed by position.
TEST_F(AFomParser, refusesTwoExtensionsContributingOneAlternative)
{
  try
  {
    read({"alternative.xml", "alternative_again.xml"});
    FAIL() << "one alternative was contributed twice and both were kept";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("Line"), std::string::npos) << what;
  }
}

/// @test
/// A file named as an extension is read as one even when it sits in the module directory.
TEST_F(AFomParser, readsAnExtensionSittingInsideTheModuleDirectoryAsAnExtension)
{
  // Putting the extension beside the modules is the natural thing to try. Read as a module it
  // would declare a second Vehicle in that package instead of adding a member to the first.
  context = sen::lang::parseFomDocuments({fomData() / "extension_inside"},
                                         {fomData() / "extension_inside" / "property.xml"},
                                         {},
                                         sen::lang::TypeSettings {});

  const auto* vehicle = classAt("extension_inside.Vehicle");
  ASSERT_NE(vehicle, nullptr);
  EXPECT_NE(property(*vehicle, "callsign"), nullptr);

  // One type set for the module, and none for the extension.
  for (const auto& typeSet: context)
  {
    EXPECT_NE(typeSet.fileName, "property.xml");
  }
}

/// @test
/// A directory given to the extensions input contributes every object model in it.
TEST_F(AFomParser, readsEveryExtensionInADirectory)
{
  // A directory is accepted where a file is, and nothing else takes that branch.
  context =
    sen::lang::parseFomDocuments({fomData() / "base"}, {fomData() / "extension_dir"}, {}, sen::lang::TypeSettings {});

  ASSERT_NE(vehicle(), nullptr);
  EXPECT_NE(property(*vehicle(), "callsign"), nullptr);

  auto colour = sen::dynamicTypeHandleCast<const sen::EnumType>(find("base.Colour").value());
  ASSERT_TRUE(colour.has_value());
  EXPECT_EQ(colour.value()->getEnums().size(), 2U);
}

/// @test
/// An extension produces no type set, so nothing generates a file for it.
TEST_F(AFomParser, givesAnExtensionNoTypeSetOfItsOwn)
{
  // An extension owns nothing, so it generates no file. Without this the cpp back end writes an
  // empty header per extension and nothing notices.
  read({"property.xml"});

  for (const auto& typeSet: context)
  {
    EXPECT_NE(typeSet.fileName, "property.xml");
  }
}

//--------------------------------------------------------------------------------------------------------------
// The other two type headers a contribution must not disagree about
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A contribution disagreeing about a record's include is refused.
TEST_F(AFomParser, refusesAContributionThatDisagreesAboutARecordInclude)
{
  try
  {
    read({"include_clash.xml"});
    FAIL() << "a record declared twice with a different include was accepted";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    // "declared twice" is what separates this from the fallback refusal, which also names the
    // record and the word include and so satisfied the other two assertions on its own.
    EXPECT_NE(what.find("PositionStruct"), std::string::npos) << what;
    EXPECT_NE(what.find("include"), std::string::npos) << what;
    EXPECT_NE(what.find("declared twice"), std::string::npos) << what;
  }
}

/// @test
/// A contribution disagreeing about a variant's discriminant is refused.
TEST_F(AFomParser, refusesAContributionThatDisagreesAboutAVariantDiscriminant)
{
  try
  {
    read({"discriminant_clash.xml"});
    FAIL() << "a variant declared twice with a different discriminant was accepted";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("ShapeStruct"), std::string::npos) << what;
    EXPECT_NE(what.find("discriminant"), std::string::npos) << what;
  }
}

//--------------------------------------------------------------------------------------------------------------
// Shapes the bundled FOM modules do not have
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A class node with members but no semantics is read as a contribution rather than a definition.
TEST_F(AFomParser, readsAClassNodeThatCarriesMembersAndNoSemantics)
{
  // An extension has this shape, so semantics alone cannot be what makes a node a declaration.
  readModule("members_only");

  auto handle = find("members_only.Beacon");
  ASSERT_TRUE(handle.has_value());
  auto asClass = sen::dynamicTypeHandleCast<const sen::ClassType>(handle.value());
  ASSERT_TRUE(asClass.has_value());
  EXPECT_EQ(asClass.value()->getProperties(sen::ClassType::SearchMode::doNotIncludeParents).size(), 1U);
}

/// @test
/// A cardinality given as a count and the same one given as a range cap to the same bound.
TEST_F(AFomParser, capsACardinalityTheSameWayWhetherItIsACountOrARange)
{
  readModule("cardinality");

  auto maxSizeOf = [this](std::string_view name) -> std::optional<std::size_t>
  {
    auto handle = find(name);
    if (!handle)
    {
      return std::nullopt;
    }
    auto asSequence = sen::dynamicTypeHandleCast<const sen::SequenceType>(handle.value());
    return asSequence ? asSequence.value()->getMaxSize() : std::nullopt;
  };

  EXPECT_EQ(maxSizeOf("cardinality.SmallCountArray"), std::optional<std::size_t> {8U});
  EXPECT_EQ(maxSizeOf("cardinality.SmallRangeArray"), std::optional<std::size_t> {8U});

  // Above the cap the sequence is unbounded, and a count and a range say the same thing.
  EXPECT_FALSE(maxSizeOf("cardinality.HugeCountArray").has_value());
  EXPECT_FALSE(maxSizeOf("cardinality.HugeRangeArray").has_value());
}

/// @test
/// An enumerator value that is not a number at all is refused.
TEST_F(AFomParser, refusesAnEnumeratorValueThatIsNotANumber)
{
  try
  {
    readModule("bad_enumerator");
    FAIL() << "an enumerator whose value is not a number was accepted";
  }
  catch (const std::exception& err)
  {
    const std::string what = err.what();
    EXPECT_NE(what.find("Nonsense"), std::string::npos) << what;
    EXPECT_NE(what.find("twelve"), std::string::npos) << what;
  }
}

}  // namespace
