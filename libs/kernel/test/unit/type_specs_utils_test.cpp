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

/// @test
/// What getRuntimeDifferences says about two versions of a value type. Each branch builds its own
/// message, so one being wrong is invisible from the others, and the kernel logs these when it
/// decides to adapt a type rather than refuse it.
TEST(RuntimeDifferences, ReportsEveryChangeToAValueType)
{
  const auto differences = [](const ConstTypeHandle<>& local, const ConstTypeHandle<>& remote)
  { return sen::kernel::getRuntimeDifferences(local.type(), remote.type()); };
  const auto differs = [&differences](const ConstTypeHandle<>& local, const ConstTypeHandle<>& remote)
  { return !differences(local, remote).empty(); };

  auto wide = sen::UInt32Type::get();
  auto narrow = sen::UInt8Type::get();

  // a sequence: element, bound, and the fixed-size flag, each on its own
  const auto sequenceOf = [](const ConstTypeHandle<>& element, std::optional<std::size_t> maxSize, bool fixed)
  { return sen::SequenceType::make({"Seq", "ns.Seq", "", element, maxSize, fixed}); };
  EXPECT_TRUE(differs(sequenceOf(wide, 4U, false), sequenceOf(narrow, 4U, false))) << "sequence element";
  EXPECT_TRUE(differs(sequenceOf(wide, 4U, false), sequenceOf(wide, 8U, false))) << "sequence bound";
  EXPECT_TRUE(differs(sequenceOf(wide, 4U, false), sequenceOf(wide, 4U, true))) << "fixed size flag";
  EXPECT_FALSE(differs(sequenceOf(wide, 4U, false), sequenceOf(wide, 4U, false))) << "identical sequences";
  EXPECT_TRUE(differs(sequenceOf(wide, 4U, false), wide)) << "sequence against a number";

  // a struct: a field whose type changed, and a field the other side does not have
  const auto structOf = [](const std::vector<sen::StructField>& fields)
  { return StructType::make({"S", "ns.S", "", fields, {}}); };
  EXPECT_TRUE(differs(structOf({{"f", "", wide}}), structOf({{"f", "", narrow}}))) << "struct field type";
  EXPECT_TRUE(differs(structOf({{"f", "", wide}}), structOf({{"f", "", wide}, {"g", "", wide}})))
    << "a field the other side does not have";
  EXPECT_TRUE(differs(structOf({{"f", "", wide}}), wide)) << "struct against a number";

  // an optional: the type inside it
  EXPECT_TRUE(differs(OptionalType::make({"O", "ns.O", "", wide}), OptionalType::make({"O", "ns.O", "", narrow})))
    << "optional value type";
  EXPECT_TRUE(differs(OptionalType::make({"O", "ns.O", "", wide}), wide)) << "optional against a number";

  // a quantity: element, unit, minimum and maximum, each reported separately
  const auto metre = sen::UnitRegistry::get().searchUnitByName("meter");
  const auto second = sen::UnitRegistry::get().searchUnitByName("second");
  ASSERT_TRUE(metre.has_value());
  ASSERT_TRUE(second.has_value());

  const auto quantity =
    [](auto element, std::optional<const sen::Unit*> unit, std::optional<double> lo, std::optional<double> hi)
  { return QuantityType::make({"Q", "ns.Q", "", std::move(element), unit, lo, hi}); };

  auto base = quantity(sen::Float32Type::get(), metre, 0.0, 10.0);
  EXPECT_FALSE(differs(base, quantity(sen::Float32Type::get(), metre, 0.0, 10.0))) << "identical quantities";
  EXPECT_TRUE(differs(base, quantity(sen::Float64Type::get(), metre, 0.0, 10.0))) << "quantity element";
  EXPECT_TRUE(differs(base, quantity(sen::Float32Type::get(), second, 0.0, 10.0))) << "quantity unit";
  EXPECT_TRUE(differs(base, quantity(sen::Float32Type::get(), std::nullopt, 0.0, 10.0))) << "a unit on one side only";
  EXPECT_TRUE(differs(base, quantity(sen::Float32Type::get(), metre, -1.0, 10.0))) << "minimum value";
  EXPECT_TRUE(differs(base, quantity(sen::Float32Type::get(), metre, 0.0, 99.0))) << "maximum value";
  EXPECT_TRUE(differs(base, wide)) << "quantity against a number";

  // the minimum and the maximum are reported apart, so a swap cannot hide behind one message
  const auto minChanged = differences(base, quantity(sen::Float32Type::get(), metre, -1.0, 10.0));
  const auto maxChanged = differences(base, quantity(sen::Float32Type::get(), metre, 0.0, 99.0));
  ASSERT_FALSE(minChanged.empty());
  ASSERT_FALSE(maxChanged.empty());
  EXPECT_NE(minChanged.front(), maxChanged.front()) << "minimum and maximum must not report the same thing";
}

/// @test
/// What getRuntimeDifferences says about two versions of a class. A class has three kinds of
/// member and each carries several attributes beyond its type, so this walks them one at a time:
/// a difference reported under the wrong name, or not at all, is what an operator has to debug
/// from.
TEST(RuntimeDifferences, ReportsEveryChangeToAClass)
{
  auto wide = sen::UInt32Type::get();
  auto narrow = sen::UInt8Type::get();

  const auto classOf = [](const std::vector<sen::PropertySpec>& properties,
                          const std::vector<sen::MethodSpec>& methods,
                          const std::vector<sen::EventSpec>& events)
  {
    const ClassSpec spec {"C", "ns.C", "", properties, methods, events, constructorSpec(), {}, false, {}, {}};
    return ClassType::make(spec);
  };

  const auto differs = [](const ConstTypeHandle<>& local, const ConstTypeHandle<>& remote)
  { return !sen::kernel::getRuntimeDifferences(local.type(), remote.type()).empty(); };

  const sen::PropertySpec plain {"val", "", wide};
  const sen::MethodSpec plainMethod {{"call", "", {{"arg", "", wide}}}, wide};
  const sen::EventSpec plainEvent {{"happened", "", {{"arg", "", wide}}}};

  auto base = classOf({plain}, {plainMethod}, {plainEvent});
  EXPECT_FALSE(differs(base, classOf({plain}, {plainMethod}, {plainEvent}))) << "identical classes";

  // properties: presence, type, category and transport mode
  EXPECT_TRUE(differs(base, classOf({}, {plainMethod}, {plainEvent}))) << "a property the other side lacks";
  EXPECT_TRUE(differs(base, classOf({{"val", "", narrow}}, {plainMethod}, {plainEvent}))) << "property type";
  EXPECT_TRUE(
    differs(base, classOf({{"val", "", wide, sen::PropertyCategory::dynamicRW}}, {plainMethod}, {plainEvent})))
    << "property category";
  EXPECT_TRUE(differs(base,
                      classOf({{"val", "", wide, sen::PropertyCategory::dynamicRO, sen::TransportMode::confirmed}},
                              {plainMethod},
                              {plainEvent})))
    << "property transport mode";

  // methods: presence, return type, argument type, an argument the other side lacks
  EXPECT_TRUE(differs(base, classOf({plain}, {}, {plainEvent}))) << "a method the other side lacks";
  EXPECT_TRUE(differs(base, classOf({plain}, {{{"call", "", {{"arg", "", wide}}}, narrow}}, {plainEvent})))
    << "method return type";
  EXPECT_TRUE(differs(base, classOf({plain}, {{{"call", "", {{"arg", "", narrow}}}, wide}}, {plainEvent})))
    << "method argument type";
  EXPECT_TRUE(differs(base, classOf({plain}, {{{"call", "", {}}, wide}}, {plainEvent})))
    << "an argument the other side lacks";

  // methods: the attributes beside the signature
  EXPECT_TRUE(differs(
    base, classOf({plain}, {{{"call", "", {{"arg", "", wide}}, sen::TransportMode::multicast}, wide}}, {plainEvent})))
    << "method transport mode";
  EXPECT_TRUE(differs(
    base, classOf({plain}, {{{"call", "", {{"arg", "", wide}}}, wide, sen::Constness::constant}}, {plainEvent})))
    << "method constness";
  EXPECT_TRUE(differs(
    base,
    classOf({plain},
            {{{"call", "", {{"arg", "", wide}}}, wide, sen::Constness::nonConstant, sen::NonPropertyRelated {}, true}},
            {plainEvent})))
    << "method deferred flag";

  // events: presence and argument type
  EXPECT_TRUE(differs(base, classOf({plain}, {plainMethod}, {}))) << "an event the other side lacks";
  EXPECT_TRUE(differs(base, classOf({plain}, {plainMethod}, {{{"happened", "", {{"arg", "", narrow}}}}})))
    << "event argument type";
  EXPECT_TRUE(differs(base, classOf({plain}, {plainMethod}, {{{"happened", "", {}}}})))
    << "an event argument the other side lacks";

  // attributes the method hash does not cover, and which were therefore never reported: gating
  // them on the hash made each unreachable for exactly the difference it exists to describe
  EXPECT_TRUE(differs(
    base,
    classOf(
      {plain},
      {{{"call", "", {{"arg", "", wide}}}, wide, sen::Constness::nonConstant, sen::NonPropertyRelated {}, false, true}},
      {plainEvent})))
    << "method local-only flag";
  EXPECT_TRUE(
    differs(base,
            classOf({plain},
                    {{{"call", "", {{"arg", "", wide}}}, wide, sen::Constness::nonConstant, sen::PropertyGetter {}}},
                    {plainEvent})))
    << "method property relation";
}

/// @test
/// The mappings between a meta type and its wire spec, both directions, for every value. These are
/// switch statements, so a wrong case is a silent mistranslation rather than a failure, and it
/// shows up as the wrong type on the far side of a bus.
TEST(TypeSpecs, MapsEveryIntegralStorageTypeBothWays)
{
  const sen::CustomTypeRegistry natives;
  sen::CustomTypeRegistry remote;

  const std::vector<std::pair<std::string, sen::ConstTypeHandle<sen::IntegralType>>> storages = {
    {"u8", sen::UInt8Type::get()},
    {"i16", sen::Int16Type::get()},
    {"u16", sen::UInt16Type::get()},
    {"i32", sen::Int32Type::get()},
    {"u32", sen::UInt32Type::get()},
    {"i64", sen::Int64Type::get()},
    {"u64", sen::UInt64Type::get()}};

  for (const auto& [label, storage]: storages)
  {
    auto original = EnumType::make({"E", "ns.E", "", {{"one", 1, ""}}, storage});
    const auto rebuilt =
      sen::kernel::buildNonNativeType(sen::kernel::makeCustomTypeSpec(original.type()), natives, remote);
    remote.add(rebuilt);

    ASSERT_TRUE(rebuilt->isEnumType()) << label;
    EXPECT_EQ(rebuilt->asEnumType()->getStorageType().getName(), storage->getName())
      << "an enum stored as " << label << " came back as " << rebuilt->asEnumType()->getStorageType().getName();
  }
}

/// @test
/// The same for unit categories, which is a wider switch and the one where a wrong case turns a
/// mass into a length.
TEST(TypeSpecs, MapsEveryUnitCategoryBothWays)
{
  const sen::CustomTypeRegistry natives;
  sen::CustomTypeRegistry remote;

  // one registered unit per category the registry actually carries
  const std::vector<std::string> unitNames = {
    "meter", "gram", "second", "radian", "kelvin", "pascals", "newton", "hertz", "knot"};

  for (const auto& name: unitNames)
  {
    const auto unit = sen::UnitRegistry::get().searchUnitByName(name);
    ASSERT_TRUE(unit.has_value()) << name << " is expected in the unit registry";

    auto original =
      QuantityType::make({"Q", "ns.Q", "", sen::Float64Type::get(), unit.value(), std::nullopt, std::nullopt});
    const auto rebuilt =
      sen::kernel::buildNonNativeType(sen::kernel::makeCustomTypeSpec(original.type()), natives, remote);
    remote.add(rebuilt);

    ASSERT_TRUE(rebuilt->isQuantityType()) << name;
    const auto rebuiltUnit = rebuilt->asQuantityType()->getUnit();
    ASSERT_TRUE(rebuiltUnit.has_value()) << name << " lost its unit";
    EXPECT_EQ(rebuiltUnit.value()->getName(), unit.value()->getName())
      << name << " came back as " << rebuiltUnit.value()->getName();
    EXPECT_EQ(rebuiltUnit.value()->getCategory(), unit.value()->getCategory()) << name << " changed category";
  }
}

/// @test
/// The unit category as it is written onto the wire. The rebuild resolves a unit by name, so it
/// never reads the category back and a round trip cannot see a wrong one: this asserts the spec
/// itself. Other implementations of this protocol do read the field.
TEST(TypeSpecs, WritesTheRightUnitCategoryOntoTheWire)
{
  const auto categoryOf = [](const std::string& unitName)
  {
    const auto unit = sen::UnitRegistry::get().searchUnitByName(unitName);
    EXPECT_TRUE(unit.has_value()) << unitName;
    auto quantity =
      QuantityType::make({"Q", "ns.Q", "", sen::Float64Type::get(), unit.value(), std::nullopt, std::nullopt});
    const auto spec = sen::kernel::makeCustomTypeSpec(quantity.type());
    const auto* data = std::get_if<sen::kernel::QuantityTypeSpec>(&spec.data);
    EXPECT_NE(data, nullptr) << unitName;
    return data->unit.category;
  };

  EXPECT_EQ(categoryOf("meter"), sen::kernel::UnitCat::length);
  EXPECT_EQ(categoryOf("gram"), sen::kernel::UnitCat::mass);
  EXPECT_EQ(categoryOf("second"), sen::kernel::UnitCat::time);
  EXPECT_EQ(categoryOf("radian"), sen::kernel::UnitCat::angle);
  EXPECT_EQ(categoryOf("kelvin"), sen::kernel::UnitCat::temperature);
  EXPECT_EQ(categoryOf("hertz"), sen::kernel::UnitCat::frequency);
  EXPECT_EQ(categoryOf("newton"), sen::kernel::UnitCat::force);
  EXPECT_EQ(categoryOf("pascals"), sen::kernel::UnitCat::pressure);
  EXPECT_EQ(categoryOf("knot"), sen::kernel::UnitCat::velocity);

  // a quantity with no unit is written as length with an empty name, which is what the reader has
  // to disambiguate on. Pinned because it is a deliberate placeholder rather than a real category.
  auto dimensionless =
    QuantityType::make({"Q", "ns.Q", "", sen::Float64Type::get(), std::nullopt, std::nullopt, std::nullopt});
  const auto spec = sen::kernel::makeCustomTypeSpec(dimensionless.type());
  const auto* data = std::get_if<sen::kernel::QuantityTypeSpec>(&spec.data);
  ASSERT_NE(data, nullptr);
  EXPECT_TRUE(data->unit.name.empty()) << "a dimensionless quantity carries no unit name";
  EXPECT_EQ(data->unit.abbreviation, "none");
}

/// @test
/// A quantity migrated from an older protocol. V4 and V5 encode "no range" as a minimum that is
/// not below the maximum, where the current spec uses empty optionals, so a copy that does not
/// apply the rule turns an unbounded quantity into one whose range admits nothing.
TEST(TypeSpecs, MigratesAQuantityRangeFromOlderProtocols)
{
  const auto migratedV4 = [](double lo, double hi)
  {
    sen::kernel::QuantityTypeSpecV4 quantity {};
    quantity.numericType = sen::kernel::RealType::float64Type;
    quantity.unit.name = "meter";
    quantity.unit.abbreviation = "m";
    quantity.unit.category = sen::kernel::UnitCat::length;
    quantity.minValue = lo;
    quantity.maxValue = hi;

    sen::kernel::CustomTypeSpecV4 v4 {};
    v4.name = "Q";
    v4.qualifiedName = "ns.Q";
    v4.data = quantity;
    return sen::kernel::toCurrentVersion(v4);
  };

  // a real range survives
  {
    const auto migrated = migratedV4(-2.5, 7.5);
    const auto* data = std::get_if<sen::kernel::QuantityTypeSpec>(&migrated.data);
    ASSERT_NE(data, nullptr);
    ASSERT_TRUE(data->minValue.asOptional().has_value());
    ASSERT_TRUE(data->maxValue.asOptional().has_value());
    EXPECT_DOUBLE_EQ(data->minValue.asOptional().value(), -2.5);
    EXPECT_DOUBLE_EQ(data->maxValue.asOptional().value(), 7.5);
  }

  // and "no range", which the older versions spell as a minimum not below the maximum
  {
    const auto migrated = migratedV4(0.0, 0.0);
    const auto* data = std::get_if<sen::kernel::QuantityTypeSpec>(&migrated.data);
    ASSERT_NE(data, nullptr);
    EXPECT_FALSE(data->minValue.asOptional().has_value())
      << "a V4 quantity with minValue >= maxValue has no range, rather than a range of nothing";
    EXPECT_FALSE(data->maxValue.asOptional().has_value());

    const sen::CustomTypeRegistry natives;
    const sen::CustomTypeRegistry remote;
    const auto rebuilt = sen::kernel::buildNonNativeType(migrated, natives, remote);
    ASSERT_TRUE(rebuilt->isQuantityType());
    EXPECT_FALSE(rebuilt->asQuantityType()->getMinValue().has_value());
    EXPECT_FALSE(rebuilt->asQuantityType()->getMaxValue().has_value());
  }
}

/// @test
/// Every remaining kind a type definition can take, migrated from V4. The migration is one visit
/// arm per kind, each copying its own fields, so a kind nobody exercises is a kind nobody has
/// checked.
TEST(TypeSpecs, MigratesEveryKindFromV4)
{
  const auto migrate = [](const sen::kernel::CustomTypeDataV4& data)
  {
    sen::kernel::CustomTypeSpecV4 v4 {};
    v4.name = "T";
    v4.qualifiedName = "ns.T";
    v4.description = "from an older kernel";
    v4.data = data;
    return sen::kernel::toCurrentVersion(v4);
  };

  // alias
  {
    sen::kernel::AliasTypeSpecV4 alias {};
    alias.aliasedType = "u32";
    const auto migrated = migrate(alias);
    const auto* data = std::get_if<sen::kernel::AliasTypeSpec>(&migrated.data);
    ASSERT_NE(data, nullptr) << "alias";
    EXPECT_EQ(data->aliasedType, "u32");
  }

  // optional
  {
    sen::kernel::OptionalTypeSpecV4 optional {};
    optional.type = "u32";
    const auto migrated = migrate(optional);
    const auto* data = std::get_if<sen::kernel::OptionalTypeSpec>(&migrated.data);
    ASSERT_NE(data, nullptr) << "optional";
    EXPECT_EQ(data->type, "u32");
  }

  // struct, including the parent it may declare
  {
    sen::kernel::StructTypeSpecV4 structure {};
    structure.parent = "ns.Base";
    structure.fields.emplace_back();
    structure.fields.back() = {"f", "a field", "u32"};
    const auto migrated = migrate(structure);
    const auto* data = std::get_if<sen::kernel::StructTypeSpec>(&migrated.data);
    ASSERT_NE(data, nullptr) << "struct";
    EXPECT_EQ(data->parent, "ns.Base");
    ASSERT_EQ(data->fields.size(), 1U);
    EXPECT_EQ(data->fields.at(0).name, "f");
    EXPECT_EQ(data->fields.at(0).type, "u32");
  }

  // variant, whose alternatives are keyed rather than positional
  {
    sen::kernel::VariantTypeSpecV4 variant {};
    variant.fields.emplace_back();
    variant.fields.back() = {7, "an alternative", "u32"};
    const auto migrated = migrate(variant);
    const auto* data = std::get_if<sen::kernel::VariantTypeSpec>(&migrated.data);
    ASSERT_NE(data, nullptr) << "variant";
    ASSERT_EQ(data->fields.size(), 1U);
    EXPECT_EQ(data->fields.at(0).key, 7U) << "the key must survive, not become a position";
    EXPECT_EQ(data->fields.at(0).type, "u32");
  }

  // class, with one member of each kind
  {
    sen::kernel::ClassTypeSpecV4 klass {};
    klass.isInterface = false;

    klass.properties.emplace_back();
    klass.properties.back().name = "val";
    klass.properties.back().type = "u32";

    klass.methods.emplace_back();
    klass.methods.back().name = "call";
    klass.methods.back().returnTypeId = "u32";
    klass.methods.back().deferred = true;
    klass.methods.back().args.emplace_back();
    klass.methods.back().args.back() = {"arg", "", "u32"};

    klass.events.emplace_back();
    klass.events.back().name = "happened";
    klass.events.back().args.emplace_back();
    klass.events.back().args.back() = {"arg", "", "u32"};

    klass.constructor.name = "constructorT";

    const auto migrated = migrate(klass);
    const auto* data = std::get_if<sen::kernel::ClassTypeSpec>(&migrated.data);
    ASSERT_NE(data, nullptr) << "class";
    ASSERT_EQ(data->properties.size(), 1U);
    EXPECT_EQ(data->properties.at(0).name, "val");
    ASSERT_EQ(data->methods.size(), 1U);
    EXPECT_EQ(data->methods.at(0).name, "call");
    EXPECT_TRUE(data->methods.at(0).deferred) << "the deferred flag must survive the migration";
    ASSERT_EQ(data->methods.at(0).args.size(), 1U);
    EXPECT_EQ(data->methods.at(0).args.at(0).name, "arg");
    ASSERT_EQ(data->events.size(), 1U);
    EXPECT_EQ(data->events.at(0).name, "happened");
    EXPECT_EQ(data->constructor.name, "constructorT");
  }
}

/// @test
/// The same kinds from V5, whose method carries localOnly where V4 carried deferred, and whose
/// class gained the parent list. Each version drops what the other has, so what survives a
/// migration is not symmetric and is worth stating.
TEST(TypeSpecs, MigratesEveryKindFromV5)
{
  const auto migrate = [](const sen::kernel::CustomTypeDataV5& data)
  {
    sen::kernel::CustomTypeSpecV5 v5 {};
    v5.name = "T";
    v5.qualifiedName = "ns.T";
    v5.description = "from a less old kernel";
    v5.data = data;
    return sen::kernel::toCurrentVersion(v5);
  };

  {
    sen::kernel::AliasTypeSpecV5 alias {};
    alias.aliasedType = "u32";
    const auto migrated = migrate(alias);
    const auto* data = std::get_if<sen::kernel::AliasTypeSpec>(&migrated.data);
    ASSERT_NE(data, nullptr) << "alias";
    EXPECT_EQ(data->aliasedType, "u32");
  }

  {
    sen::kernel::OptionalTypeSpecV5 optional {};
    optional.type = "u32";
    const auto migrated = migrate(optional);
    const auto* data = std::get_if<sen::kernel::OptionalTypeSpec>(&migrated.data);
    ASSERT_NE(data, nullptr) << "optional";
    EXPECT_EQ(data->type, "u32");
  }

  {
    sen::kernel::StructTypeSpecV5 structure {};
    structure.parent = "ns.Base";
    structure.fields.emplace_back();
    structure.fields.back() = {"f", "a field", "u32", 0U};
    const auto migrated = migrate(structure);
    const auto* data = std::get_if<sen::kernel::StructTypeSpec>(&migrated.data);
    ASSERT_NE(data, nullptr) << "struct";
    EXPECT_EQ(data->parent, "ns.Base");
    ASSERT_EQ(data->fields.size(), 1U);
    EXPECT_EQ(data->fields.at(0).name, "f");
  }

  {
    sen::kernel::VariantTypeSpecV5 variant {};
    variant.fields.emplace_back();
    variant.fields.back() = {7, "an alternative", "u32", 0U};
    const auto migrated = migrate(variant);
    const auto* data = std::get_if<sen::kernel::VariantTypeSpec>(&migrated.data);
    ASSERT_NE(data, nullptr) << "variant";
    ASSERT_EQ(data->fields.size(), 1U);
    EXPECT_EQ(data->fields.at(0).key, 7U) << "the key must survive, not become a position";
  }

  {
    sen::kernel::ClassTypeSpecV5 klass {};
    klass.parents.emplace_back("ns.Base");

    klass.properties.emplace_back();
    klass.properties.back().name = "val";
    klass.properties.back().type = "u32";

    klass.methods.emplace_back();
    klass.methods.back().name = "call";
    klass.methods.back().returnTypeId = "u32";
    klass.methods.back().localOnly = true;

    klass.events.emplace_back();
    klass.events.back().name = "happened";

    klass.constructor.name = "constructorT";

    const auto migrated = migrate(klass);
    const auto* data = std::get_if<sen::kernel::ClassTypeSpec>(&migrated.data);
    ASSERT_NE(data, nullptr) << "class";
    ASSERT_EQ(data->parents.size(), 1U) << "V5 carries a parent list, unlike V4";
    EXPECT_EQ(data->parents.at(0), "ns.Base");
    ASSERT_EQ(data->methods.size(), 1U);
    EXPECT_TRUE(data->methods.at(0).localOnly) << "V5 carries localOnly where V4 carried deferred";
    EXPECT_FALSE(data->methods.at(0).deferred) << "V5 has no deferred flag, so it takes the default";
    ASSERT_EQ(data->properties.size(), 1U);
    ASSERT_EQ(data->events.size(), 1U);
    EXPECT_EQ(data->constructor.name, "constructorT");
  }
}

/// @test
/// Every arm of the three mapping switches that translate a meta value into its wire form. A wrong
/// case is silent: the value is written, read back, and means something else on the far side. The
/// unit categories are built directly rather than looked up, as the registry lacks some of them.
TEST(TypeSpecs, MapsEveryEnumeratedValueOntoTheWire)
{
  // every unit category
  const std::vector<std::pair<UnitCategory, sen::kernel::UnitCat>> categories = {
    {UnitCategory::length, sen::kernel::UnitCat::length},
    {UnitCategory::mass, sen::kernel::UnitCat::mass},
    {UnitCategory::time, sen::kernel::UnitCat::time},
    {UnitCategory::angle, sen::kernel::UnitCat::angle},
    {UnitCategory::temperature, sen::kernel::UnitCat::temperature},
    {UnitCategory::density, sen::kernel::UnitCat::density},
    {UnitCategory::pressure, sen::kernel::UnitCat::pressure},
    {UnitCategory::area, sen::kernel::UnitCat::area},
    {UnitCategory::force, sen::kernel::UnitCat::force},
    {UnitCategory::frequency, sen::kernel::UnitCat::frequency},
    {UnitCategory::velocity, sen::kernel::UnitCat::velocity},
    {UnitCategory::angularVelocity, sen::kernel::UnitCat::angularVelocity},
    {UnitCategory::acceleration, sen::kernel::UnitCat::acceleration},
    {UnitCategory::angularAcceleration, sen::kernel::UnitCat::angularAcceleration},
    {UnitCategory::torque, sen::kernel::UnitCat::torque}};

  for (const auto& [category, expected]: categories)
  {
    auto unit = sen::Unit::make(UnitSpec {category, "u", "us", "u", 1.0, 0.0, 0.0});
    auto quantity =
      QuantityType::make({"Q", "ns.Q", "", sen::Float64Type::get(), unit.get(), std::nullopt, std::nullopt});
    const auto spec = sen::kernel::makeCustomTypeSpec(quantity.type());
    const auto* data = std::get_if<sen::kernel::QuantityTypeSpec>(&spec.data);
    ASSERT_NE(data, nullptr);
    EXPECT_EQ(data->unit.category, expected)
      << "unit category " << static_cast<int>(category) << " was written as " << static_cast<int>(data->unit.category);
  }

  // every property category and transport mode, read back off the wire form of a class
  const std::vector<std::pair<sen::PropertyCategory, sen::kernel::PropertyCategorySpec>> propertyCategories = {
    {sen::PropertyCategory::staticRO, sen::kernel::PropertyCategorySpec::staticRO},
    {sen::PropertyCategory::staticRW, sen::kernel::PropertyCategorySpec::staticRW},
    {sen::PropertyCategory::dynamicRO, sen::kernel::PropertyCategorySpec::dynamicRO},
    {sen::PropertyCategory::dynamicRW, sen::kernel::PropertyCategorySpec::dynamicRW}};

  const std::vector<std::pair<sen::TransportMode, sen::kernel::TransportModeSpec>> transportModes = {
    {sen::TransportMode::unicast, sen::kernel::TransportModeSpec::unicast},
    {sen::TransportMode::multicast, sen::kernel::TransportModeSpec::multicast},
    {sen::TransportMode::confirmed, sen::kernel::TransportModeSpec::confirmed}};

  for (const auto& [category, expectedCategory]: propertyCategories)
  {
    for (const auto& [mode, expectedMode]: transportModes)
    {
      const sen::PropertySpec property {"val", "", sen::UInt32Type::get(), category, mode};
      const ClassSpec spec {"C", "ns.C", "", {property}, {}, {}, constructorSpec(), {}, false, {}, {}};
      auto klass = ClassType::make(spec);

      const auto typeSpec = sen::kernel::makeCustomTypeSpec(klass.type());
      const auto* data = std::get_if<sen::kernel::ClassTypeSpec>(&typeSpec.data);
      ASSERT_NE(data, nullptr);
      ASSERT_EQ(data->properties.size(), 1U);
      EXPECT_EQ(data->properties.at(0).category, expectedCategory)
        << "property category " << static_cast<int>(category);
      EXPECT_EQ(data->properties.at(0).transportMode, expectedMode) << "transport mode " << static_cast<int>(mode);

      // and back again, which is a separate switch and so a separate chance to be wrong
      const sen::CustomTypeRegistry natives;
      const sen::CustomTypeRegistry remote;
      const auto rebuilt = sen::kernel::buildNonNativeType(typeSpec, natives, remote);
      ASSERT_TRUE(rebuilt->isClassType());
      const auto* rebuiltProperty = rebuilt->asClassType()->searchPropertyByName("val");
      ASSERT_NE(rebuiltProperty, nullptr);
      EXPECT_EQ(rebuiltProperty->getCategory(), category) << "category did not survive the return trip";
      EXPECT_EQ(rebuiltProperty->getTransportMode(), mode) << "transport mode did not survive the return trip";
    }
  }
}
