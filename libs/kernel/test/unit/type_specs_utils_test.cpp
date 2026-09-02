// === type_specs_utils_test.cpp =======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/meta/unit.h"
#include "sen/core/meta/unit_registry.h"
#include "sen/core/meta/variant_type.h"
#include "sen/kernel/type_specs_utils.h"

// gtest
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace
{

using sen::AliasSpec;
using sen::AliasType;
using sen::ClassSpec;
using sen::ClassType;
using sen::ConstTypeHandle;
using sen::Enumerator;
using sen::EnumSpec;
using sen::EnumType;
using sen::OptionalSpec;
using sen::OptionalType;
using sen::QuantitySpec;
using sen::QuantityType;
using sen::StructSpec;
using sen::StructType;
using sen::UnitCategory;
using sen::UnitSpec;
using sen::VariantField;
using sen::VariantSpec;
using sen::VariantType;

/// Every generated class carries a constructor; a hand-built one without it is not realistic.
sen::MethodSpec constructorSpec() { return sen::MethodSpec {{"constructorHeld", "", {}}, sen::VoidType::get()}; }

/// A class of one property, so two versions differ only in what that property holds.
sen::TypeHandle<ClassType> classHolding(const ConstTypeHandle<>& valueType)
{
  const ClassSpec spec {"Held", "ns.Held", "", {{"val", "", valueType}}, {}, {}, constructorSpec(), {}, false, {}, {}};
  return ClassType::make(spec);
}

/// A class whose single event carries one argument of the given type.
sen::TypeHandle<ClassType> classEmitting(const ConstTypeHandle<>& argType)
{
  const sen::EventSpec event {{"happened", "", {{"arg", "", argType}}}};
  const ClassSpec spec {"Held", "ns.Held", "", {}, {}, {event}, constructorSpec(), {}, false, {}, {}};
  return ClassType::make(spec);
}

/// A class whose single method takes one argument of argType and returns returnType.
sen::TypeHandle<ClassType> classCalling(const ConstTypeHandle<>& argType, const ConstTypeHandle<>& returnType)
{
  const sen::MethodSpec method {{"call", "", {{"arg", "", argType}}}, returnType};
  const ClassSpec spec {"Held", "ns.Held", "", {}, {method}, {}, constructorSpec(), {}, false, {}, {}};
  return ClassType::make(spec);
}

/// Every lossy finding for one ordered pair, as the kernel asks the question.
std::vector<std::string> lossyReading(const ConstTypeHandle<>& remote, const ConstTypeHandle<>& local)
{
  std::vector<std::string> lossy;
  std::ignore = sen::kernel::runtimeCompatible(local.type(), remote.type(), &lossy);
  return lossy;
}

}  // namespace

/// @test
/// Pins the version differences the kernel accepts today. Each of these is a model that changed
/// between two builds and still interoperates, so a stricter answer here is a regression.
TEST(RuntimeCompatibility, AcceptsTheVersionDifferencesItIsMeantTo)
{
  // the same type on both sides
  EXPECT_TRUE(sen::kernel::runtimeCompatible(sen::UInt32Type::get().type(), sen::UInt32Type::get().type()).empty());

  // a value that became optional
  const OptionalSpec optSpec {"MaybeU32", "ns.MaybeU32", "", sen::UInt32Type::get()};
  auto optional = OptionalType::make(optSpec);
  EXPECT_TRUE(sen::kernel::runtimeCompatible(sen::UInt32Type::get().type(), optional.type()).empty())
    << "a remote value that became optional must stay usable";

  // a numeric that narrowed: allowed today, and the loss is what this branch is about
  EXPECT_TRUE(sen::kernel::runtimeCompatible(sen::UInt8Type::get().type(), sen::UInt32Type::get().type()).empty());
  EXPECT_TRUE(sen::kernel::runtimeCompatible(sen::UInt32Type::get().type(), sen::UInt8Type::get().type()).empty());

  // a property the other side does not declare is skipped rather than refused
  const ClassSpec empty {"Held", "ns.Held", "", {}, {}, {}, std::nullopt, {}, false, {}, {}};
  auto withoutIt = ClassType::make(empty);
  auto withIt = classHolding(sen::UInt32Type::get());
  EXPECT_TRUE(sen::kernel::runtimeCompatible(withIt.type(), withoutIt.type()).empty())
    << "a remote that dropped a property must stay usable";
  EXPECT_TRUE(sen::kernel::runtimeCompatible(withoutIt.type(), withIt.type()).empty())
    << "a remote that added a property must stay usable";
}

/// @test
/// Pins what the kernel refuses, so a laxer answer is a regression too.
TEST(RuntimeCompatibility, RefusesWhatCannotBeConverted)
{
  auto held = classHolding(sen::UInt32Type::get());

  // a class against a number
  EXPECT_FALSE(sen::kernel::runtimeCompatible(held.type(), sen::UInt32Type::get().type()).empty());

  // a property whose type cannot convert at all
  auto holdingClass = classHolding(held);
  auto holdingNumber = classHolding(sen::UInt32Type::get());
  EXPECT_FALSE(sen::kernel::runtimeCompatible(holdingNumber.type(), holdingClass.type()).empty());
}

/// @test
/// Checks that a conversion which is allowed but can drop a value is reported apart from one
/// that cannot, which is what the compatibility modes decide on.
TEST(RuntimeCompatibility, ReportsAConversionThatCanDropAValue)
{
  const auto lossyBetween = [](const ConstTypeHandle<>& remote, const ConstTypeHandle<>& local)
  {
    std::vector<std::string> lossy;
    const auto problems = sen::kernel::runtimeCompatible(local.type(), remote.type(), &lossy);
    EXPECT_TRUE(problems.empty()) << "expected a usable conversion";
    return lossy;
  };

  // narrower destination: the value may not fit
  EXPECT_FALSE(lossyBetween(sen::UInt32Type::get(), sen::UInt8Type::get()).empty());
  EXPECT_FALSE(lossyBetween(sen::Int32Type::get(), sen::Int16Type::get()).empty());
  EXPECT_FALSE(lossyBetween(sen::Float64Type::get(), sen::Float32Type::get()).empty());
  // the negatives have nowhere to go
  EXPECT_FALSE(lossyBetween(sen::Int32Type::get(), sen::UInt32Type::get()).empty());
  // an integer wider than the mantissa
  EXPECT_FALSE(lossyBetween(sen::Int32Type::get(), sen::Float32Type::get()).empty());
  // the fractional part is dropped
  EXPECT_FALSE(lossyBetween(sen::Float64Type::get(), sen::Int32Type::get()).empty());

  // widening keeps every value
  EXPECT_TRUE(lossyBetween(sen::UInt8Type::get(), sen::UInt64Type::get()).empty());
  EXPECT_TRUE(lossyBetween(sen::UInt16Type::get(), sen::Int32Type::get()).empty());
  EXPECT_TRUE(lossyBetween(sen::Int32Type::get(), sen::Float64Type::get()).empty());
  EXPECT_TRUE(lossyBetween(sen::Float32Type::get(), sen::Float64Type::get()).empty());

  // and it is found through a member rather than only at the top level
  EXPECT_FALSE(lossyBetween(classHolding(sen::UInt32Type::get()), classHolding(sen::UInt8Type::get())).empty());
  EXPECT_TRUE(lossyBetween(classHolding(sen::UInt8Type::get()), classHolding(sen::UInt32Type::get())).empty());

  // asking without the out-parameter still answers the old question
  EXPECT_TRUE(sen::kernel::runtimeCompatible(sen::UInt8Type::get().type(), sen::UInt32Type::get().type()).empty());
}

/// @test
/// Every ordered pair of native types, against an explicit list of the ones that keep every value.
/// Written out rather than derived, so the test cannot share a mistake with the implementation.
TEST(RuntimeCompatibility, GradesEveryNativePair)
{
  const std::vector<std::pair<std::string, ConstTypeHandle<>>> natives = {{"bool", sen::BoolType::get()},
                                                                          {"u8", sen::UInt8Type::get()},
                                                                          {"i16", sen::Int16Type::get()},
                                                                          {"u16", sen::UInt16Type::get()},
                                                                          {"i32", sen::Int32Type::get()},
                                                                          {"u32", sen::UInt32Type::get()},
                                                                          {"i64", sen::Int64Type::get()},
                                                                          {"u64", sen::UInt64Type::get()},
                                                                          {"f32", sen::Float32Type::get()},
                                                                          {"f64", sen::Float64Type::get()}};

  // from -> to pairs where no value can be lost. Everything else must be reported.
  const std::set<std::string> lossless = {
    "bool>u8", "bool>i16", "bool>u16", "bool>i32", "bool>u32", "bool>i64", "bool>u64", "bool>f32", "bool>f64",
    "u8>i16",  "u8>u16",   "u8>i32",   "u8>u32",   "u8>i64",   "u8>u64",   "u8>f32",   "u8>f64",   "i16>i32",
    "i16>i64", "i16>f32",  "i16>f64",  "u16>i32",  "u16>u32",  "u16>i64",  "u16>u64",  "u16>f32",  "u16>f64",
    "i32>i64", "i32>f64",  "u32>i64",  "u32>u64",  "u32>f64",  "f32>f64"};

  for (const auto& [fromName, fromType]: natives)
  {
    for (const auto& [toName, toType]: natives)
    {
      const auto key = fromName + ">" + toName;
      const bool expectedLossless = (fromName == toName) || lossless.count(key) != 0;
      const bool reported = !lossyReading(fromType, toType).empty();

      EXPECT_EQ(reported, !expectedLossless) << "reading " << fromName << " as " << toName;
    }
  }
}

/// @test
/// The loss has to be found wherever it hides in a class, not only on a property.
TEST(RuntimeCompatibility, FindsLossThroughEveryKindOfMember)
{
  auto wide = sen::UInt32Type::get();
  auto narrow = sen::UInt8Type::get();

  // a property, read from the writer
  EXPECT_FALSE(lossyReading(classHolding(wide), classHolding(narrow)).empty()) << "property";
  EXPECT_TRUE(lossyReading(classHolding(narrow), classHolding(wide)).empty()) << "property, widening";

  // an event argument, which reaches us the way a property does
  EXPECT_FALSE(lossyReading(classEmitting(wide), classEmitting(narrow)).empty()) << "event argument";
  EXPECT_TRUE(lossyReading(classEmitting(narrow), classEmitting(wide)).empty()) << "event argument, widening";

  // a method return value, which also comes from the writer
  EXPECT_FALSE(lossyReading(classCalling(wide, wide), classCalling(wide, narrow)).empty()) << "method return";
  EXPECT_TRUE(lossyReading(classCalling(wide, narrow), classCalling(wide, wide)).empty()) << "method return, widening";

  // a method argument travels the other way: we supply it, so the writer's type is the destination
  EXPECT_FALSE(lossyReading(classCalling(narrow, wide), classCalling(wide, wide)).empty()) << "method argument";
  EXPECT_TRUE(lossyReading(classCalling(wide, wide), classCalling(narrow, wide)).empty())
    << "method argument, widening";
}

/// @test
/// And wherever it hides inside a value type.
TEST(RuntimeCompatibility, FindsLossThroughNestedTypes)
{
  const auto structOf = [](const ConstTypeHandle<>& fieldType)
  { return StructType::make({"S", "ns.S", "", {{"f", "", fieldType}}, {}}); };

  const auto sequenceOf = [](const ConstTypeHandle<>& elementType)
  { return sen::SequenceType::make({"Seq", "ns.Seq", "", elementType}); };

  const auto optionalOf = [](const ConstTypeHandle<>& valueType)
  { return OptionalType::make({"Opt", "ns.Opt", "", valueType}); };

  auto wide = sen::UInt32Type::get();
  auto narrow = sen::UInt8Type::get();

  EXPECT_FALSE(lossyReading(structOf(wide), structOf(narrow)).empty()) << "struct field";
  EXPECT_TRUE(lossyReading(structOf(narrow), structOf(wide)).empty()) << "struct field, widening";

  EXPECT_FALSE(lossyReading(sequenceOf(wide), sequenceOf(narrow)).empty()) << "sequence element";
  EXPECT_TRUE(lossyReading(sequenceOf(narrow), sequenceOf(wide)).empty()) << "sequence element, widening";

  EXPECT_FALSE(lossyReading(optionalOf(wide), optionalOf(narrow)).empty()) << "optional value";
  EXPECT_TRUE(lossyReading(optionalOf(narrow), optionalOf(wide)).empty()) << "optional value, widening";

  // and through a property whose type is itself a struct
  EXPECT_FALSE(lossyReading(classHolding(structOf(wide)), classHolding(structOf(narrow))).empty())
    << "struct inside a class";
}

/// @test
/// Every combination the mode decision can be asked about: three modes against a type that is
/// identical, one that differs harmlessly, and one whose conversion can drop a value.
TEST(CompatibilityModes, DecidesEveryCombination)
{
  using sen::kernel::acceptsUnderCompatibilityMode;
  using sen::kernel::CompatibilityMode;

  // relaxed converts whatever it can, which is what the kernel does today
  EXPECT_TRUE(acceptsUnderCompatibilityMode(CompatibilityMode::relaxed, true, false)) << "identical";
  EXPECT_TRUE(acceptsUnderCompatibilityMode(CompatibilityMode::relaxed, false, false)) << "harmless difference";
  EXPECT_TRUE(acceptsUnderCompatibilityMode(CompatibilityMode::relaxed, false, true)) << "lossy difference";

  // strict converts, but not when something can be lost
  EXPECT_TRUE(acceptsUnderCompatibilityMode(CompatibilityMode::strict, true, false)) << "identical";
  EXPECT_TRUE(acceptsUnderCompatibilityMode(CompatibilityMode::strict, false, false)) << "harmless difference";
  EXPECT_FALSE(acceptsUnderCompatibilityMode(CompatibilityMode::strict, false, true)) << "lossy difference";

  // disabled takes an exact match and nothing else, harmless or not
  EXPECT_TRUE(acceptsUnderCompatibilityMode(CompatibilityMode::disabled, true, false)) << "identical";
  EXPECT_FALSE(acceptsUnderCompatibilityMode(CompatibilityMode::disabled, false, false)) << "harmless difference";
  EXPECT_FALSE(acceptsUnderCompatibilityMode(CompatibilityMode::disabled, false, true)) << "lossy difference";

  // a harmless difference is the only case where strict and disabled part company
  EXPECT_NE(acceptsUnderCompatibilityMode(CompatibilityMode::strict, false, false),
            acceptsUnderCompatibilityMode(CompatibilityMode::disabled, false, false));

  // relaxed never refuses, whatever it is shown
  for (const bool equivalent: {true, false})
  {
    for (const bool lossy: {true, false})
    {
      EXPECT_TRUE(acceptsUnderCompatibilityMode(CompatibilityMode::relaxed, equivalent, lossy));
    }
  }
}

/// @test
/// The default has to stay relaxed, which is what the kernel did before the mode existed. It is
/// otherwise implicit: the field value-initialises to zero and relaxed happens to be declared
/// first, so reordering the enum would change it silently.
TEST(CompatibilityModes, DefaultsToRelaxed)
{
  const sen::kernel::KernelParams params {};
  EXPECT_EQ(params.compatibilityMode, sen::kernel::CompatibilityMode::relaxed);

  // and relaxed is the mode that accepts everything the kernel accepted before
  EXPECT_TRUE(sen::kernel::acceptsUnderCompatibilityMode(params.compatibilityMode, false, true));
}

/// @test
/// The kinds of type the compatibility check handles that nothing else exercises. Written because
/// the analysis had no unit tests at all, so most of its branches were reached only by two
/// multi-process tests that assert a log line.
TEST(RuntimeCompatibility, HandlesEveryKindOfCustomType)
{
  const auto enumOf = [](const std::vector<Enumerator>& enumerators, auto storage)
  { return EnumType::make({"E", "ns.E", "", enumerators, std::move(storage)}); };

  const std::vector<Enumerator> one = {{"one", 1, ""}};
  const std::vector<Enumerator> two = {{"one", 1, ""}, {"two", 2, ""}};

  // an enumerator added on one side, and a storage type that narrows
  EXPECT_TRUE(
    sen::kernel::runtimeCompatible(enumOf(one, sen::UInt8Type::get()).type(), enumOf(two, sen::UInt8Type::get()).type())
      .empty())
    << "an added enumerator must stay usable";
  std::vector<std::string> lossy;
  std::ignore = sen::kernel::runtimeCompatible(
    enumOf(one, sen::UInt8Type::get()).type(), enumOf(one, sen::UInt32Type::get()).type(), &lossy);
  EXPECT_FALSE(lossy.empty()) << "an enum whose storage narrows can drop a value";

  // a variant alternative whose type narrows
  const auto variantOf = [](const ConstTypeHandle<>& fieldType)
  { return VariantType::make({"V", "ns.V", "", {VariantField {1, "", fieldType}}}); };
  EXPECT_FALSE(lossyReading(variantOf(sen::UInt32Type::get()), variantOf(sen::UInt8Type::get())).empty())
    << "variant alternative";
  EXPECT_TRUE(lossyReading(variantOf(sen::UInt8Type::get()), variantOf(sen::UInt32Type::get())).empty())
    << "variant alternative, widening";

  // an alias is transparent, so the loss behind it still shows
  const auto aliasOf = [](const ConstTypeHandle<>& aliased) { return AliasType::make({"A", "ns.A", "", aliased}); };
  EXPECT_FALSE(lossyReading(aliasOf(sen::UInt32Type::get()), aliasOf(sen::UInt8Type::get())).empty()) << "alias";

  // time types convert between themselves and are not numbers
  EXPECT_TRUE(lossyReading(sen::TimestampType::get(), sen::DurationType::get()).empty()) << "timestamp to duration";
  EXPECT_TRUE(lossyReading(sen::DurationType::get(), sen::TimestampType::get()).empty()) << "duration to timestamp";

  // a string on either side is a supported difference and is not graded for loss
  EXPECT_TRUE(sen::kernel::runtimeCompatible(sen::UInt32Type::get().type(), sen::StringType::get().type()).empty())
    << "a remote string read as a number is accepted today";
  EXPECT_TRUE(lossyReading(sen::StringType::get(), sen::UInt8Type::get()).empty()) << "string is not graded";

  // void matches only itself
  EXPECT_TRUE(sen::kernel::runtimeCompatible(sen::VoidType::get().type(), sen::VoidType::get().type()).empty());
  EXPECT_FALSE(sen::kernel::runtimeCompatible(sen::VoidType::get().type(), sen::UInt8Type::get().type()).empty());
}

/// @test
/// getRuntimeDifferences is what the kernel logs when it decides to adapt two versions of a type.
/// It had no tests at all, so nothing said what it is supposed to report.
TEST(RuntimeDifferences, ReportsWhatChangedBetweenTwoVersions)
{
  // the same definition has nothing to say
  EXPECT_TRUE(sen::kernel::getRuntimeDifferences(sen::UInt32Type::get().type(), sen::UInt32Type::get().type()).empty());

  auto wide = classHolding(sen::UInt32Type::get());
  auto narrow = classHolding(sen::UInt8Type::get());

  // a property whose type changed is reported
  EXPECT_FALSE(sen::kernel::getRuntimeDifferences(narrow.type(), wide.type()).empty())
    << "a changed property type must be reported";

  // a property present on one side only is reported
  const ClassSpec empty {"Held", "ns.Held", "", {}, {}, {}, std::nullopt, {}, false, {}, {}};
  auto without = ClassType::make(empty);
  EXPECT_FALSE(sen::kernel::getRuntimeDifferences(wide.type(), without.type()).empty())
    << "a dropped property must be reported";
}

/// @test
/// Quantities, which the compatibility check treats as a number carrying a unit.
TEST(RuntimeCompatibility, HandlesQuantities)
{
  auto metre = sen::Unit::make(UnitSpec {UnitCategory::length, "metre", "metres", "m", 1.0, 0.0, 0.0});
  auto kilogram = sen::Unit::make(UnitSpec {UnitCategory::mass, "kilogram", "kilograms", "kg", 1.0, 0.0, 0.0});

  const auto quantityOf = [](auto element, const sen::Unit* unit)
  { return QuantityType::make({"Q", "ns.Q", "", std::move(element), unit, std::nullopt, std::nullopt}); };

  // a length arriving where a mass is expected is refused, which is the case the documentation cites
  EXPECT_FALSE(sen::kernel::runtimeCompatible(quantityOf(sen::Float32Type::get(), metre.get()).type(),
                                              quantityOf(sen::Float32Type::get(), kilogram.get()).type())
                 .empty())
    << "a length must not be read as a mass";

  // the same unit either side is usable
  EXPECT_TRUE(sen::kernel::runtimeCompatible(quantityOf(sen::Float32Type::get(), metre.get()).type(),
                                             quantityOf(sen::Float32Type::get(), metre.get()).type())
                .empty());

  // and the element type narrowing under the unit is a loss like any other
  EXPECT_FALSE(
    lossyReading(quantityOf(sen::Float64Type::get(), metre.get()), quantityOf(sen::Float32Type::get(), metre.get()))
      .empty())
    << "quantity element narrowing";
  EXPECT_TRUE(
    lossyReading(quantityOf(sen::Float32Type::get(), metre.get()), quantityOf(sen::Float64Type::get(), metre.get()))
      .empty())
    << "quantity element widening";
}

/// @test
/// getRuntimeDifferences over the kinds of type it handles. It reports what changed between two
/// versions, which the kernel logs when it decides to adapt them, and only the class case had a
/// test before this one.
TEST(RuntimeDifferences, ReportsEveryKindOfChange)
{
  const auto differs = [](const ConstTypeHandle<>& local, const ConstTypeHandle<>& remote)
  { return !sen::kernel::getRuntimeDifferences(local.type(), remote.type()).empty(); };

  auto wide = sen::UInt32Type::get();
  auto narrow = sen::UInt8Type::get();

  // struct: a field whose type changed
  const auto structOf = [](const ConstTypeHandle<>& fieldType)
  { return StructType::make({"S", "ns.S", "", {{"f", "", fieldType}}, {}}); };
  EXPECT_TRUE(differs(structOf(wide), structOf(narrow))) << "struct field";
  EXPECT_FALSE(differs(structOf(wide), structOf(wide))) << "identical structs";

  // sequence: an element whose type changed
  const auto sequenceOf = [](const ConstTypeHandle<>& elementType)
  { return sen::SequenceType::make({"Seq", "ns.Seq", "", elementType}); };
  EXPECT_TRUE(differs(sequenceOf(wide), sequenceOf(narrow))) << "sequence element";

  // enum: a differing enumerator set, and a differing storage type
  const auto enumOf = [](const std::vector<Enumerator>& enumerators, auto storage)
  { return EnumType::make({"E", "ns.E", "", enumerators, std::move(storage)}); };
  const std::vector<Enumerator> one = {{"one", 1, ""}};
  const std::vector<Enumerator> two = {{"one", 1, ""}, {"two", 2, ""}};
  EXPECT_TRUE(differs(enumOf(one, sen::UInt8Type::get()), enumOf(two, sen::UInt8Type::get()))) << "enumerators";
  EXPECT_TRUE(differs(enumOf(one, sen::UInt8Type::get()), enumOf(one, sen::UInt32Type::get()))) << "enum storage";

  // variant: an alternative present on one side only, addressed by key rather than position
  const auto variantOf = [](const std::vector<VariantField>& fields)
  { return VariantType::make({"V", "ns.V", "", fields}); };
  EXPECT_TRUE(differs(variantOf({VariantField {7, "", wide}}), variantOf({VariantField {7, "", narrow}})))
    << "variant alternative type";
  EXPECT_TRUE(differs(variantOf({VariantField {7, "", wide}}),
                      variantOf({VariantField {7, "", wide}, VariantField {9, "", wide}})))
    << "an alternative the other side does not have";

  // optional and alias are transparent wrappers
  EXPECT_TRUE(differs(OptionalType::make({"O", "ns.O", "", wide}), OptionalType::make({"O", "ns.O", "", narrow})))
    << "optional value";
  EXPECT_TRUE(differs(AliasType::make({"A", "ns.A", "", wide}), AliasType::make({"A", "ns.A", "", narrow})))
    << "aliased type";
}

/// @test
/// The wire round trip: a type becomes a spec, and the spec rebuilds the type. This is what
/// crosses the bus between two kernels, so anything the pair loses is a type that arrives wrong.
TEST(TypeSpecs, RoundTripsEveryKindOfType)
{
  const sen::CustomTypeRegistry natives;
  sen::CustomTypeRegistry remote;

  const auto roundTrips = [&natives, &remote](const sen::ConstTypeHandle<sen::CustomType>& original)
  {
    const auto spec = sen::kernel::makeCustomTypeSpec(original.type());
    const auto rebuilt = sen::kernel::buildNonNativeType(spec, natives, remote);
    remote.add(rebuilt);
    return sen::kernel::equivalent(original.type(), rebuilt.type());
  };

  EXPECT_TRUE(roundTrips(StructType::make({"S", "ns.S", "", {{"f", "", sen::UInt32Type::get()}}, {}}))) << "struct";

  EXPECT_TRUE(roundTrips(EnumType::make({"E", "ns.E", "", {{"one", 1, ""}, {"two", 2, ""}}, sen::UInt8Type::get()})))
    << "enum";

  EXPECT_TRUE(roundTrips(sen::SequenceType::make({"Seq", "ns.Seq", "", sen::UInt32Type::get(), 10U}))) << "sequence";

  EXPECT_TRUE(roundTrips(VariantType::make({"V", "ns.V", "", {VariantField {7, "", sen::UInt32Type::get()}}})))
    << "variant, with a key that is not its position";

  EXPECT_TRUE(roundTrips(AliasType::make({"A", "ns.A", "", sen::UInt32Type::get()}))) << "alias";

  EXPECT_TRUE(roundTrips(OptionalType::make({"O", "ns.O", "", sen::UInt32Type::get()}))) << "optional";

  // the rebuild resolves the unit through the global registry, so the round trip only holds for a
  // unit the receiving side knows
  const auto registeredMetre = sen::UnitRegistry::get().searchUnitByName("meter");
  ASSERT_TRUE(registeredMetre.has_value());
  EXPECT_TRUE(roundTrips(QuantityType::make(
    {"Q", "ns.Q", "", sen::Float32Type::get(), registeredMetre.value(), std::nullopt, std::nullopt})))
    << "quantity";

  EXPECT_TRUE(roundTrips(classHolding(sen::UInt32Type::get()))) << "class with a property";
  EXPECT_TRUE(roundTrips(classEmitting(sen::UInt32Type::get()))) << "class with an event";
  EXPECT_TRUE(roundTrips(classCalling(sen::UInt32Type::get(), sen::UInt8Type::get()))) << "class with a method";
}

/// @test
/// The protocol version migration, which is how a current kernel reads a type definition sent by
/// an older one. Untested before this, and a mistake here is silent: the type is accepted and
/// wrong rather than refused.
TEST(TypeSpecs, MigratesOlderProtocolVersions)
{
  // an enumeration as protocol V4 described it, with a signed key and no per-enumerator description
  sen::kernel::EnumTypeSpecV4 enumV4 {};
  enumV4.storageType = sen::kernel::IntegralType::uint8Type;
  enumV4.enums.emplace_back();
  enumV4.enums.back() = {"one", 1};
  enumV4.enums.emplace_back();
  enumV4.enums.back() = {"two", 2};

  sen::kernel::CustomTypeSpecV4 v4 {};
  v4.name = "E";
  v4.qualifiedName = "ns.E";
  v4.description = "an enum from an older kernel";
  v4.data = enumV4;

  const auto migrated = sen::kernel::toCurrentVersion(v4);

  EXPECT_EQ(migrated.name, "E");
  EXPECT_EQ(migrated.qualifiedName, "ns.E");
  EXPECT_EQ(migrated.description, "an enum from an older kernel");

  const auto* asEnum = std::get_if<sen::kernel::EnumTypeSpec>(&migrated.data);
  ASSERT_NE(asEnum, nullptr) << "an enum must migrate to an enum";
  ASSERT_EQ(asEnum->enums.size(), 2U) << "no enumerator may be dropped";
  EXPECT_EQ(asEnum->enums.at(0).name, "one");
  EXPECT_EQ(asEnum->enums.at(0).key, 1U);
  EXPECT_EQ(asEnum->enums.at(1).name, "two");
  EXPECT_EQ(asEnum->enums.at(1).key, 2U);

  // and the migrated spec has to be usable, not merely well formed
  const sen::CustomTypeRegistry natives;
  const sen::CustomTypeRegistry remote;
  const auto rebuilt = sen::kernel::buildNonNativeType(migrated, natives, remote);
  ASSERT_TRUE(rebuilt->isEnumType());
  EXPECT_EQ(rebuilt->asEnumType()->getEnums().size(), 2U);

  // V4 encodes an unbounded sequence as a bound of zero, the same as V5
  sen::kernel::SequenceTypeSpecV4 sequenceV4 {};
  sequenceV4.elementType = "u32";
  sequenceV4.maxSize = 0U;

  sen::kernel::CustomTypeSpecV4 sequenceSpec {};
  sequenceSpec.name = "Seq";
  sequenceSpec.qualifiedName = "ns.Seq";
  sequenceSpec.data = sequenceV4;

  const auto migratedSequence = sen::kernel::toCurrentVersion(sequenceSpec);
  const auto* asSequence = std::get_if<sen::kernel::SequenceTypeSpec>(&migratedSequence.data);
  ASSERT_NE(asSequence, nullptr);
  EXPECT_FALSE(asSequence->maxSize.asOptional().has_value())
    << "a V4 sequence with maxSize 0 is unbounded, not bounded at zero";
}

/// @test
/// Field-by-field, rather than by hash. The round trip above compares with equivalent(), which is
/// hash equality, so a field the hash does not cover could be dropped or swapped and still look
/// identical. These are the fields most easily lost in a copy: bounds, ranges, categories, flags.
TEST(TypeSpecs, RoundTripKeepsFieldsTheHashDoesNotCover)
{
  const sen::CustomTypeRegistry natives;
  sen::CustomTypeRegistry remote;

  const auto rebuild = [&natives, &remote](const sen::ConstTypeHandle<sen::CustomType>& original)
  {
    const auto rebuilt =
      sen::kernel::buildNonNativeType(sen::kernel::makeCustomTypeSpec(original.type()), natives, remote);
    remote.add(rebuilt);
    return rebuilt;
  };

  // a sequence's bound
  {
    auto rebuilt = rebuild(sen::SequenceType::make({"Seq", "ns.Seq", "", sen::UInt32Type::get(), 7U}));
    ASSERT_TRUE(rebuilt->isSequenceType());
    EXPECT_EQ(rebuilt->asSequenceType()->getMaxSize(), std::optional<std::size_t> {7U}) << "maximum size";
  }

  // an unbounded sequence must not acquire a bound
  {
    auto rebuilt = rebuild(sen::SequenceType::make({"Unb", "ns.Unb", "", sen::UInt32Type::get()}));
    ASSERT_TRUE(rebuilt->isSequenceType());
    EXPECT_FALSE(rebuilt->asSequenceType()->getMaxSize().has_value()) << "unbounded must stay unbounded";
  }

  // a quantity's range, which is two values of the same type and so the easiest pair to swap
  {
    const auto metre = sen::UnitRegistry::get().searchUnitByName("meter");
    ASSERT_TRUE(metre.has_value());
    auto rebuilt = rebuild(QuantityType::make({"Q", "ns.Q", "", sen::Float64Type::get(), metre.value(), -3.5, 11.25}));
    ASSERT_TRUE(rebuilt->isQuantityType());
    const auto* quantity = rebuilt->asQuantityType();
    ASSERT_TRUE(quantity->getMinValue().has_value());
    ASSERT_TRUE(quantity->getMaxValue().has_value());
    EXPECT_DOUBLE_EQ(quantity->getMinValue().value(), -3.5) << "minimum, not the maximum";
    EXPECT_DOUBLE_EQ(quantity->getMaxValue().value(), 11.25) << "maximum, not the minimum";
    ASSERT_TRUE(quantity->getUnit().has_value());
    EXPECT_EQ(quantity->getUnit().value()->getName(), metre.value()->getName()) << "unit";
  }

  // an enumerator's description, which the class hash does not reach
  {
    auto rebuilt = rebuild(EnumType::make(
      {"E", "ns.E", "", {{"one", 1, "the first one"}, {"two", 2, "the second"}}, sen::UInt8Type::get()}));
    ASSERT_TRUE(rebuilt->isEnumType());
    const auto& enums = rebuilt->asEnumType()->getEnums();
    ASSERT_EQ(enums.size(), 2U);
    EXPECT_EQ(enums[0].key, 1U);
    EXPECT_EQ(enums[1].key, 2U);
    EXPECT_EQ(enums[0].description, "the first one") << "enumerator description";
    EXPECT_EQ(rebuilt->asEnumType()->getStorageType().getName(), sen::UInt8Type::get()->getName()) << "storage";
  }

  // a property's category and transport mode, neither of which is in the hash of the property type
  {
    const sen::PropertySpec writable {
      "val", "a description", sen::UInt32Type::get(), sen::PropertyCategory::dynamicRW, sen::TransportMode::confirmed};
    const ClassSpec spec {"C", "ns.C", "", {writable}, {}, {}, constructorSpec(), {}, false, {}, {}};

    auto rebuilt = rebuild(ClassType::make(spec));
    ASSERT_TRUE(rebuilt->isClassType());
    const auto* property = rebuilt->asClassType()->searchPropertyByName("val");
    ASSERT_NE(property, nullptr);
    EXPECT_EQ(property->getCategory(), sen::PropertyCategory::dynamicRW) << "category";
    EXPECT_EQ(property->getTransportMode(), sen::TransportMode::confirmed) << "transport mode";
    EXPECT_EQ(property->getDescription(), "a description") << "description";
  }
}

/// @test
/// The V5 migration, the other half of the older-protocol path. V5 carries a per-enumerator hash
/// the current version does not, and a sequence bound where zero means unbounded rather than a
/// bound of zero, which is the kind of encoding a copy gets wrong.
TEST(TypeSpecs, MigratesProtocolV5)
{
  const auto migrateSequence = [](std::uint64_t maxSize, bool fixedSize)
  {
    sen::kernel::SequenceTypeSpecV5 sequenceV5 {};
    sequenceV5.elementType = "u32";
    sequenceV5.maxSize = maxSize;
    sequenceV5.fixedSize = fixedSize;

    sen::kernel::CustomTypeSpecV5 v5 {};
    v5.name = "Seq";
    v5.qualifiedName = "ns.Seq";
    v5.data = sequenceV5;

    return sen::kernel::toCurrentVersion(v5);
  };

  // a bound survives, and so does the fixed-size flag beside it
  {
    const auto migrated = migrateSequence(9U, true);
    const auto* asSequence = std::get_if<sen::kernel::SequenceTypeSpec>(&migrated.data);
    ASSERT_NE(asSequence, nullptr);
    EXPECT_EQ(asSequence->maxSize.asOptional(), std::optional<std::uint64_t> {9U}) << "maximum size";
    EXPECT_TRUE(asSequence->fixedSize) << "fixed size flag";
  }

  // zero means unbounded, and must not become a bound of zero
  {
    const auto migrated = migrateSequence(0U, false);
    const auto* asSequence = std::get_if<sen::kernel::SequenceTypeSpec>(&migrated.data);
    ASSERT_NE(asSequence, nullptr);
    EXPECT_FALSE(asSequence->maxSize.asOptional().has_value())
      << "V5 encodes unbounded as zero; the current spec uses an empty optional";
    EXPECT_FALSE(asSequence->fixedSize);

    const sen::CustomTypeRegistry natives;
    const sen::CustomTypeRegistry remote;
    const auto rebuilt = sen::kernel::buildNonNativeType(migrated, natives, remote);
    ASSERT_TRUE(rebuilt->isSequenceType());
    EXPECT_FALSE(rebuilt->asSequenceType()->getMaxSize().has_value())
      << "a V5 sequence with maxSize 0 is unbounded, not bounded at zero";
  }

  // an enumerator keeps its name and key across the migration
  {
    sen::kernel::EnumTypeSpecV5 enumV5 {};
    enumV5.storageType = sen::kernel::IntegralType::uint16Type;
    enumV5.enums.emplace_back();
    enumV5.enums.back() = {"alpha", 11, 0U};
    enumV5.enums.emplace_back();
    enumV5.enums.back() = {"beta", 22, 0U};

    sen::kernel::CustomTypeSpecV5 v5 {};
    v5.name = "E";
    v5.qualifiedName = "ns.E";
    v5.data = enumV5;

    const auto migrated = sen::kernel::toCurrentVersion(v5);
    const auto* asEnum = std::get_if<sen::kernel::EnumTypeSpec>(&migrated.data);
    ASSERT_NE(asEnum, nullptr);
    ASSERT_EQ(asEnum->enums.size(), 2U);
    EXPECT_EQ(asEnum->enums.at(0).name, "alpha");
    EXPECT_EQ(asEnum->enums.at(0).key, 11U);
    EXPECT_EQ(asEnum->enums.at(1).name, "beta");
    EXPECT_EQ(asEnum->enums.at(1).key, 22U);
    EXPECT_EQ(asEnum->storageType, sen::kernel::IntegralType::uint16Type) << "storage type";
  }
}

/// @test
/// A member that cannot be converted at all, one kind at a time. These are the branches that build
/// the message an operator reads when two models genuinely clash, and each member kind builds its
/// own, so a mistake in one is invisible from the others.
TEST(RuntimeCompatibility, RefusesAnIncompatibleMemberOfEveryKind)
{
  // a class where a number is expected converts to nothing
  auto incompatible = classHolding(sen::UInt32Type::get());
  auto number = sen::UInt32Type::get();

  const auto refuses = [](const ConstTypeHandle<>& local, const ConstTypeHandle<>& remote)
  { return !sen::kernel::runtimeCompatible(local.type(), remote.type()).empty(); };

  // a struct field
  const auto structOf = [](const ConstTypeHandle<>& fieldType)
  { return StructType::make({"S", "ns.S", "", {{"f", "", fieldType}}, {}}); };
  EXPECT_TRUE(refuses(structOf(number), structOf(incompatible))) << "struct field";

  // a variant alternative, addressed by a key that is not its position
  const auto variantOf = [](const ConstTypeHandle<>& fieldType)
  { return VariantType::make({"V", "ns.V", "", {VariantField {7, "", fieldType}}}); };
  EXPECT_TRUE(refuses(variantOf(number), variantOf(incompatible))) << "variant alternative";

  // an event argument
  EXPECT_TRUE(refuses(classEmitting(number), classEmitting(incompatible))) << "event argument";

  // a method argument and a method return value
  EXPECT_TRUE(refuses(classCalling(number, number), classCalling(incompatible, number))) << "method argument";
  EXPECT_TRUE(refuses(classCalling(number, number), classCalling(number, incompatible))) << "method return";

  // a sequence element
  const auto sequenceOf = [](const ConstTypeHandle<>& elementType)
  { return sen::SequenceType::make({"Seq", "ns.Seq", "", elementType}); };
  EXPECT_TRUE(refuses(sequenceOf(number), sequenceOf(incompatible))) << "sequence element";

  // a time type against something that is not one
  EXPECT_TRUE(refuses(sen::DurationType::get(), sen::UInt32Type::get())) << "duration against a number";
  EXPECT_TRUE(refuses(sen::TimestampType::get(), sen::UInt32Type::get())) << "timestamp against a number";

  // and an alternative the other side does not declare is skipped rather than refused
  EXPECT_FALSE(refuses(VariantType::make({"V", "ns.V", "", {VariantField {7, "", number}}}),
                       VariantType::make({"V", "ns.V", "", {VariantField {9, "", number}}})))
    << "an alternative present on one side only";
}

/// @test
/// A type that is not the kind the other side expects at all, and a quantity that gains or loses
/// its unit. These are the outright refusals, as opposed to a member that fails inside a matching
/// pair.
TEST(RuntimeCompatibility, RefusesAMismatchedKind)
{
  const auto refuses = [](const ConstTypeHandle<>& local, const ConstTypeHandle<>& remote)
  { return !sen::kernel::runtimeCompatible(local.type(), remote.type()).empty(); };

  auto number = sen::UInt32Type::get();
  auto aStruct = StructType::make({"S", "ns.S", "", {{"f", "", number}}, {}});
  auto aVariant = VariantType::make({"V", "ns.V", "", {VariantField {7, "", number}}});
  auto aSequence = sen::SequenceType::make({"Seq", "ns.Seq", "", number});
  auto aClass = classHolding(number);

  // each kind against something that is not that kind
  EXPECT_TRUE(refuses(aStruct, aVariant)) << "struct against a variant";
  EXPECT_TRUE(refuses(aVariant, aStruct)) << "variant against a struct";
  EXPECT_TRUE(refuses(aSequence, aStruct)) << "sequence against a struct";
  EXPECT_TRUE(refuses(aClass, aStruct)) << "class against a struct";

  // a quantity that is dimensional on one side only
  const auto metre = sen::UnitRegistry::get().searchUnitByName("meter");
  ASSERT_TRUE(metre.has_value());
  auto withUnit =
    QuantityType::make({"Q", "ns.Q", "", sen::Float32Type::get(), metre.value(), std::nullopt, std::nullopt});
  auto withoutUnit =
    QuantityType::make({"Q", "ns.Q", "", sen::Float32Type::get(), std::nullopt, std::nullopt, std::nullopt});

  EXPECT_TRUE(refuses(withoutUnit, withUnit)) << "a dimensionless local reading a unit-carrying remote";
  EXPECT_TRUE(refuses(withUnit, withoutUnit)) << "a unit-carrying local reading a dimensionless remote";

  // and a quantity against something that carries no number at all
  EXPECT_TRUE(refuses(withUnit, aStruct)) << "quantity against a struct";
}
