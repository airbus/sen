// === fom_parser_test.cpp
// ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/lang/fom_parser.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type.h"

// google test
#include <gtest/gtest-param-test.h>
#include <gtest/gtest.h>

// std
#include <algorithm>
#include <array>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iterator>
#include <map>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

/// Root of the copied FOM test fixtures, relative to the test working directory.
const std::filesystem::path fomFixtureDir {"bin/lang/fom"};

/// Builds a map of type-name -> type for every custom type in the produced type sets.
std::map<std::string, const sen::Type*> collectTypes(const sen::lang::TypeSetContext& context)
{
  std::map<std::string, const sen::Type*> result;
  for (const auto& typeSet: context)
  {
    for (const auto& type: typeSet.types)
    {
      result.emplace(type->getName(), type.type());
    }
  }
  return result;
}

/// Finds the first class type with the given simple name in the produced type sets.
const sen::ClassType* findClass(const sen::lang::TypeSetContext& context, std::string_view name)
{
  for (const auto& typeSet: context)
  {
    for (const auto& type: typeSet.types)
    {
      if (type->isClassType() && type->asClassType()->getName() == name)
      {
        return type->asClassType();
      }
    }
  }
  return nullptr;
}

/// Builds a map of property-name -> property for a class, considering only properties declared on that class.
auto collectProperties(const sen::ClassType& classType) -> std::map<std::string_view, const sen::Property*>
{
  std::map<std::string_view, const sen::Property*> result;
  auto properties = classType.getProperties(sen::ClassType::SearchMode::doNotIncludeParents);
  std::transform(properties.begin(),
                 properties.end(),
                 std::inserter(result, result.end()),
                 [](auto property) { return std::make_pair(property->getName(), property.get()); });
  return result;
}

/// @test
/// Verifies that a FOM declaring every kind of HLA type produces the corresponding Sen custom types
/// @requirements(TBD)
TEST(FomParser, ParsesEveryTypePossibleInAnHlaFom)
{
  using sen::Type;
  sen::lang::TypeSetContext context {};
  ASSERT_NO_THROW(context = sen::lang::parseFomDocuments({fomFixtureDir / "all_types"}, {}, {}));

  // assert: each kind of HLA type is mapped to the expected Sen custom type. Note that the dynamic
  // char array TextBuffer is also parsed, but string is a native type and therefore never shows up
  // in the produced TypeSets (which only hold custom types).
  const auto types = collectTypes(context);

  static const std::array<std::pair<std::string, decltype(&Type::isAliasType)>, 7> typeChecks {{
    {"PlainIdent", &Type::isAliasType},      // simple data without units -> alias
    {"LengthMetre", &Type::isQuantityType},  // simple data with units -> quantity
    {"PointList", &Type::isSequenceType},    // dynamic array -> sequence
    {"GeodeticPoint", &Type::isStructType},  // fixed record -> struct
    {"ShapeOrSize", &Type::isVariantType},   // variant record -> variant
    {"State", &Type::isEnumType},            // enumerated data -> enum
    {"Entity", &Type::isClassType},          // object class -> class
  }};

  for (const auto& [typeName, method]: typeChecks)
  {
    const auto itr = types.find(typeName);
    ASSERT_NE(itr, types.end()) << "missing type '" << typeName << "'";
    const auto& type = *itr->second;
    auto result = (type.*method)();
    EXPECT_TRUE(result) << "type '" << typeName << "' has an unexpected kind: '" << type.getName() << "'";
  }
}

/// @test
/// Verifies that every supported basic data encoding maps to its corresponding native integer type
/// @requirements(TBD)
TEST(FomParser, ParsesAllBasicDataRepresentations)
{
  using sen::Type;
  sen::lang::TypeSetContext context {};
  ASSERT_NO_THROW(context = sen::lang::parseFomDocuments({fomFixtureDir / "basic_data_representations"}, {}, {}));

  // assert: each basic data encoding maps the simple-data alias to the expected native integer type
  const auto* entityClass = findClass(context, "Entity");
  ASSERT_NE(entityClass, nullptr);
  const auto props = collectProperties(*entityClass);

  static constexpr std::array<std::pair<std::string_view, decltype(&Type::isUInt8Type)>, 7> propertyChecks {{
    {"u8Data", &Type::isUInt8Type},    // 8-bit unsigned -> u8
    {"u16Data", &Type::isUInt16Type},  // 16-bit unsigned -> u16
    {"u32Data", &Type::isUInt32Type},  // 32-bit unsigned -> u32
    {"u64Data", &Type::isUInt64Type},  // 64-bit unsigned -> u64
    {"i16Data", &Type::isInt16Type},   // 16-bit signed -> i16
    {"i32Data", &Type::isInt32Type},   // 32-bit signed -> i32
    {"i64Data", &Type::isInt64Type},   // 64-bit signed -> i64
  }};

  for (const auto& [propName, method]: propertyChecks)
  {
    const auto itr = props.find(propName);
    ASSERT_NE(itr, props.end()) << "missing property '" << propName << "'";
    const auto& type = *itr->second->getType();
    ASSERT_TRUE(type.isAliasType()) << "property '" << propName << "' should be a simple-data alias";
    const auto& aliased = *type.asAliasType()->getAliasedType();
    auto result = (aliased.*method)();
    EXPECT_TRUE(result) << "property '" << propName << "' has an unexpected type: '" << aliased.getName() << "'";
  }
}

/// @test
/// Verifies that every unit string in stringToSenUnitAbbrMap_ produces a QuantityType with the expected unit
/// abbreviation
/// @requirements(TBD)
TEST(FomParser, ParsesAllSupportedUnitStrings)
{
  using sen::Type;
  sen::lang::TypeSetContext context {};
  ASSERT_NO_THROW(context = sen::lang::parseFomDocuments({fomFixtureDir / "units"}, {}, {}));

  const auto types = collectTypes(context);

  static constexpr std::array<std::pair<std::string_view, std::string_view>, 18> quantityChecks {{
    {"AccelQ", "m_per_s_sq"},   // meter per second squared (m/(s^2))
    {"DegQ", "deg"},            // degree (deg)
    {"RadQ", "rad"},            // radian (rad)
    {"RadPerSQ", "rad_per_s"},  // radian per second (rad/s)
    {"MeterQ", "m"},            // meter (m)
    {"HzQ", "hz"},              // hertz (Hz)
    {"InterrogationQ", "hz"},   // interrogations/second
    {"KgQ", "kg"},              // kilogram (kg)
    {"RpmRevQ", "rpm"},         // revolutions per minute (RPM)
    {"DegCQ", "degC"},          // degree Celsius (C)
    {"MicrosQ", "us"},          // microsecond
    {"MillisQ", "ms"},          // millisecond (ms)
    {"HourQ", "hour"},          // hour
    {"SecQ", "s"},              // second (s)
    {"MeterPerSQ", "m_per_s"},  // meter per second (m/s)
    {"MicronQ", "um"},          // micron
    {"RpmQ", "rpm"},            // RPM
    {"DmPerSQ", "dm_per_s"},    // decimeter per second (dm/s)
  }};

  for (const auto& [typeName, expectedAbbrev]: quantityChecks)
  {
    const auto itr = types.find(std::string(typeName));
    ASSERT_NE(itr, types.end()) << "missing quantity type '" << typeName << "'";
    const auto& type = *itr->second;
    ASSERT_TRUE(type.isQuantityType()) << "type '" << typeName << "' should be a QuantityType";
    const auto unit = type.asQuantityType()->getUnit();
    ASSERT_TRUE(unit.has_value()) << "quantity '" << typeName << "' should have a unit";
    EXPECT_EQ(unit.value()->getAbbreviation(), expectedAbbrev)
      << "quantity '" << typeName << "' has unexpected unit abbreviation: '" << unit.value()->getAbbreviation() << "'";
  }
}

/// @test
/// Verifies every name adaptation applied to HLA attribute and interaction parameter names (lower camel casing,
/// reserved-word and reserved-property-name suffixing), and that the adapted names replace the originals
/// @requirements(TBD)
TEST(FomParser, AdaptsHlaNamesForSen)
{
  sen::lang::TypeSetContext context {};
  ASSERT_NO_THROW(context = sen::lang::parseFomDocuments({fomFixtureDir / "name_adaptations"}, {}, {}));

  // assert: attribute names are lower camel cased into properties
  const auto* entityClass = findClass(context, "Entity");
  ASSERT_NE(entityClass, nullptr);
  const auto props = collectProperties(*entityClass);

  static constexpr std::array<std::string_view, 19> expectedProperties {{
    "myVar",                  // MY_VAR: all caps lowered, words joined
    "dashVar",                // dash-var: dash split, first letter of each word uppercased then overall lowercased
    "aB",                     // A__B: empty word from consecutive separators is skipped
    "n123abc",                // 123abc: leading digit prefixed with 'n'
    "classVal",               // class: reserved word suffixed with 'Val'
    "trueVal",                // TRUE: all caps reserved word lowercased and suffixed
    "mixedCase",              // MixedCase: first letter lowercased
    "allcaps",                // ALLCAPS: all upper lowered
    "snakeCaseWord",          // snake_case_word: multiple words joined
    "namespaceVal",           // namespace: reserved word suffixed with 'Val'
    "templateVal",            // template: reserved word suffixed with 'Val'
    "operatorVal",            // operator: reserved word suffixed with 'Val'
    "xorVal",                 // xor: alternative token suffixed with 'Val'
    "coAwait",                // co_await: C++20 keyword, underscore split, first letter lowercased
    "nameNonSen",             // name: reserved property name suffixed with 'NonSen'
    "idNonSen",               // id: reserved property name suffixed with 'NonSen'
    "localNameNonSen",        // localName: reserved property name suffixed with 'NonSen'
    "lastCommitTimeNonSen",   // lastCommitTime: reserved property name suffixed with 'NonSen'
    "propertyUntypedNonSen",  // propertyUntyped: reserved property name suffixed with 'NonSen'
  }};

  for (const auto& expected: expectedProperties)
  {
    EXPECT_NE(props.find(expected), props.end()) << "missing property '" << expected << "'";
  }

  // assert: the raw, un-adapted names are not exposed, so they cannot collide with the sanitized ones
  EXPECT_EQ(props.size(), expectedProperties.size());

  // assert: interaction parameter names are lower camel cased into struct fields
  const auto types = collectTypes(context);
  const auto reportItr = types.find("CamelReport");
  ASSERT_NE(reportItr, types.end());
  ASSERT_TRUE(reportItr->second->isStructType());
  const auto fields = reportItr->second->asStructType()->getFields();

  static constexpr std::array<std::string_view, 5> expectedFields {{
    "paramName",  // ParamName
    "upperCase",  // UPPER_CASE
    "dashParam",  // dash-param
    "n123Field",  // 123Field
    "simple",     // simple
  }};

  std::array<std::string_view, 5> actualFields {};
  ASSERT_EQ(fields.size(), expectedFields.size());
  std::transform(
    fields.begin(), fields.end(), actualFields.begin(), [](const auto& field) { return std::string_view(field.name); });
  EXPECT_TRUE(std::equal(actualFields.begin(), actualFields.end(), expectedFields.begin()));
}

/// @test
/// Verifies that the special HLA type names are mapped to the expected native Sen types, and that bounded
/// sequences and duplicated enumerators are handled correctly
/// @requirements(TBD)
TEST(FomParser, MapsSpecialHlaTypeNamesAndEnumerationDetails)
{
  using sen::Type;
  sen::lang::TypeSetContext context {};
  ASSERT_NO_THROW(context = sen::lang::parseFomDocuments({fomFixtureDir / "special_names"}, {}, {}));

  // assert: the special HLA data type names are mapped to the expected native Sen types
  const auto* entityClass = findClass(context, "Entity");
  ASSERT_NE(entityClass, nullptr);
  const auto props = collectProperties(*entityClass);

  static constexpr std::array<std::pair<std::string_view, decltype(&Type::isUInt8Type)>, 15> propertyChecks {{
    {"anAsciiChar", &Type::isUInt8Type},          // HLAASCIIchar -> u8
    {"anUnicodeChar", &Type::isUInt16Type},       // HLAunicodeChar -> u16
    {"aByte", &Type::isUInt8Type},                // HLAbyte -> u8
    {"aCount", &Type::isInt32Type},               // HLAcount -> i32
    {"aSeconds", &Type::isInt32Type},             // HLAseconds -> i32
    {"aMsec", &Type::isInt32Type},                // HLAmsec -> i32
    {"anIndex", &Type::isInt32Type},              // HLAindex -> i32
    {"a64BitTime", &Type::isInt64Type},           // HLAinteger64Time -> i64
    {"aFloatTime", &Type::isFloat64Type},         // HLAfloat64Time -> f64
    {"aBoolean", &Type::isBoolType},              // HLAboolean -> bool
    {"anAsciiString", &Type::isStringType},       // HLAASCIIstring -> string
    {"anUnicodeString", &Type::isStringType},     // HLAunicodeString -> string
    {"aBoundedIntegers", &Type::isSequenceType},  // BoundedIntegers -> sequence
    {"aState", &Type::isEnumType},                // State -> enum
    {"anRprBoolean", &Type::isBoolType},          // RPRboolean -> bool
  }};

  for (const auto& [propName, method]: propertyChecks)
  {
    const auto itr = props.find(propName);
    ASSERT_NE(itr, props.end()) << "missing property '" << propName << "'";
    const auto& type = *itr->second->getType();
    auto result = (type.*method)();
    EXPECT_TRUE(result) << "property '" << propName << "' has an unexpected type: '" << type.getName() << "'";
  }

  // assert: the bounded sequence has the expected maximum size
  const auto& bounded = props.find("aBoundedIntegers");
  EXPECT_EQ(bounded->second->getType()->asSequenceType()->getMaxSize(), std::optional<std::size_t>(5U));

  // assert: duplicated enumerator names are disambiguated with a numeric suffix
  const auto* enumType = props.find("aState")->second->getType()->asEnumType();
  ASSERT_NE(enumType->getEnumFromName("value"), nullptr);
  ASSERT_NE(enumType->getEnumFromName("value1"), nullptr);
  EXPECT_EQ(enumType->getEnumFromName("value")->key, 0U);
  EXPECT_EQ(enumType->getEnumFromName("value1")->key, 1U);
}

/// @test
/// Verifies that every HLA representation variant (BE/LE, integer widths, octets) maps to the expected native type
/// @requirements(TBD)
TEST(FomParser, ParsesAllHlaRepresentationVariants)
{
  using sen::Type;
  sen::lang::TypeSetContext context {};
  ASSERT_NO_THROW(context = sen::lang::parseFomDocuments({fomFixtureDir / "representations"}, {}, {}));

  const auto types = collectTypes(context);

  // assert: each simple-data alias maps to the expected native type, keyed on the bare representation variant
  static constexpr std::array<std::pair<std::string_view, decltype(&Type::isUInt8Type)>, 13> typeChecks {{
    {"Float32BEData", &Type::isFloat32Type},   // HLAfloat32BE -> f32
    {"Float32LEData", &Type::isFloat32Type},   // HLAfloat32LE -> f32
    {"Float64BEData", &Type::isFloat64Type},   // HLAfloat64BE -> f64
    {"Float64LEData", &Type::isFloat64Type},   // HLAfloat64LE -> f64
    {"Int16BEData", &Type::isInt16Type},       // HLAinteger16BE -> i16
    {"Int16LEData", &Type::isInt16Type},       // HLAinteger16LE -> i16
    {"Int32BEData", &Type::isInt32Type},       // HLAinteger32BE -> i32
    {"Int32LEData", &Type::isInt32Type},       // HLAinteger32LE -> i32
    {"Int64BEData", &Type::isInt64Type},       // HLAinteger64BE -> i64
    {"Int64LEData", &Type::isInt64Type},       // HLAinteger64LE -> i64
    {"OctetData", &Type::isUInt8Type},         // HLAoctet -> u8
    {"OctetPairBEData", &Type::isUInt16Type},  // HLAoctetPairBE -> u16
    {"OctetPairLEData", &Type::isUInt16Type},  // HLAoctetPairLE -> u16
  }};

  for (const auto& [typeName, method]: typeChecks)
  {
    const auto itr = types.find(std::string(typeName));
    ASSERT_NE(itr, types.end()) << "missing type '" << typeName << "'";
    const auto& type = *itr->second;
    ASSERT_TRUE(type.isAliasType()) << "type '" << typeName << "' should be a simple-data alias";
    const auto& aliased = *type.asAliasType()->getAliasedType();
    auto result = (aliased.*method)();
    EXPECT_TRUE(result) << "type '" << typeName << "' has an unexpected aliased type: '" << aliased.getName() << "'";
  }
}

/// @test
/// Verifies that a mapping file turns interactions into events and methods attached to the mapped class
/// @requirements(TBD)
TEST(FomParser, AddsEventsAndMethodsToClassesFromMappings)
{
  // act
  sen::lang::TypeSetContext context {};
  context = sen::lang::parseFomDocuments({fomFixtureDir / "mapping_fom"}, {fomFixtureDir / "mappings.xml"}, {});

  // find the Entity class in the produced type sets
  const sen::ClassType* entityClass = findClass(context, "Entity");
  ASSERT_NE(entityClass, nullptr);

  // assert: the mapping turned the interaction into both an event and a method on the class
  EXPECT_NE(entityClass->searchEventByName("entityMoved", sen::ClassType::SearchMode::doNotIncludeParents), nullptr);
  const auto* method = entityClass->searchMethodByName("entityMoved", sen::ClassType::SearchMode::doNotIncludeParents);
  ASSERT_NE(method, nullptr);
  EXPECT_FALSE(method->getReturnType()->isVoidType()) << "the mapped <return> should override the implicit void";
}

/// @test
/// Verifies that object class inheritance, property categories and transport modes are correctly parsed
/// @requirements(TBD)
TEST(FomParser, ParsesClassHierarchyPropertiesAndTransportModes)
{
  sen::lang::TypeSetContext context {};
  ASSERT_NO_THROW(context = sen::lang::parseFomDocuments({fomFixtureDir / "hierarchy"}, {}, {}));

  // assert: BaseEntity and its PhysicalEntity subclass are produced, with the correct inheritance
  const auto* baseClass = findClass(context, "BaseEntity");
  ASSERT_NE(baseClass, nullptr);
  const auto* physicalClass = findClass(context, "PhysicalEntity");
  ASSERT_NE(physicalClass, nullptr);

  ASSERT_EQ(physicalClass->getParents().size(), 1);
  EXPECT_EQ(physicalClass->getParents().front()->getName(), "BaseEntity");

  // assert: the property is inherited by the subclass
  const auto* inherited = physicalClass->searchPropertyByName("identifier", sen::ClassType::SearchMode::includeParents);
  ASSERT_NE(inherited, nullptr);

  // assert: property categories derived from updateType/sharing
  const auto baseProperties = collectProperties(*baseClass);
  ASSERT_GT(baseProperties.size(), 0U);
  EXPECT_EQ(baseProperties.at("identifier")->getCategory(),
            sen::PropertyCategory::staticRW);  // Static+PublishSubscribe
  EXPECT_EQ(baseProperties.at("displayName")->getCategory(), sen::PropertyCategory::staticRO);  // Static+Subscribe
  EXPECT_EQ(baseProperties.at("position")->getCategory(), sen::PropertyCategory::dynamicRW);    // Conditional+Writeable

  const auto physicalProperties = collectProperties(*physicalClass);
  ASSERT_GT(physicalProperties.size(), 0U);
  EXPECT_EQ(physicalProperties.at("speed")->getCategory(), sen::PropertyCategory::dynamicRO);  // Conditional+Neither

  // assert: transport modes are mapped from the HLA transport names
  EXPECT_EQ(baseProperties.at("identifier")->getTransportMode(), sen::TransportMode::confirmed);   // HLAreliable
  EXPECT_EQ(baseProperties.at("displayName")->getTransportMode(), sen::TransportMode::multicast);  // HLAbestEffort
}

/// @test
/// Verifies that an `Optional.`-prefixed attribute semantics yields an optional property type
/// @requirements(TBD)
TEST(FomParser, MakesOptionalPropertyTypeFromOptionalSemantics)
{
  sen::lang::TypeSetContext context {};
  ASSERT_NO_THROW(context = sen::lang::parseFomDocuments({fomFixtureDir / "hierarchy"}, {}, {}));

  // assert: the Position property, whose semantics starts with "Optional.", is an optional of a quantity
  const auto* baseClass = findClass(context, "BaseEntity");
  ASSERT_NE(baseClass, nullptr);
  const auto* position = baseClass->searchPropertyByName("position", sen::ClassType::SearchMode::doNotIncludeParents);
  ASSERT_NE(position, nullptr);
  EXPECT_TRUE(position->getType()->isOptionalType());
  EXPECT_EQ(position->getType()->asOptionalType()->getType()->getName(), "QuantityFloat");
}

/// @test
/// Verifies that a nested interaction is resolved through its full path, that a mapped interaction walks its
/// interaction class hierarchy, and that a mapped subclass produces a correctly qualified class path.
/// @requirements(TBD)
TEST(FomParser, MapsNestedInteractionsAndSubclassQualifiedPath)
{
  // act
  sen::lang::TypeSetContext context {};
  context = sen::lang::parseFomDocuments({fomFixtureDir / "nested_interactions"},
                                         {fomFixtureDir / "nested_interactions" / "nested_interactions_mappings.xml"},
                                         {});

  // assert: PhysicalEntity keeps its BaseEntity parent, i.e. computeClassPath was asked for a parent != root
  const auto* physicalClass = findClass(context, "PhysicalEntity");
  ASSERT_NE(physicalClass, nullptr);
  ASSERT_FALSE(physicalClass->getParents().empty());
  EXPECT_EQ(physicalClass->getParents().front()->getName(), "BaseEntity");

  // assert: the nested interaction (Report.ReportMoved) is turned into both an event and a method on the class;
  // resolving it requires computeInteractionPath and findInteractionNode to walk ReportMoved's parent interaction
  EXPECT_NE(physicalClass->searchEventByName("reportMoved", sen::ClassType::SearchMode::doNotIncludeParents), nullptr);
  const auto* method =
    physicalClass->searchMethodByName("reportMoved", sen::ClassType::SearchMode::doNotIncludeParents);
  ASSERT_NE(method, nullptr);

  // assert: the HLAbestEffort method transport maps to unicast
  EXPECT_EQ(method->getTransportMode(), sen::TransportMode::unicast);
}

/// @test
/// Verifies that a document can reuse types declared in another document it depends on
/// @requirements(TBD)
TEST(FomParser, ResolvesTypesDeclaredInDependencyDocument)
{
  sen::lang::TypeSetContext context {};
  ASSERT_NO_THROW(context = sen::lang::parseFomDocuments({fomFixtureDir / "dependent_fom"}, {}, {}));

  // assert: types from both documents are present in the produced type sets
  const auto types = collectTypes(context);
  EXPECT_NE(types.find("PlainIdent"), types.end()) << "dependency alias not parsed";
  EXPECT_NE(types.find("LengthMetre"), types.end()) << "dependency quantity not parsed";

  // assert: the main document's Entity references the dependency-provided types
  const auto* entityClass = findClass(context, "Entity");
  ASSERT_NE(entityClass, nullptr);
  const auto props = collectProperties(*entityClass);
  ASSERT_NE(props.find("identifier"), props.end()) << "missing identifier property";
  ASSERT_NE(props.find("length"), props.end()) << "missing length property";
  EXPECT_TRUE(props.at("identifier")->getType()->isAliasType());
  EXPECT_TRUE(props.at("length")->getType()->isQuantityType());
}

/// @test
/// Verifies that a range cardinality on an array is translated into a bounded sequence
/// @requirements(TBD)
TEST(FomParser, ParsesCardinalityRangeIntoBoundedSequence)
{
  sen::lang::TypeSetContext context {};
  ASSERT_NO_THROW(context = sen::lang::parseFomDocuments({fomFixtureDir / "array_range"}, {}, {}));

  // assert: the [1..5] cardinality yields a sequence whose maximum size is the range maximum
  const auto types = collectTypes(context);
  const auto itr = types.find("RangeIntegers");
  ASSERT_NE(itr, types.end());
  ASSERT_TRUE(itr->second->isSequenceType());
  EXPECT_EQ(itr->second->asSequenceType()->getMaxSize(), std::optional<std::size_t>(5U));
}

/// @test
/// Verifies that a record can reference another record defined later in document order (forward reference)
/// @requirements(TBD)
TEST(FomParser, ResolvesForwardReferencedRecords)
{
  sen::lang::TypeSetContext context {};
  ASSERT_NO_THROW(context = sen::lang::parseFomDocuments({fomFixtureDir / "forward_ref"}, {}, {}));

  const auto types = collectTypes(context);

  // assert: both records are present
  const auto container = types.find("Container");
  ASSERT_NE(container, types.end()) << "Container record not found";
  const auto inner = types.find("InnerRecord");
  ASSERT_NE(inner, types.end()) << "InnerRecord not found";

  // assert: Container has InnerRecord as a field type (forward reference resolved)
  ASSERT_TRUE(container->second->isStructType());
  const auto& fields = container->second->asStructType()->getFields();
  ASSERT_EQ(fields.size(), 1U);
  EXPECT_EQ(fields[0].type->getName(), "InnerRecord");
}

/// @test
/// Verifies that an optional property whose element type is a native HLA representation resolves correctly
/// @requirements(TBD)
TEST(FomParser, ParsesOptionalNativeTypedProperty)
{
  sen::lang::TypeSetContext context {};
  ASSERT_NO_THROW(context = sen::lang::parseFomDocuments({fomFixtureDir / "optional_native"}, {}, {}));

  const auto* entityClass = findClass(context, "Entity");
  ASSERT_NE(entityClass, nullptr);
  const auto* identifier =
    entityClass->searchPropertyByName("identifier", sen::ClassType::SearchMode::doNotIncludeParents);
  ASSERT_NE(identifier, nullptr);
  ASSERT_TRUE(identifier->getType()->isOptionalType()) << "native-typed optional property should be an optional";
  EXPECT_TRUE(identifier->getType()->asOptionalType()->getType()->isInt32Type());
}

/// @test
/// Verifies that property mappings with writable and checked attributes are processed correctly
/// @requirements(TBD)
TEST(FomParser, MapsPropertyWritableAndCheckedAttributes)
{
  sen::lang::TypeSetContext context {};
  ASSERT_NO_THROW(context =
                    sen::lang::parseFomDocuments({fomFixtureDir / "property_mapping"},
                                                 {fomFixtureDir / "property_mapping" / "property_mapping_mappings.xml"},
                                                 {}));

  const auto* entityClass = findClass(context, "Entity");
  ASSERT_NE(entityClass, nullptr);
  const auto props = collectProperties(*entityClass);

  // assert: Identifier (Static+PublishSubscribe -> staticRW) is overridden to dynamicRW by writable="true"
  const auto identifier = props.find("identifier");
  ASSERT_NE(identifier, props.end());
  EXPECT_EQ(identifier->second->getCategory(), sen::PropertyCategory::dynamicRW);

  // assert: Position only carries the checked="true" attribute, so its category is unchanged
  const auto position = props.find("position");
  ASSERT_NE(position, props.end());
  EXPECT_EQ(position->second->getCategory(), sen::PropertyCategory::dynamicRW);  // Conditional+Writeable
  EXPECT_TRUE(position->second->getCheckedSet());

  // assert: Velocity carries both writable and checked attributes
  const auto velocity = props.find("velocity");
  ASSERT_NE(velocity, props.end());
  EXPECT_EQ(velocity->second->getCategory(), sen::PropertyCategory::dynamicRW);
  EXPECT_TRUE(velocity->second->getCheckedSet());

  // assert: Timestamp has no mapping entry, so its category is unchanged and it is not marked checked
  const auto timestamp = props.find("timestamp");
  ASSERT_NE(timestamp, props.end());
  EXPECT_EQ(timestamp->second->getCategory(), sen::PropertyCategory::staticRW);  // Static+PublishSubscribe
  EXPECT_FALSE(timestamp->second->getCheckedSet());
}

/// @test
/// Verifies that various error conditions in FOM parsing are properly rejected with the expected message
/// @requirements(TBD)
struct FomThrowsParam
{
  std::string name;
  std::string fomSubdir;
  std::string mappingSubdir;
  std::string expectedPrefix;

  friend std::ostream& operator<<(std::ostream& os, const FomThrowsParam& param) { return os << param.name; }
};

using FomParserThrowsTest = testing::TestWithParam<FomThrowsParam>;
TEST_P(FomParserThrowsTest, ThrowsWithExpectedMessage)
{
  const auto& param = GetParam();
  std::vector<std::filesystem::path> fomPaths;
  std::vector<std::filesystem::path> mappingPaths;
  if (!param.fomSubdir.empty())
  {
    fomPaths.push_back(fomFixtureDir / param.fomSubdir);
  }
  if (!param.mappingSubdir.empty())
  {
    mappingPaths.push_back(fomFixtureDir / param.mappingSubdir);
  }

  try
  {
    sen::lang::parseFomDocuments(fomPaths, mappingPaths, {});
    FAIL() << param.name << ": expected exception";
  }
  catch (const std::exception& e)
  {
    EXPECT_TRUE(std::strncmp(e.what(), param.expectedPrefix.c_str(), std::strlen(param.expectedPrefix.c_str())) == 0)
      << "expected prefix: '" << param.expectedPrefix << "', actual: '" << e.what() << "'";
  }
}

INSTANTIATE_TEST_SUITE_P(
  FomParserErrorCases,
  FomParserThrowsTest,
  testing::Values(
    /// Transport mode errors
    FomThrowsParam {"UnknownTransport", "bad_transport", "", "unknown transport mode 'UnknownTransport'"},
    FomThrowsParam {"UnknownMethodTransport",
                    "bad_method_transport",
                    "bad_method_transport/bad_method_transport_mappings.xml",
                    "unknown transport mode 'UnknownTransport'"},
    FomThrowsParam {"UnknownEventTransport",
                    "bad_event_transport",
                    "bad_event_transport/bad_event_transport_mappings.xml",
                    "unknown transport mode 'UnknownTransport'"},

    /// Type resolution errors
    FomThrowsParam {"UnknownType", "bad_type", "", "could not find type 'DoesNotExist'"},
    FomThrowsParam {"CircularType", "bad_circular", "", "circular reference while resolving type 'Node'"},
    FomThrowsParam {"UnresolvableDependency",
                    "bad_dependency",
                    "",
                    "could not find dependent document 'NonExistentDependency' for"},

    /// Record/struct errors
    FomThrowsParam {"RecordMultipleIncludes",
                    "bad_multi_include",
                    "",
                    "record 'BadMultiInclude' includes more than one record"},
    FomThrowsParam {"RecordNonRecordInclude",
                    "bad_record",
                    "",
                    "record 'BadRecord' includes 'PlainIdent', which is not a record"},
    FomThrowsParam {"VariantVoidField", "bad_variant", "", "could not find type 'void'"},

    /// Encoding/representation errors
    FomThrowsParam {"UnknownEncoding",
                    "bad_encoding",
                    "",
                    "unknown encoding '128-bit quantum integer' for basic data representation 'MyIntegral'"},
    FomThrowsParam {"NonIntegralEnum",
                    "bad_enumeration",
                    "",
                    "storage type 'HLAfloat64BE' for enumeration 'BadEnum' is not an integral type"},
    FomThrowsParam {"NonNumericQuantity",
                    "bad_simple_data",
                    "",
                    "data representation 'HLAASCIIstring' is not yet supported"},

    /// Sharing mode errors
    FomThrowsParam {"UnknownSharing", "bad_sharing", "", "unknown sharing mode 'UnknownSharing'"},

    /// Interaction mapping errors
    FomThrowsParam {"MissingInteraction",
                    "bad_interaction",
                    "bad_mappings.xml",
                    "could not find interaction 'NonExistentInteraction' using query"},
    FomThrowsParam {"AmbiguousInteraction",
                    "bad_duplicate_interaction",
                    "bad_duplicate_interaction/duplicate_interaction_mappings.xml",
                    "multiple 'Dup' interactions found"},

    /// Mapping file errors
    FomThrowsParam {"InvalidBooleanMapping",
                    "mapping_fom",
                    "bad_is_true_mappings.xml",
                    "invalid value 'banana' for a boolean parameter (expecting 'true' or 'false')"},
    FomThrowsParam {"NonLoadableMapping", "mapping_fom", "non_existent_mapping.xml", "File was not found"},

    /// XML parse errors
    FomThrowsParam {"NonParsableFom", "bad_xml", "", "Start-end tags mismatch"}),
  [](const testing::TestParamInfo<FomThrowsParam>& info) { return info.param.name; });

}  // namespace
