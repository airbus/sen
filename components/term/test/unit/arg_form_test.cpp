// === arg_form_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "arg_form.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/var.h"

// generated code
#include "stl/test_object.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace sen::components::term
{
namespace
{

//--------------------------------------------------------------------------------------------------------------
// isScalarType / methodIsFormCompatible
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A bool, an integer, a float, a string and a duration are all scalar types.
TEST(ArgForm, ScalarPrimitivesAreScalar)
{
  EXPECT_TRUE(isScalarType(BoolType::get()));
  EXPECT_TRUE(isScalarType(Int32Type::get()));
  EXPECT_TRUE(isScalarType(UInt64Type::get()));
  EXPECT_TRUE(isScalarType(Float64Type::get()));
  EXPECT_TRUE(isScalarType(StringType::get()));
  EXPECT_TRUE(isScalarType(DurationType::get()));
}

/// @test
/// A sequence and a struct are not scalar types.
TEST(ArgForm, CompositeTypesAreNotScalar)
{
  auto seq = SequenceType::make(SequenceSpec {"Seq", "ns.Seq", "", Int32Type::get()});
  EXPECT_FALSE(isScalarType(seq));

  auto str = StructType::make(StructSpec {"S", "ns.S", "", {{"x", "", Int32Type::get()}}});
  EXPECT_FALSE(isScalarType(str));
}

/// @test
/// An optional wrapping a scalar is scalar.
TEST(ArgForm, OptionalOfScalarUnwrapsToScalar)
{
  auto opt = OptionalType::make(OptionalSpec {"O", "ns.O", "", Int32Type::get()});
  EXPECT_TRUE(isScalarType(opt));
}

/// @test
/// An optional wrapping a composite is not scalar.
TEST(ArgForm, OptionalOfCompositeIsNotScalar)
{
  auto seq = SequenceType::make(SequenceSpec {"Seq", "ns.Seq", "", Int32Type::get()});
  auto opt = OptionalType::make(OptionalSpec {"O", "ns.O", "", seq});
  EXPECT_FALSE(isScalarType(opt));
}

/// @test
/// A method taking scalars is form compatible, and one taking no arguments trivially is.
TEST(ArgForm, TestMethodsAreFormCompatible)
{
  // `add(a: i32, b: i32)` and `echo(message: string)` should both be form-compatible.
  const auto& cls = *::term::test::TestObjectInterface::meta();
  EXPECT_TRUE(methodIsFormCompatible(*cls.searchMethodByName("add")));
  EXPECT_TRUE(methodIsFormCompatible(*cls.searchMethodByName("echo")));
  EXPECT_TRUE(methodIsFormCompatible(*cls.searchMethodByName("ping")));   // no args, trivially compatible
  EXPECT_TRUE(methodIsFormCompatible(*cls.searchMethodByName("reset")));  // no args, trivially compatible
}

//--------------------------------------------------------------------------------------------------------------
// defaultTextFor
//--------------------------------------------------------------------------------------------------------------

/// @test
/// Each primitive has a default text: false for a bool, zero for a number or duration, and
/// nothing for a string.
TEST(ArgForm, DefaultsForPrimitives)
{
  EXPECT_EQ(defaultTextFor(BoolType::get()), "false");
  EXPECT_EQ(defaultTextFor(Int32Type::get()), "0");
  EXPECT_EQ(defaultTextFor(Float64Type::get()), "0");
  EXPECT_EQ(defaultTextFor(StringType::get()), "");
  EXPECT_EQ(defaultTextFor(DurationType::get()), "0");
}

/// @test
/// An enumeration defaults to its first enumerator.
TEST(ArgForm, DefaultForEnumIsFirstEnumerator)
{
  auto e =
    EnumType::make(EnumSpec {"E", "ns.E", "", {{"alpha", 0, ""}, {"beta", 1, ""}, {"gamma", 2, ""}}, UInt8Type::get()});
  EXPECT_EQ(defaultTextFor(e), "alpha");
}

/// @test
/// An optional takes the default of the type it wraps.
TEST(ArgForm, DefaultForOptionalUnwraps)
{
  auto opt = OptionalType::make(OptionalSpec {"O", "ns.O", "", Int32Type::get()});
  EXPECT_EQ(defaultTextFor(opt), "0");
}

//--------------------------------------------------------------------------------------------------------------
// formatInlineArg
//--------------------------------------------------------------------------------------------------------------

/// @test
/// An integer formats as its digits, keeping a minus sign.
TEST(ArgForm, InlineArgInteger)
{
  EXPECT_EQ(formatInlineArg(Var(int32_t {42}), Int32Type::get()), "42");
  EXPECT_EQ(formatInlineArg(Var(int32_t {-7}), Int32Type::get()), "-7");
}

/// @test
/// A bool formats as true or false.
TEST(ArgForm, InlineArgBool)
{
  EXPECT_EQ(formatInlineArg(Var(true), BoolType::get()), "true");
  EXPECT_EQ(formatInlineArg(Var(false), BoolType::get()), "false");
}

/// @test
/// A string formats inside quotes.
TEST(ArgForm, InlineArgStringIsQuoted)
{
  EXPECT_EQ(formatInlineArg(Var(std::string("hello world")), StringType::get()), "\"hello world\"");
}

/// @test
/// A quote, a backslash and a newline in a string are escaped, so the result can be typed back.
TEST(ArgForm, InlineArgStringEscapesMetacharacters)
{
  EXPECT_EQ(formatInlineArg(Var(std::string("a\"b")), StringType::get()), "\"a\\\"b\"");
  EXPECT_EQ(formatInlineArg(Var(std::string("a\\b")), StringType::get()), "\"a\\\\b\"");
  EXPECT_EQ(formatInlineArg(Var(std::string("a\nb")), StringType::get()), "\"a\\nb\"");
}

/// @test
/// An enumerator formats as its name, whether it is held as a name or as its integer key.
TEST(ArgForm, InlineArgEnumUsesName)
{
  auto e = EnumType::make(EnumSpec {"Col", "ns.Col", "", {{"red", 0, ""}, {"blue", 1, ""}}, UInt8Type::get()});
  EXPECT_EQ(formatInlineArg(Var(std::string("red")), e), "\"red\"");
  // Stored as integer key, still rendered as the name.
  EXPECT_EQ(formatInlineArg(Var(uint8_t {1}), e), "\"blue\"");
}

/// @test
/// A duration formats as seconds with its unit, so reading it back gives the same duration.
TEST(ArgForm, InlineArgDurationIsSecondsWithUnit)
{
  // Duration inline format is a quoted string with unit ("N s") so the round-trip through
  // Unit::fromString lands back at the same number of seconds rather than the storage-default
  // nanoseconds.
  auto dur = Duration(std::chrono::milliseconds(1500));
  auto formatted = formatInlineArg(Var(dur), DurationType::get());
  EXPECT_EQ(formatted.front(), '"');
  EXPECT_EQ(formatted.back(), '"');
  EXPECT_NE(formatted.find("1.5"), std::string::npos);
  EXPECT_NE(formatted.find(" s"), std::string::npos);
}

/// @test
/// A duration formatted and parsed again is the same duration.
TEST(ArgForm, DurationRoundTripPreservesSeconds)
{
  // Build a duration, format inline, reparse, the value we get back should be 1.5 seconds.
  auto dur = Duration(std::chrono::milliseconds(1500));
  auto formatted = formatInlineArg(Var(dur), DurationType::get());

  auto doc = std::string("{ \"v\": ") + formatted + " }";
  auto parsed = fromJson(doc).get<VarMap>();
  auto& roundTrip = parsed.at("v");
  ASSERT_TRUE(impl::adaptVariant(*DurationType::get(), roundTrip).isOk());
  auto back = roundTrip.getCopyAs<Duration>();
  EXPECT_DOUBLE_EQ(back.toSeconds(), 1.5);
}

//--------------------------------------------------------------------------------------------------------------
// formatInlineInvocation
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A method with no arguments formats as an empty argument list.
TEST(ArgForm, InvocationWithNoArgs)
{
  const auto& cls = *::term::test::TestObjectInterface::meta();
  const auto* ping = cls.searchMethodByName("ping");
  ASSERT_NE(ping, nullptr);

  auto out = formatInlineInvocation("obj", "ping", ping->getArgs(), {});
  EXPECT_EQ(out, "obj.ping");
}

/// @test
/// A method with two integers formats both, separated.
TEST(ArgForm, InvocationWithTwoIntegers)
{
  const auto& cls = *::term::test::TestObjectInterface::meta();
  const auto* add = cls.searchMethodByName("add");
  ASSERT_NE(add, nullptr);

  VarList values = {Var(int32_t {3}), Var(int32_t {4})};
  auto out = formatInlineInvocation("calc", "add", add->getArgs(), values);
  EXPECT_EQ(out, "calc.add 3 4");
}

/// @test
/// A method taking a string formats it quoted.
TEST(ArgForm, InvocationWithString)
{
  const auto& cls = *::term::test::TestObjectInterface::meta();
  const auto* echo = cls.searchMethodByName("echo");
  ASSERT_NE(echo, nullptr);

  VarList values = {Var(std::string("hello"))};
  auto out = formatInlineInvocation("e", "echo", echo->getArgs(), values);
  EXPECT_EQ(out, "e.echo \"hello\"");
}

//--------------------------------------------------------------------------------------------------------------
// ArgForm, stateful form
//--------------------------------------------------------------------------------------------------------------

class ArgFormStateful: public ::testing::Test
{
protected:
  [[nodiscard]] const Method& addMethod() const
  {
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("add");
  }
  [[nodiscard]] const Method& echoMethod() const
  {
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("echo");
  }
  [[nodiscard]] const Method& pingMethod() const
  {
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("ping");
  }
};

/// @test
/// A form has one field per argument, names the object and method, and starts on the first field.
TEST_F(ArgFormStateful, BuildProducesOneFieldPerArg)
{
  auto form = ArgForm::build(addMethod(), "calc");
  ASSERT_TRUE(form.has_value());
  EXPECT_EQ(form->fields().size(), 2U);
  EXPECT_EQ(form->objectName(), "calc");
  EXPECT_EQ(form->methodName(), "add");
  EXPECT_EQ(form->focusedIndex(), 0U);
}

/// @test
/// With nothing prefilled, every field holds its default and is not marked as edited.
TEST_F(ArgFormStateful, FieldsGetDefaultsWhenNothingPrefilled)
{
  auto form = ArgForm::build(addMethod(), "calc");
  ASSERT_TRUE(form.has_value());
  for (const auto& f: form->fields())
  {
    EXPECT_EQ(f.text, "0");
    EXPECT_FALSE(f.userEdited);
  }
}

/// @test
/// A prefilled argument fills its field and marks it edited, the rest take defaults, and focus
/// lands on the first unfilled field.
TEST_F(ArgFormStateful, PrefilledValuesPopulateFieldsAndFocusNext)
{
  std::vector<Var> prefilled = {Var(int32_t {5})};  // provide only the first arg
  auto form = ArgForm::build(addMethod(), "calc", prefilled);
  ASSERT_TRUE(form.has_value());

  EXPECT_EQ(form->fields()[0].text, "5");
  EXPECT_TRUE(form->fields()[0].userEdited);
  EXPECT_EQ(form->fields()[1].text, "0");  // default
  EXPECT_FALSE(form->fields()[1].userEdited);
  EXPECT_EQ(form->focusedIndex(), 1U);  // cursor at first unfilled
}

/// @test
/// Focus moves forward and backward and wraps at either end.
TEST_F(ArgFormStateful, FocusNextAndPrevWrapAround)
{
  auto form = ArgForm::build(addMethod(), "calc");
  ASSERT_TRUE(form.has_value());

  EXPECT_EQ(form->focusedIndex(), 0U);
  form->focusNext();
  EXPECT_EQ(form->focusedIndex(), 1U);
  form->focusNext();  // wraps
  EXPECT_EQ(form->focusedIndex(), 0U);
  form->focusPrev();  // wraps backward
  EXPECT_EQ(form->focusedIndex(), 1U);
}

/// @test
/// Typing changes the focused field and leaves the others alone.
TEST_F(ArgFormStateful, InsertTextMutatesFocusedFieldOnly)
{
  auto form = ArgForm::build(addMethod(), "calc");
  ASSERT_TRUE(form.has_value());

  form->clearField();
  form->insertText("42");

  EXPECT_EQ(form->fields()[0].text, "42");
  EXPECT_TRUE(form->fields()[0].userEdited);
  EXPECT_EQ(form->fields()[1].text, "0");  // untouched
}

/// @test
/// Backspace removes the last character of the focused field.
TEST_F(ArgFormStateful, BackspaceDropsTrailingCharacter)
{
  auto form = ArgForm::build(addMethod(), "calc");
  ASSERT_TRUE(form.has_value());

  form->clearField();
  form->insertText("42");
  form->backspace();
  EXPECT_EQ(form->fields()[0].text, "4");
}

/// @test
/// Submitting returns the typed text parsed into the argument types.
TEST_F(ArgFormStateful, SubmitReturnsParsedValues)
{
  std::vector<Var> prefilled = {Var(int32_t {3}), Var(int32_t {4})};
  auto form = ArgForm::build(addMethod(), "calc", prefilled);
  ASSERT_TRUE(form.has_value());

  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk());
  const auto& values = result.getValue();
  ASSERT_EQ(values.size(), 2U);
  EXPECT_EQ(values[0].get<int32_t>(), 3);
  EXPECT_EQ(values[1].get<int32_t>(), 4);
}

/// @test
/// Submitting with an unparsable field fails, naming which field and why.
TEST_F(ArgFormStateful, SubmitFailsOnInvalidField)
{
  auto form = ArgForm::build(addMethod(), "calc");
  ASSERT_TRUE(form.has_value());

  // Put garbage in the second field.
  form->focusNext();
  form->clearField();
  form->insertText("not-a-number");

  auto result = form->trySubmit();
  ASSERT_TRUE(result.isError());
  EXPECT_EQ(result.getError().fieldIndex, 1U);
  EXPECT_FALSE(result.getError().message.empty());
}

/// @test
/// Submitting with a required field left empty fails, naming that field.
TEST_F(ArgFormStateful, SubmitEmptyRequiredFieldFails)
{
  auto form = ArgForm::build(addMethod(), "calc");
  ASSERT_TRUE(form.has_value());

  form->clearField();
  auto result = form->trySubmit();
  ASSERT_TRUE(result.isError());
  EXPECT_EQ(result.getError().fieldIndex, 0U);
}

/// @test
/// A string field takes raw text, with no quotes needed.
TEST_F(ArgFormStateful, StringFieldAcceptsRawTextWithoutQuotes)
{
  auto form = ArgForm::build(echoMethod(), "e");
  ASSERT_TRUE(form.has_value());

  form->clearField();
  form->insertText("hello world");  // note: no surrounding quotes
  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk());
  EXPECT_EQ(result.getValue()[0].get<std::string>(), "hello world");
}

/// @test
/// A method with no arguments builds an empty form that submits an empty argument list.
TEST_F(ArgFormStateful, NoArgsMethodBuildsEmptyForm)
{
  auto form = ArgForm::build(pingMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  EXPECT_EQ(form->fields().size(), 0U);

  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk());
  EXPECT_TRUE(result.getValue().empty());
}

/// @test
/// An enumerator is accepted written as a bare name.
TEST_F(ArgFormStateful, EnumFieldAcceptsBareName)
{
  // Bare enum name (no quotes) should parse correctly.
  auto e =
    EnumType::make(EnumSpec {"E", "ns.E", "", {{"alpha", 0, ""}, {"beta", 1, ""}, {"gamma", 2, ""}}, UInt8Type::get()});
  EXPECT_EQ(defaultTextFor(e), "alpha");
  Var v(std::string("beta"));
  ASSERT_TRUE(impl::adaptVariant(*e, v).isOk());
}

/// @test
/// The first keystroke replaces the default rather than appending to it.
TEST_F(ArgFormStateful, FirstKeystrokeReplacesDefaultPlaceholder)
{
  // First keystroke replaces the default placeholder, not appends to it.
  auto form = ArgForm::build(addMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  ASSERT_EQ(form->fields()[0].text, "0");
  ASSERT_FALSE(form->fields()[0].userEdited);

  form->insertText("5");
  EXPECT_EQ(form->fields()[0].text, "5");
  EXPECT_TRUE(form->fields()[0].userEdited);
}

/// @test
/// Later keystrokes append to what has been typed.
TEST_F(ArgFormStateful, SubsequentKeystrokesAppendAsNormal)
{
  auto form = ArgForm::build(addMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->insertText("5");
  form->insertText("7");
  EXPECT_EQ(form->fields()[0].text, "57");
}

/// @test
/// The first backspace clears the default rather than editing it.
TEST_F(ArgFormStateful, FirstBackspaceClearsDefaultPlaceholder)
{
  // Backspace on an untouched default clears the entire placeholder.
  auto form = ArgForm::build(addMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  ASSERT_EQ(form->fields()[0].text, "0");

  form->backspace();
  EXPECT_EQ(form->fields()[0].text, "");
  EXPECT_TRUE(form->fields()[0].userEdited);
}

/// @test
/// Backspace removes a whole character, not one byte of it.
TEST_F(ArgFormStateful, BackspaceHandlesUtf8Multibyte)
{
  auto form = ArgForm::build(echoMethod(), "e");
  ASSERT_TRUE(form.has_value());

  form->clearField();
  form->insertText("héllo");  // é is 2 bytes in UTF-8
  form->backspace();          // should remove "o"
  EXPECT_EQ(form->fields()[0].text, "héll");
  form->backspace();  // "l"
  form->backspace();  // "l"
  form->backspace();  // "é", both bytes go
  EXPECT_EQ(form->fields()[0].text, "h");
}

//--------------------------------------------------------------------------------------------------------------
// effectiveDescription fallback
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A field uses the argument's own description when it has one.
TEST(ArgFormEffectiveDescription, PrefersExplicitDescription)
{
  // Explicit description takes priority over the type's built-in description.
  EXPECT_EQ(effectiveDescription("a user-chosen flag", BoolType::get()), "a user-chosen flag");
}

/// @test
/// A field with no description of its own falls back to the type's.
TEST(ArgFormEffectiveDescription, FallsBackToTypeDescriptionWhenEmpty)
{
  auto desc = effectiveDescription("", BoolType::get());
  EXPECT_FALSE(desc.empty());  // something from BoolType::getDescription()
}

//--------------------------------------------------------------------------------------------------------------
// Struct-typed arguments, the composite-editor path
//--------------------------------------------------------------------------------------------------------------

class ArgFormStruct: public ::testing::Test
{
protected:
  [[nodiscard]] const Method& movePointMethod() const
  {
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("movePoint");
  }
};

/// @test
/// A struct argument becomes one composite field with a child per member, and only the children
/// count as leaves.
TEST_F(ArgFormStruct, BuildExpandsStructIntoChildren)
{
  auto form = ArgForm::build(movePointMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  // One top-level field (the `p` argument), which is a composite with two children.
  ASSERT_EQ(form->fields().size(), 1U);
  const auto& pField = form->fields()[0];
  EXPECT_EQ(pField.name, "p");
  EXPECT_EQ(pField.typeName, "Point");
  EXPECT_FALSE(isLeaf(pField));
  ASSERT_EQ(pField.children.size(), 2U);
  EXPECT_EQ(pField.children[0].name, "x");
  EXPECT_EQ(pField.children[1].name, "y");

  // Leaf count is 2, the two scalar sub-fields, not the composite group itself.
  EXPECT_EQ(form->leafCount(), 2U);
}

/// @test
/// Every leaf of a nested field takes its own default.
TEST_F(ArgFormStruct, LeafDefaultsPropagate)
{
  auto form = ArgForm::build(movePointMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  // Scalar defaults applied to every leaf regardless of nesting depth.
  EXPECT_EQ(form->fields()[0].children[0].text, "0");
  EXPECT_EQ(form->fields()[0].children[1].text, "0");
}

/// @test
/// A struct value typed on the command line fills the matching leaves.
TEST_F(ArgFormStruct, PrefillFromVarMapPopulatesLeaves)
{
  // Simulate the user having typed a partial struct value on the command line:
  //   obj.movePoint {"x": 3, "y": 7}
  VarMap prefill;
  prefill.try_emplace("x", int32_t {3});
  prefill.try_emplace("y", int32_t {7});

  std::vector<Var> values = {Var(std::move(prefill))};
  auto form = ArgForm::build(movePointMethod(), "obj", values);
  ASSERT_TRUE(form.has_value());

  const auto& pField = form->fields()[0];
  EXPECT_EQ(pField.children[0].text, "3");
  EXPECT_EQ(pField.children[1].text, "7");
  EXPECT_TRUE(pField.children[0].userEdited);
  EXPECT_TRUE(pField.children[1].userEdited);
}

/// @test
/// Focus moves through the nested leaves rather than stopping at the composite.
TEST_F(ArgFormStruct, FocusNavigatesThroughNestedLeaves)
{
  auto form = ArgForm::build(movePointMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  // Starts at the first leaf, `p.x`.
  EXPECT_EQ(form->focusedIndex(), 0U);
  EXPECT_EQ(form->focusedField().name, "x");

  form->focusNext();
  EXPECT_EQ(form->focusedField().name, "y");

  form->focusNext();  // wraps back to x
  EXPECT_EQ(form->focusedField().name, "x");

  form->focusPrev();  // wraps to y
  EXPECT_EQ(form->focusedField().name, "y");
}

/// @test
/// Typing changes the focused leaf and leaves the other leaves alone.
TEST_F(ArgFormStruct, InsertTextTargetsFocusedLeafOnly)
{
  auto form = ArgForm::build(movePointMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->clearField();
  form->insertText("42");
  form->focusNext();
  form->clearField();
  form->insertText("9");

  EXPECT_EQ(form->fields()[0].children[0].text, "42");
  EXPECT_EQ(form->fields()[0].children[1].text, "9");
}

/// @test
/// Submitting assembles the leaves back into a struct value.
TEST_F(ArgFormStruct, SubmitAssemblesVarMap)
{
  auto form = ArgForm::build(movePointMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->clearField();
  form->insertText("5");
  form->focusNext();
  form->clearField();
  form->insertText("11");

  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk());
  const auto& values = result.getValue();
  ASSERT_EQ(values.size(), 1U);

  const auto& map = values[0].get<VarMap>();
  ASSERT_NE(map.find("x"), map.end());
  ASSERT_NE(map.find("y"), map.end());
  EXPECT_EQ(map.at("x").get<int32_t>(), 5);
  EXPECT_EQ(map.at("y").get<int32_t>(), 11);
}

/// @test
/// Submitting with an unparsable nested leaf fails, naming that leaf.
TEST_F(ArgFormStruct, SubmitFailsOnInvalidNestedLeaf)
{
  auto form = ArgForm::build(movePointMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  // Put garbage in the second leaf (y).
  form->focusNext();
  form->clearField();
  form->insertText("not-a-number");

  auto result = form->trySubmit();
  ASSERT_TRUE(result.isError());
  EXPECT_EQ(result.getError().fieldIndex, 1U);  // y is leaf index 1
  EXPECT_FALSE(result.getError().message.empty());
}

//--------------------------------------------------------------------------------------------------------------
// Specialized editors: bool + enum
//--------------------------------------------------------------------------------------------------------------

class ArgFormEditors: public ::testing::Test
{
protected:
  [[nodiscard]] const Method& configureMethod() const
  {
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("configure");
  }
  [[nodiscard]] const Method& addMethod() const
  {
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("add");
  }
};

/// @test
/// A bool leaf is edited as a boolean.
TEST_F(ArgFormEditors, BoolLeafClassifiedAsBooleanEditor)
{
  auto form = ArgForm::build(configureMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  ASSERT_EQ(form->leafCount(), 2U);
  EXPECT_EQ(form->fields()[0].editor, EditorKind::boolean);
  EXPECT_EQ(form->fields()[0].text, "false");  // default
}

/// @test
/// An enumeration leaf is edited as an enumeration.
TEST_F(ArgFormEditors, EnumLeafClassifiedAsEnumerationEditor)
{
  auto form = ArgForm::build(configureMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  EXPECT_EQ(form->fields()[1].editor, EditorKind::enumeration);
  EXPECT_EQ(form->fields()[1].text, "red");  // first enumerator
}

/// @test
/// An integer leaf is edited as a spinner.
TEST_F(ArgFormEditors, IntLeafClassifiedAsIntegerSpinEditor)
{
  auto form = ArgForm::build(addMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  EXPECT_EQ(form->fields()[0].editor, EditorKind::integerSpin);
  EXPECT_EQ(form->fields()[1].editor, EditorKind::integerSpin);
}

/// @test
/// Toggling a bool field flips it.
TEST_F(ArgFormEditors, ToggleFocusedFlipsBool)
{
  auto form = ArgForm::build(configureMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  EXPECT_EQ(form->focusedField().text, "false");
  form->toggleFocused();
  EXPECT_EQ(form->focusedField().text, "true");
  form->toggleFocused();
  EXPECT_EQ(form->focusedField().text, "false");
}

/// @test
/// Cycling a bool field flips it.
TEST_F(ArgFormEditors, CycleFocusedFlipsBool)
{
  auto form = ArgForm::build(configureMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  // cycleFocused on a bool is equivalent to toggling (only two options).
  form->cycleFocused(true);
  EXPECT_EQ(form->focusedField().text, "true");
  form->cycleFocused(false);
  EXPECT_EQ(form->focusedField().text, "false");
}

/// @test
/// Cycling an enumeration forward moves to the next enumerator.
TEST_F(ArgFormEditors, CycleFocusedWalksEnumForward)
{
  auto form = ArgForm::build(configureMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->focusNext();  // tint
  EXPECT_EQ(form->focusedField().text, "red");

  form->cycleFocused(true);
  EXPECT_EQ(form->focusedField().text, "green");
  form->cycleFocused(true);
  EXPECT_EQ(form->focusedField().text, "blue");
  form->cycleFocused(true);  // wraps
  EXPECT_EQ(form->focusedField().text, "red");
}

/// @test
/// Cycling an enumeration backward moves to the previous enumerator.
TEST_F(ArgFormEditors, CycleFocusedWalksEnumBackward)
{
  auto form = ArgForm::build(configureMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->focusNext();          // tint
  form->cycleFocused(false);  // wraps from first → last
  EXPECT_EQ(form->focusedField().text, "blue");
  form->cycleFocused(false);
  EXPECT_EQ(form->focusedField().text, "green");
}

/// @test
/// Toggling an enumeration does nothing, since toggling is for booleans.
TEST_F(ArgFormEditors, ToggleNoOpOnEnum)
{
  auto form = ArgForm::build(configureMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->focusNext();      // move to enum
  form->toggleFocused();  // toggle is a no-op for enums
  EXPECT_EQ(form->focusedField().text, "red");
}

/// @test
/// Cycling an integer field steps its value.
TEST_F(ArgFormEditors, CycleSpinsIntegerFields)
{
  // Right spins +1, left spins -1, clamped to the type's range.
  auto form = ArgForm::build(addMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->cycleFocused(true);
  EXPECT_EQ(form->focusedField().text, "1");
  form->cycleFocused(true);
  EXPECT_EQ(form->focusedField().text, "2");
  form->cycleFocused(false);
  EXPECT_EQ(form->focusedField().text, "1");
}

/// @test
/// The editor reported is the one for the focused field.
TEST_F(ArgFormEditors, FocusedEditorReflectsFocus)
{
  auto form = ArgForm::build(configureMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  EXPECT_EQ(form->focusedEditor(), EditorKind::boolean);
  form->focusNext();
  EXPECT_EQ(form->focusedEditor(), EditorKind::enumeration);
}

/// @test
/// A bool and an enumeration set with the editors submit as the values chosen.
TEST_F(ArgFormEditors, SubmitRoundTripBoolAndEnum)
{
  auto form = ArgForm::build(configureMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->toggleFocused();  // enabled -> true
  form->focusNext();
  form->cycleFocused(true);  // tint -> green

  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk());
  const auto& values = result.getValue();
  ASSERT_EQ(values.size(), 2U);
  EXPECT_EQ(values[0].get<bool>(), true);
  // Enum may be stored as the name (string) or as the integer key (u8) depending on how
  // adaptVariant resolved it. Either form is fine, we just want to see "green"-ness.
  if (auto* s = values[1].getIf<std::string>(); s != nullptr)
  {
    EXPECT_EQ(*s, "green");
  }
  else
  {
    EXPECT_EQ(values[1].getCopyAs<uint32_t>(), 1U);  // green's key in the Colour enum
  }
}

//--------------------------------------------------------------------------------------------------------------
// Numeric input filtering + integer range-aware spin
//--------------------------------------------------------------------------------------------------------------

/// @test
/// An integer field ignores keystrokes that are not digits.
TEST_F(ArgFormEditors, IntegerFieldRejectsNonDigitKeystrokes)
{
  auto form = ArgForm::build(addMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->insertText("4");
  form->insertText("a");  // ignored
  form->insertText("2");
  form->insertText("x");  // ignored
  EXPECT_EQ(form->focusedField().text, "42");
}

/// @test
/// A signed integer field accepts a leading minus.
TEST_F(ArgFormEditors, SignedIntegerAcceptsLeadingMinus)
{
  auto form = ArgForm::build(addMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->insertText("-");
  form->insertText("7");
  form->insertText("-");  // ignored, minus only valid at position 0
  EXPECT_EQ(form->focusedField().text, "-7");
}

/// @test
/// Spinning up stops at the type's maximum.
TEST_F(ArgFormEditors, IntegerSpinClampsAtTypeRange)
{
  // Spin clamps at INT32_MAX.
  auto form = ArgForm::build(addMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  form->clearField();
  form->insertText(std::to_string(std::numeric_limits<int32_t>::max() - 1));
  form->cycleFocused(true);  // +1 → INT32_MAX
  EXPECT_EQ(form->focusedField().text, std::to_string(std::numeric_limits<int32_t>::max()));
  form->cycleFocused(true);  // +1 again → still INT32_MAX (clamped)
  EXPECT_EQ(form->focusedField().text, std::to_string(std::numeric_limits<int32_t>::max()));
}

/// @test
/// Spinning down stops at the type's minimum.
TEST_F(ArgFormEditors, IntegerSpinClampsAtTypeRangeLow)
{
  auto form = ArgForm::build(addMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->clearField();
  form->insertText(std::to_string(std::numeric_limits<int32_t>::min() + 1));
  form->cycleFocused(false);  // -1 → INT32_MIN
  EXPECT_EQ(form->focusedField().text, std::to_string(std::numeric_limits<int32_t>::min()));
  form->cycleFocused(false);  // -1 again → still INT32_MIN (clamped)
  EXPECT_EQ(form->focusedField().text, std::to_string(std::numeric_limits<int32_t>::min()));
}

/// @test
/// An argument whose type is an alias of a quantity submits as that quantity.
TEST(ArgFormAlias, AliasedQuantitySubmits)
{
  // AliasType overrides none of the as*Type() accessors, so a field that stored the alias reported
  // no quantity type: assembleValue returned nullopt without setting failIdx and Return did nothing,
  // silently, forever.
  const auto& cls = *::term::test::TestObjectInterface::meta();
  const auto* method = cls.searchMethodByName("setAliasedLength");
  ASSERT_NE(method, nullptr);
  auto form = ArgForm::build(*method, "obj");
  ASSERT_TRUE(form.has_value());
  form->clearField();
  form->insertText("42");
  auto result = form->trySubmit();
  EXPECT_TRUE(result.isOk()) << "submit failed: " << result.getError().message;
}

/// @test
/// An alias is resolved before the form is built, so the form sees the underlying type.
TEST(ArgFormAlias, TheGeneratorResolvesAnAliasBeforeTheFormSeesIt)
{
  // Measured, not assumed: `setAliasedLength` takes AliasedMeters in the STL, and the form reports
  // the field's type as "Meters". So an alias never reaches buildField through a method argument,
  // which is why the two tests above pass with or without the alias-peeling in buildField. The
  // peeling is kept as a cheap guard for any path that does carry one, such as a nested struct field or
  // sequence element, which is not covered here. It is not what makes an aliased argument work.
  const auto& cls = *::term::test::TestObjectInterface::meta();
  const auto* method = cls.searchMethodByName("setAliasedLength");
  ASSERT_NE(method, nullptr);
  auto form = ArgForm::build(*method, "obj");
  ASSERT_TRUE(form.has_value());
  EXPECT_EQ(form->fields()[0].typeName, "Meters");
}

/// @test
/// An alias of an optional can be filled and emptied like the optional itself.
TEST(ArgFormAlias, AliasedOptionalToggles)
{
  const auto& cls = *::term::test::TestObjectInterface::meta();
  const auto* method = cls.searchMethodByName("setAliasedMaybe");
  ASSERT_NE(method, nullptr);
  auto form = ArgForm::build(*method, "obj");
  ASSERT_TRUE(form.has_value());
  const bool before = form->fields()[0].optionalIsEmpty;
  form->toggleFocusedOptional();
  EXPECT_NE(form->fields()[0].optionalIsEmpty, before) << "Ctrl+O did nothing on an aliased optional";
}

/// @test
/// Spinning at the limit saturates rather than wrapping round.
TEST_F(ArgFormEditors, IntegerSpinSaturatesInsteadOfOverflowing)
{
  // The text is parsed as int64 before being clamped to the field's type, so a pasted INT64_MAX
  // made `current + delta` signed overflow. It has to saturate on the way to the clamp.
  auto form = ArgForm::build(addMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  form->clearField();
  form->insertText(std::to_string(std::numeric_limits<int64_t>::max()));
  form->cycleFocused(true);
  EXPECT_EQ(form->focusedField().text, std::to_string(std::numeric_limits<int32_t>::max()));

  form->clearField();
  form->insertText(std::to_string(std::numeric_limits<int64_t>::min()));
  form->cycleFocused(false);
  EXPECT_EQ(form->focusedField().text, std::to_string(std::numeric_limits<int32_t>::min()));
}

/// @test
/// A value too large for a signed 64 bit integer is kept rather than truncated.
TEST_F(ArgFormEditors, IntegerSpinKeepsAValueTooLargeForInt64)
{
  // Anything above INT64_MAX threw out_of_range and was read as 0, so the first arrow press turned
  // a very large number into 1. It should saturate high and then clamp to the field's type.
  auto form = ArgForm::build(addMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  form->clearField();
  form->insertText("99999999999999999999999");
  form->cycleFocused(true);
  EXPECT_EQ(form->focusedField().text, std::to_string(std::numeric_limits<int32_t>::max()));
}

/// @test
/// A quoted enumerator typed on the command line fills the field as a bare name.
TEST_F(ArgFormEditors, PrefillStripsOuterQuotesForEnum)
{
  // When the user types `configure true "blue"` inline, the enum arg arrives as the string
  // "blue". The form should display it as a bare name so its cycler starts from there.
  std::vector<Var> prefill = {Var(true), Var(std::string("blue"))};
  auto form = ArgForm::build(configureMethod(), "obj", prefill);
  ASSERT_TRUE(form.has_value());
  EXPECT_EQ(form->fields()[0].text, "true");
  EXPECT_EQ(form->fields()[1].text, "blue");  // no surrounding quotes
}

//--------------------------------------------------------------------------------------------------------------
// Sequence editor, dynamic-shape composite
//--------------------------------------------------------------------------------------------------------------

class ArgFormSequence: public ::testing::Test
{
protected:
  [[nodiscard]] const Method& sumIntsMethod() const
  {
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("sumInts");
  }
  [[nodiscard]] const Method& centroidMethod() const
  {
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("centroid");
  }
};

/// @test
/// A method taking a sequence can be filled in from a form.
TEST_F(ArgFormSequence, BuildIsFormCompatible)
{
  EXPECT_TRUE(methodIsFormCompatible(sumIntsMethod()));
  EXPECT_TRUE(methodIsFormCompatible(centroidMethod()));
}

/// @test
/// A sequence argument starts with no elements.
TEST_F(ArgFormSequence, BuildEmptyByDefault)
{
  auto form = ArgForm::build(sumIntsMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  // Top-level field is the sequence group itself; no children yet.
  ASSERT_EQ(form->fields().size(), 1U);
  EXPECT_FALSE(isLeaf(form->fields()[0]));
  EXPECT_TRUE(form->fields()[0].children.empty());
  EXPECT_EQ(form->leafCount(), 0U);
}

/// @test
/// A sequence typed on the command line fills one element per value.
TEST_F(ArgFormSequence, PrefillFromVarListPopulatesElements)
{
  VarList prefill = {Var(int32_t {10}), Var(int32_t {20}), Var(int32_t {30})};
  std::vector<Var> argPrefill = {Var(std::move(prefill))};

  auto form = ArgForm::build(sumIntsMethod(), "obj", argPrefill);
  ASSERT_TRUE(form.has_value());
  ASSERT_EQ(form->fields()[0].children.size(), 3U);
  EXPECT_EQ(form->fields()[0].children[0].text, "10");
  EXPECT_EQ(form->fields()[0].children[1].text, "20");
  EXPECT_EQ(form->fields()[0].children[2].text, "30");
  EXPECT_EQ(form->leafCount(), 3U);
}

/// @test
/// Adding to an empty sequence creates its first element.
TEST_F(ArgFormSequence, AddElementToEmptySequenceTopLevel)
{
  auto form = ArgForm::build(sumIntsMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  ASSERT_EQ(form->leafCount(), 0U);

  form->addElementToFocusedSequence();
  EXPECT_EQ(form->leafCount(), 1U);
  EXPECT_EQ(form->fields()[0].children.size(), 1U);
  EXPECT_EQ(form->fields()[0].children[0].name, "[0]");
  EXPECT_EQ(form->fields()[0].children[0].text, "0");  // scalar default

  // Focus should be on the new element's first leaf.
  EXPECT_EQ(form->focusedField().name, "[0]");
}

/// @test
/// Several added elements submit as a sequence of those values.
TEST_F(ArgFormSequence, AddSeveralThenSubmit)
{
  auto form = ArgForm::build(sumIntsMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->addElementToFocusedSequence();
  form->clearField();
  form->insertText("5");
  form->addElementToFocusedSequence();
  form->clearField();
  form->insertText("7");

  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk());
  const auto& values = result.getValue();
  ASSERT_EQ(values.size(), 1U);
  const auto& list = values[0].get<VarList>();
  ASSERT_EQ(list.size(), 2U);
  EXPECT_EQ(list[0].get<int32_t>(), 5);
  EXPECT_EQ(list[1].get<int32_t>(), 7);
}

/// @test
/// Removing an element shortens the sequence and moves focus to a remaining one.
TEST_F(ArgFormSequence, RemoveElementShrinksAndRefocuses)
{
  auto form = ArgForm::build(sumIntsMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->addElementToFocusedSequence();
  form->addElementToFocusedSequence();
  form->addElementToFocusedSequence();
  ASSERT_EQ(form->leafCount(), 3U);

  // Focus on the middle element.
  form->focusPrev();  // currently on [2], step back to [1]
  EXPECT_EQ(form->focusedField().name, "[1]");

  form->removeFocusedSequenceElement();
  ASSERT_EQ(form->leafCount(), 2U);
  // Names re-synthesised so the rendered indices stay contiguous.
  EXPECT_EQ(form->fields()[0].children[0].name, "[0]");
  EXPECT_EQ(form->fields()[0].children[1].name, "[1]");
  // Focus lands on the element now at the removed index.
  EXPECT_EQ(form->focusedField().name, "[1]");
}

/// @test
/// Removing the last element moves focus to the one before it.
TEST_F(ArgFormSequence, RemoveLastElementRefocusesToPrevious)
{
  auto form = ArgForm::build(sumIntsMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  form->addElementToFocusedSequence();
  form->addElementToFocusedSequence();

  EXPECT_EQ(form->focusedField().name, "[1]");
  form->removeFocusedSequenceElement();
  EXPECT_EQ(form->leafCount(), 1U);
  EXPECT_EQ(form->focusedField().name, "[0]");
}

/// @test
/// A sequence of structs gives each element the struct's children.
TEST_F(ArgFormSequence, NestedCompositeSequenceBuildsStructChildren)
{
  // centroid takes a PointSeq = sequence<Point>. Each element expands into x/y leaves.
  auto form = ArgForm::build(centroidMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  EXPECT_EQ(form->leafCount(), 0U);

  form->addElementToFocusedSequence();
  // One element → two leaves (x, y).
  EXPECT_EQ(form->leafCount(), 2U);
  EXPECT_EQ(form->fields()[0].children[0].children.size(), 2U);
  EXPECT_EQ(form->fields()[0].children[0].children[0].name, "x");
  EXPECT_EQ(form->fields()[0].children[0].children[1].name, "y");
  EXPECT_EQ(form->focusedField().name, "x");  // first leaf of new element

  form->addElementToFocusedSequence();
  EXPECT_EQ(form->leafCount(), 4U);
  EXPECT_EQ(form->focusedField().name, "x");  // first leaf of second element
}

/// @test
/// A sequence of structs submits as a list of struct values.
TEST_F(ArgFormSequence, NestedCompositeSubmitAssemblesVarListOfMaps)
{
  auto form = ArgForm::build(centroidMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->addElementToFocusedSequence();
  form->clearField();
  form->insertText("1");
  form->focusNext();
  form->clearField();
  form->insertText("2");

  form->addElementToFocusedSequence();
  form->clearField();
  form->insertText("3");
  form->focusNext();
  form->clearField();
  form->insertText("4");

  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk());
  const auto& values = result.getValue();
  ASSERT_EQ(values.size(), 1U);
  const auto& list = values[0].get<VarList>();
  ASSERT_EQ(list.size(), 2U);
  EXPECT_EQ(list[0].get<VarMap>().at("x").get<int32_t>(), 1);
  EXPECT_EQ(list[0].get<VarMap>().at("y").get<int32_t>(), 2);
  EXPECT_EQ(list[1].get<VarMap>().at("x").get<int32_t>(), 3);
  EXPECT_EQ(list[1].get<VarMap>().at("y").get<int32_t>(), 4);
}

/// @test
/// A sequence formatted inline parses back to the same sequence.
TEST_F(ArgFormSequence, InlineFormattingRoundTrips)
{
  VarList original = {Var(int32_t {1}), Var(int32_t {2}), Var(int32_t {3})};
  Var value(std::move(original));

  const auto& arg = sumIntsMethod().getArgs()[0];
  auto inlineStr = formatInlineArg(value, arg.type);

  // Round-trip: reparse the inline string through a JSON doc.
  auto doc = std::string("{ \"v\": ") + inlineStr + " }";
  auto parsed = fromJson(doc).get<VarMap>();
  auto& roundTrip = parsed.at("v");

  ASSERT_TRUE(impl::adaptVariant(*arg.type, roundTrip).isOk());
  const auto& list = roundTrip.get<VarList>();
  ASSERT_EQ(list.size(), 3U);
  EXPECT_EQ(list[0].get<int32_t>(), 1);
  EXPECT_EQ(list[2].get<int32_t>(), 3);
}

//--------------------------------------------------------------------------------------------------------------
// Optional-of-composite editor, empty/filled toggle
//--------------------------------------------------------------------------------------------------------------

class ArgFormOptional: public ::testing::Test
{
protected:
  [[nodiscard]] const Method& anchorMethod() const
  {
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("anchor");
  }
};

/// @test
/// A method taking an optional can be filled in from a form.
TEST_F(ArgFormOptional, MethodIsFormCompatible) { EXPECT_TRUE(methodIsFormCompatible(anchorMethod())); }

/// @test
/// An optional argument starts empty.
TEST_F(ArgFormOptional, BuildStartsEmpty)
{
  auto form = ArgForm::build(anchorMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  ASSERT_EQ(form->fields().size(), 1U);

  const auto& optField = form->fields()[0];
  EXPECT_EQ(optField.kind, FieldKind::optionalGroup);
  EXPECT_TRUE(optField.optionalIsEmpty);
  EXPECT_TRUE(optField.children.empty());
  EXPECT_EQ(form->leafCount(), 0U);  // no leaves when empty
}

/// @test
/// Filling an empty optional builds the fields of the type it wraps.
TEST_F(ArgFormOptional, ToggleEmptyToFilledBuildsInnerSubtree)
{
  auto form = ArgForm::build(anchorMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->toggleFocusedOptional();
  const auto& optField = form->fields()[0];
  EXPECT_FALSE(optField.optionalIsEmpty);
  ASSERT_EQ(optField.children.size(), 1U);
  // Inner is a Point struct → two scalar leaves (x, y).
  EXPECT_EQ(form->leafCount(), 2U);
  EXPECT_EQ(form->focusedField().name, "x");  // focus on first leaf of new inner
}

/// @test
/// Emptying a filled optional drops those fields.
TEST_F(ArgFormOptional, ToggleFilledToEmptyDropsSubtree)
{
  auto form = ArgForm::build(anchorMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->toggleFocusedOptional();
  ASSERT_EQ(form->leafCount(), 2U);

  form->toggleFocusedOptional();
  EXPECT_TRUE(form->fields()[0].optionalIsEmpty);
  EXPECT_EQ(form->leafCount(), 0U);
}

/// @test
/// An optional left empty submits as no value.
TEST_F(ArgFormOptional, SubmitEmptyProducesMonostate)
{
  auto form = ArgForm::build(anchorMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk());
  const auto& values = result.getValue();
  ASSERT_EQ(values.size(), 1U);
  EXPECT_TRUE(values[0].holds<std::monostate>());
}

/// @test
/// A filled optional submits as the value it holds.
TEST_F(ArgFormOptional, SubmitFilledProducesInnerValue)
{
  auto form = ArgForm::build(anchorMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->toggleFocusedOptional();
  form->clearField();
  form->insertText("5");
  form->focusNext();
  form->clearField();
  form->insertText("7");

  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk());
  const auto& values = result.getValue();
  ASSERT_EQ(values.size(), 1U);
  // The inner is the Point struct (or adapted form of it).
  if (auto* m = values[0].getIf<VarMap>(); m != nullptr)
  {
    EXPECT_EQ(m->at("x").get<int32_t>(), 5);
    EXPECT_EQ(m->at("y").get<int32_t>(), 7);
  }
}

/// @test
/// An optional given a value on the command line starts filled.
TEST_F(ArgFormOptional, PrefillNonEmptyInitializesFilled)
{
  VarMap pointMap;
  pointMap.try_emplace("x", int32_t {2});
  pointMap.try_emplace("y", int32_t {3});
  std::vector<Var> prefill = {Var(std::move(pointMap))};

  auto form = ArgForm::build(anchorMethod(), "obj", prefill);
  ASSERT_TRUE(form.has_value());

  const auto& optField = form->fields()[0];
  EXPECT_FALSE(optField.optionalIsEmpty);
  ASSERT_EQ(optField.children.size(), 1U);
  EXPECT_EQ(form->leafCount(), 2U);
  EXPECT_EQ(optField.children[0].children[0].text, "2");
  EXPECT_EQ(optField.children[0].children[1].text, "3");
}

/// @test
/// An optional given no value on the command line stays empty.
TEST_F(ArgFormOptional, PrefillMonostateStaysEmpty)
{
  std::vector<Var> prefill = {Var {}};
  auto form = ArgForm::build(anchorMethod(), "obj", prefill);
  ASSERT_TRUE(form.has_value());
  EXPECT_TRUE(form->fields()[0].optionalIsEmpty);
}

/// @test
/// An empty optional formats as null.
TEST_F(ArgFormOptional, InlineFormatEmptyIsNull)
{
  const auto& arg = anchorMethod().getArgs()[0];
  Var empty;  // monostate
  EXPECT_EQ(formatInlineArg(empty, arg.type), "null");
}

/// @test
/// A filled optional formats as the value it holds.
TEST_F(ArgFormOptional, InlineFormatFilledIsInnerJson)
{
  const auto& arg = anchorMethod().getArgs()[0];
  VarMap m;
  m.try_emplace("x", int32_t {1});
  m.try_emplace("y", int32_t {2});
  Var filled(std::move(m));
  auto inlineStr = formatInlineArg(filled, arg.type);
  EXPECT_NE(inlineStr.find("\"x\": 1"), std::string::npos);
  EXPECT_NE(inlineStr.find("\"y\": 2"), std::string::npos);
}

// ----- optional-of-scalar also uses the optionalGroup, to give users a real "none" state -----

class ArgFormOptionalScalar: public ::testing::Test
{
protected:
  [[nodiscard]] const Method& limitMethod() const
  {
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("limit");
  }
};

/// @test
/// An optional of a scalar starts empty.
TEST_F(ArgFormOptionalScalar, BuildStartsEmpty)
{
  auto form = ArgForm::build(limitMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  ASSERT_EQ(form->fields().size(), 1U);
  const auto& optField = form->fields()[0];
  EXPECT_EQ(optField.kind, FieldKind::optionalGroup);
  EXPECT_TRUE(optField.optionalIsEmpty);
  EXPECT_EQ(form->leafCount(), 0U);
}

/// @test
/// Filling it creates the scalar leaf.
TEST_F(ArgFormOptionalScalar, ToggleFillsInnerScalarLeaf)
{
  auto form = ArgForm::build(limitMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->toggleFocusedOptional();
  const auto& optField = form->fields()[0];
  EXPECT_FALSE(optField.optionalIsEmpty);
  ASSERT_EQ(optField.children.size(), 1U);
  EXPECT_EQ(optField.children[0].kind, FieldKind::scalar);
  EXPECT_EQ(form->leafCount(), 1U);
  // MaybeInt's inner type is i32, which classifies as integerSpin so Left/Right spin by 1.
  EXPECT_EQ(form->focusedField().editor, EditorKind::integerSpin);
}

/// @test
/// Left empty it submits as no value.
TEST_F(ArgFormOptionalScalar, SubmitEmptyIsMonostate)
{
  auto form = ArgForm::build(limitMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk());
  EXPECT_TRUE(result.getValue()[0].holds<std::monostate>());
}

/// @test
/// Filled it submits as the scalar.
TEST_F(ArgFormOptionalScalar, SubmitFilledProducesScalar)
{
  auto form = ArgForm::build(limitMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->toggleFocusedOptional();
  form->clearField();
  form->insertText("42");

  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk());
  EXPECT_EQ(result.getValue()[0].get<int32_t>(), 42);
}

/// @test
/// A scalar given on the command line starts it filled.
TEST_F(ArgFormOptionalScalar, PrefillFromScalarInitializesFilled)
{
  std::vector<Var> prefill = {Var(int32_t {7})};
  auto form = ArgForm::build(limitMethod(), "obj", prefill);
  ASSERT_TRUE(form.has_value());
  EXPECT_FALSE(form->fields()[0].optionalIsEmpty);
  ASSERT_EQ(form->fields()[0].children.size(), 1U);
  EXPECT_EQ(form->fields()[0].children[0].text, "7");
}

//--------------------------------------------------------------------------------------------------------------
// Variant editor, one-of-N with dynamic subtree
//--------------------------------------------------------------------------------------------------------------

class ArgFormVariant: public ::testing::Test
{
protected:
  [[nodiscard]] const Method& describeMethod() const
  {
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("describe");
  }
};

/// @test
/// A method taking a variant can be filled in from a form.
TEST_F(ArgFormVariant, MethodIsFormCompatible) { EXPECT_TRUE(methodIsFormCompatible(describeMethod())); }

/// @test
/// A variant gives a selector for the alternative and the fields of the one selected.
TEST_F(ArgFormVariant, BuildProducesTypeSelectorAndValueSubtree)
{
  auto form = ArgForm::build(describeMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  ASSERT_EQ(form->fields().size(), 1U);

  const auto& variantField = form->fields()[0];
  EXPECT_EQ(variantField.kind, FieldKind::variantGroup);
  ASSERT_EQ(variantField.children.size(), 2U);

  // Child[0] is the type selector.
  EXPECT_EQ(variantField.children[0].name, "type");
  EXPECT_EQ(variantField.children[0].editor, EditorKind::variantType);

  // Default selection is the first alternative (Point in our Shape definition).
  EXPECT_EQ(variantField.selectedVariantIndex, 0U);
  EXPECT_EQ(variantField.children[0].text, "Point");

  // Child[1] is the value subtree, a struct group since Point is composite.
  EXPECT_EQ(variantField.children[1].name, "value");
  EXPECT_EQ(variantField.children[1].kind, FieldKind::structGroup);
}

/// @test
/// Choosing the next alternative replaces the fields with that alternative's.
TEST_F(ArgFormVariant, CycleForwardReshapesValueSubtree)
{
  auto form = ArgForm::build(describeMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  // Focused leaf is the type selector (first leaf of the variant group).
  EXPECT_EQ(form->focusedField().editor, EditorKind::variantType);
  EXPECT_EQ(form->focusedField().text, "Point");

  // Cycle to the next alternative (string).
  form->cycleFocused(true);
  EXPECT_EQ(form->focusedField().text, "string");
  EXPECT_EQ(form->fields()[0].selectedVariantIndex, 1U);
  // Value subtree is now a scalar string leaf, not a composite.
  EXPECT_EQ(form->fields()[0].children[1].kind, FieldKind::scalar);
  EXPECT_EQ(form->fields()[0].children[1].typeName, "string");
}

/// @test
/// Choosing the previous alternative wraps round at the first.
TEST_F(ArgFormVariant, CycleBackwardWraps)
{
  auto form = ArgForm::build(describeMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->cycleFocused(false);  // wraps from index 0 → last (i32)
  EXPECT_EQ(form->focusedField().text, "i32");
  EXPECT_EQ(form->fields()[0].selectedVariantIndex, 2U);
}

/// @test
/// Focus stays on the selector after changing the alternative.
TEST_F(ArgFormVariant, FocusStaysOnTypeSelectorAfterCycle)
{
  auto form = ArgForm::build(describeMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->cycleFocused(true);
  EXPECT_EQ(form->focusedField().editor, EditorKind::variantType);
}

/// @test
/// Focus moves from the selector into the chosen alternative's fields.
TEST_F(ArgFormVariant, NavigateFromTypeToValueLeaves)
{
  // After Point is selected, leaves are: type (selector), x, y.
  auto form = ArgForm::build(describeMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  ASSERT_EQ(form->leafCount(), 3U);
  EXPECT_EQ(form->focusedField().name, "type");

  form->focusNext();
  EXPECT_EQ(form->focusedField().name, "x");
  form->focusNext();
  EXPECT_EQ(form->focusedField().name, "y");
}

/// @test
/// Submitting builds the variant with its chosen alternative and value.
TEST_F(ArgFormVariant, SubmitBuildsVariantVarMap)
{
  auto form = ArgForm::build(describeMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  // Default is the Point variant; fill in x/y.
  form->focusNext();  // x
  form->clearField();
  form->insertText("3");
  form->focusNext();  // y
  form->clearField();
  form->insertText("4");

  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk());
  const auto& values = result.getValue();
  ASSERT_EQ(values.size(), 1U);
  // After adaptVariant, the variant Var should hold a KeyedVar at the matched type index.
  if (auto* kv = values[0].getIf<KeyedVar>(); kv != nullptr)
  {
    auto [idx, innerPtr] = *kv;
    EXPECT_EQ(idx, 0U);  // Point is index 0
    ASSERT_NE(innerPtr, nullptr);
    const auto& map = innerPtr->get<VarMap>();
    EXPECT_EQ(map.at("x").get<int32_t>(), 3);
    EXPECT_EQ(map.at("y").get<int32_t>(), 4);
  }
  else
  {
    // If adaptVariant didn't convert, the VarMap form is still acceptable.
    const auto& map = values[0].get<VarMap>();
    EXPECT_EQ(map.at("type").get<std::string>(), "Point");
  }
}

/// @test
/// Submitting after changing the alternative uses the alternative then selected.
TEST_F(ArgFormVariant, SubmitStringVariantAfterCycle)
{
  auto form = ArgForm::build(describeMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->cycleFocused(true);  // Point → string
  form->focusNext();         // focus the value leaf
  form->clearField();
  form->insertText("hello");

  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk());
  if (auto* kv = result.getValue()[0].getIf<KeyedVar>(); kv != nullptr)
  {
    auto [idx, innerPtr] = *kv;
    EXPECT_EQ(idx, 1U);  // string is index 1
    EXPECT_EQ(innerPtr->get<std::string>(), "hello");
  }
}

/// @test
/// A variant typed on the command line selects its alternative and fills its value.
TEST_F(ArgFormVariant, PrefillFromVarMapPopulatesSelection)
{
  // Simulate the user having typed: obj.describe {"type": "string", "value": "hi"}
  VarMap prefill;
  prefill.try_emplace("type", std::string("string"));
  prefill.try_emplace("value", std::string("hi"));

  std::vector<Var> argPrefill = {Var(std::move(prefill))};
  auto form = ArgForm::build(describeMethod(), "obj", argPrefill);
  ASSERT_TRUE(form.has_value());

  const auto& variantField = form->fields()[0];
  EXPECT_EQ(variantField.selectedVariantIndex, 1U);
  EXPECT_EQ(variantField.children[0].text, "string");
  EXPECT_EQ(variantField.children[1].kind, FieldKind::scalar);
  EXPECT_EQ(variantField.children[1].text, "hi");  // JSON quotes stripped for in-form display
}

/// @test
/// A variant formatted inline parses back to the same variant.
TEST_F(ArgFormVariant, InlineFormattingRoundTrips)
{
  // Build a Var that looks like what adaptVariant would produce: a KeyedVar with inner Var.
  auto innerPoint = std::make_shared<Var>(
    []()
    {
      VarMap m;
      m.try_emplace("x", int32_t {1});
      m.try_emplace("y", int32_t {2});
      return Var(std::move(m));
    }());
  KeyedVar kv(0, innerPoint);
  Var value(kv);

  const auto& arg = describeMethod().getArgs()[0];
  auto inlineStr = formatInlineArg(value, arg.type);

  // Expect "{\"type\": \"Point\", \"value\": {\"x\": 1, \"y\": 2}}".
  EXPECT_NE(inlineStr.find("\"type\": \"Point\""), std::string::npos);
  EXPECT_NE(inlineStr.find("\"x\": 1"), std::string::npos);

  // Round-trip: reparse through a JSON doc and adapt.
  auto doc = std::string("{ \"v\": ") + inlineStr + " }";
  auto parsed = fromJson(doc).get<VarMap>();
  auto& roundTrip = parsed.at("v");
  ASSERT_TRUE(impl::adaptVariant(*arg.type, roundTrip).isOk());
}

//--------------------------------------------------------------------------------------------------------------
// ArgFormQuantity, quantity types with units (quantityGroup) and without (scalar leaf)
//--------------------------------------------------------------------------------------------------------------

class ArgFormQuantity: public ::testing::Test
{
protected:
  [[nodiscard]] const Method& setLengthMethod() const
  {
    // setLength takes Meters = quantity<u16, m> [min: 0, max: 500]
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("setLength");
  }
  [[nodiscard]] const Method& setRatioMethod() const
  {
    // setRatio takes Ratio = quantity<f32, f32> [min: 0.0, max: 1.0] (unit-less)
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("setRatio");
  }
};

/// @test
/// A method taking a quantity with a unit can be filled in from a form.
TEST_F(ArgFormQuantity, WithUnitIsFormCompatible) { EXPECT_TRUE(methodIsFormCompatible(setLengthMethod())); }

/// @test
/// A method taking a quantity with no unit can be filled in from a form.
TEST_F(ArgFormQuantity, UnitLessIsFormCompatible) { EXPECT_TRUE(methodIsFormCompatible(setRatioMethod())); }

/// @test
/// A quantity with a unit gives a value field and a unit selector.
TEST_F(ArgFormQuantity, WithUnitBuildsQuantityGroupWithValueAndUnit)
{
  auto form = ArgForm::build(setLengthMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  ASSERT_EQ(form->fields().size(), 1U);
  const auto& q = form->fields()[0];
  EXPECT_EQ(q.kind, FieldKind::quantityGroup);
  ASSERT_EQ(q.children.size(), 2U);

  // Value leaf: integer-storage storage → integerSpin editor. Range bounds carried from the
  // quantity declaration.
  const auto& valueLeaf = q.children[0];
  EXPECT_EQ(valueLeaf.name, "value");
  EXPECT_EQ(valueLeaf.editor, EditorKind::integerSpin);
  EXPECT_TRUE(valueLeaf.quantityMinInt.has_value());
  EXPECT_TRUE(valueLeaf.quantityMaxInt.has_value());
  EXPECT_EQ(*valueLeaf.quantityMinInt, 0);
  EXPECT_EQ(*valueLeaf.quantityMaxInt, 500);

  // Unit leaf: defaults to the quantity's canonical unit (meters).
  const auto& unitLeaf = q.children[1];
  EXPECT_EQ(unitLeaf.name, "unit");
  EXPECT_EQ(unitLeaf.editor, EditorKind::unitSelector);
  EXPECT_EQ(unitLeaf.text, "m");
}

/// @test
/// A quantity with no unit gives a plain scalar field.
TEST_F(ArgFormQuantity, UnitLessBuildsScalarLeaf)
{
  auto form = ArgForm::build(setRatioMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  ASSERT_EQ(form->fields().size(), 1U);
  const auto& q = form->fields()[0];
  // Unit-less quantity has no unit picker, so the form treats it as a plain scalar with the
  // storage type's editor, f32 storage → text editor.
  EXPECT_EQ(q.kind, FieldKind::scalar);
  EXPECT_EQ(q.editor, EditorKind::text);
  EXPECT_EQ(q.children.size(), 0U);
}

/// @test
/// Spinning up stops at the quantity's maximum.
TEST_F(ArgFormQuantity, IntegerSpinClampsToQuantityMax)
{
  auto form = ArgForm::build(setLengthMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  // Focus starts on the value leaf. Jump to the high end and try to spin past 500.
  form->clearField();
  form->insertText("500");
  form->cycleFocused(true);  // should not exceed the quantity max of 500
  EXPECT_EQ(form->focusedField().text, "500");

  form->cycleFocused(false);  // down to 499
  EXPECT_EQ(form->focusedField().text, "499");
}

/// @test
/// Spinning down stops at the quantity's minimum.
TEST_F(ArgFormQuantity, IntegerSpinClampsToQuantityMin)
{
  auto form = ArgForm::build(setLengthMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->clearField();
  form->insertText("0");
  form->cycleFocused(false);  // should not go below 0 even though u16 allows wraparound
  EXPECT_EQ(form->focusedField().text, "0");
}

/// @test
/// The unit selector steps through the units available.
TEST_F(ArgFormQuantity, CycleUnitSelectorSteps)
{
  auto form = ArgForm::build(setLengthMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  // Navigate to the unit leaf (it's the second leaf).
  form->focusNext();
  ASSERT_EQ(form->focusedField().editor, EditorKind::unitSelector);
  const auto startUnit = form->focusedField().text;

  form->cycleFocused(true);
  EXPECT_NE(form->focusedField().text, startUnit);  // moved to a different unit
}

/// @test
/// Submitting converts the value to the quantity's canonical unit.
TEST_F(ArgFormQuantity, SubmitConvertsToCanonicalUnit)
{
  auto form = ArgForm::build(setLengthMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  // Enter 500 in the value field (this is already within the [0, 500] range expressed in meters).
  form->clearField();
  form->insertText("500");

  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk());
  const auto& values = result.getValue();
  ASSERT_EQ(values.size(), 1U);
  // Canonical unit is meters, so the submitted value is 500 m.
  EXPECT_DOUBLE_EQ(values[0].getCopyAs<float64_t>(), 500.0);
}

/// @test
/// A value entered in another unit is converted on submit.
TEST_F(ArgFormQuantity, SubmitAppliesUnitConversionWhenUnitDiffersFromCanonical)
{
  // If the user picks a different unit from the quantity's canonical one, assembleValue must
  // convert the entered value to the canonical unit before handing it off. We use centimeters
  // here, an unambiguous factor-of-100 conversion from meters.
  auto form = ArgForm::build(setLengthMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  // Value leaf: enter 100.
  form->clearField();
  form->insertText("100");

  // Navigate to the unit leaf and cycle until we land on "cm". The cycle ordering depends on the
  // registry, so we look for "cm" explicitly and cycle up to a bounded number of times.
  form->focusNext();
  ASSERT_EQ(form->focusedField().editor, EditorKind::unitSelector);
  for (int i = 0; i < 20 && form->focusedField().text != "cm"; ++i)
  {
    form->cycleFocused(true);
  }
  ASSERT_EQ(form->focusedField().text, "cm") << "could not cycle to centimetres in the registry";

  // 100 cm = 1 m, well within the [0, 500] range in meters.
  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk()) << "submit failed: " << result.getError().message;
  EXPECT_DOUBLE_EQ(result.getValue()[0].getCopyAs<float64_t>(), 1.0);
}

/// @test
/// A quantity with no unit submits as a plain number.
TEST_F(ArgFormQuantity, UnitLessSubmitIsNumeric)
{
  auto form = ArgForm::build(setRatioMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  form->clearField();
  form->insertText("0.5");

  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk());
  EXPECT_DOUBLE_EQ(result.getValue()[0].getCopyAs<float64_t>(), 0.5);
}

//--------------------------------------------------------------------------------------------------------------
// Duration / TimeStamp, QuantityType subclasses with bespoke dispatch
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A duration is a scalar field rather than a value and unit pair.
TEST(ArgFormDuration, DurationFieldIsScalarNotQuantityGroup)
{
  // Duration fields use a scalar text editor (not the generic quantity group).
  const auto& cls = *::term::test::TestObjectInterface::meta();
  const auto* waitMethod = cls.searchMethodByName("wait");
  ASSERT_NE(waitMethod, nullptr);

  auto form = ArgForm::build(*waitMethod, "obj");
  ASSERT_TRUE(form.has_value());
  ASSERT_EQ(form->fields().size(), 1U);
  const auto& f = form->fields()[0];
  EXPECT_EQ(f.kind, FieldKind::scalar);
  EXPECT_EQ(f.editor, EditorKind::text);  // float grammar, critical for "1.5 s" input
  EXPECT_EQ(f.children.size(), 0U);
}

/// @test
/// A duration field accepts a unit written with the number.
TEST(ArgFormDuration, DurationAcceptsUnitLettersInInput)
{
  // Duration input must accept unit suffix characters (s, m, n, h, ...).
  const auto& cls = *::term::test::TestObjectInterface::meta();
  const auto* waitMethod = cls.searchMethodByName("wait");
  ASSERT_NE(waitMethod, nullptr);

  auto form = ArgForm::build(*waitMethod, "obj");
  ASSERT_TRUE(form.has_value());
  form->clearField();
  form->insertText("500 ms");
  EXPECT_EQ(form->focusedField().text, "500 ms");

  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk());
  auto values = result.getValue();
  ASSERT_TRUE(impl::adaptVariant(*DurationType::get(), values[0]).isOk());
  // 500 ms through Unit::fromString rounds to ~0.5 s with float rounding in the conversion.
  EXPECT_NEAR(values[0].getCopyAs<Duration>().toSeconds(), 0.5, 1e-6);
}

/// @test
/// A duration typed in milliseconds survives the round trip through the command engine.
TEST(ArgFormDuration, TypedMillisecondsRoundTripThroughCommandEngine)
{
  // "1000 ms" typed in the Duration field must survive the full round-trip:
  // form → formatInlineInvocation → parseArgs → adaptVariant → Duration.
  const auto& cls = *::term::test::TestObjectInterface::meta();
  const auto* waitMethod = cls.searchMethodByName("wait");
  ASSERT_NE(waitMethod, nullptr);

  auto form = ArgForm::build(*waitMethod, "obj");
  ASSERT_TRUE(form.has_value());
  form->clearField();
  form->insertText("1000 ms");

  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk()) << "submit failed: " << result.getError().message;

  auto inlined =
    formatInlineInvocation(form->objectName(), form->methodName(), form->method().getArgs(), result.getValue());
  // The inline form must wrap the unit-bearing value in JSON quotes so splitTopLevelArgs /
  // fromJson can tokenize it without tripping on the letters. formatInlineArg emits every
  // Duration in seconds (arg_form.cpp, the isDurationType branch), so 1000 ms comes back as
  // "1 s": assert the quoting and the suffix, and let the parse below assert the value.
  auto openQuote = inlined.find('"');
  ASSERT_NE(openQuote, std::string::npos) << "inline invocation did not quote the value, got: " << inlined;
  auto closeQuote = inlined.find('"', openQuote + 1U);
  ASSERT_NE(closeQuote, std::string::npos) << "unterminated quote in: " << inlined;
  const std::string arg = inlined.substr(openQuote + 1U, closeQuote - openQuote - 1U);
  auto space = arg.find(' ');
  ASSERT_NE(space, std::string::npos) << R"(expected "<number> <unit>", got: )" << arg;
  EXPECT_EQ(arg.find_first_of("0123456789", space + 1U), std::string::npos)
    << "expected a unit suffix after the number, got: " << arg;

  // Now simulate the command engine: tokenize, wrap in a JSON doc, parse, adapt to Duration.
  auto argsStart = inlined.find(' ');
  ASSERT_NE(argsStart, std::string::npos);
  std::string argsStr = inlined.substr(argsStart + 1);
  auto doc = std::string(R"({ "args": [)") + argsStr + "] }";
  auto parsed = fromJson(doc).get<VarMap>();
  auto& list = parsed.at("args").get<VarList>();
  ASSERT_EQ(list.size(), 1U);
  ASSERT_TRUE(impl::adaptVariant(*DurationType::get(), list[0]).isOk());
  EXPECT_NEAR(list[0].getCopyAs<Duration>().toSeconds(), 1.0, 1e-6);
}

/// @test
/// A duration formatted into an invocation parses back to the same duration.
TEST(ArgFormDuration, InlineInvocationRoundTripsTypedValue)
{
  // Bare number "100" in the Duration field round-trips through inline invocation.
  const auto& cls = *::term::test::TestObjectInterface::meta();
  const auto* waitMethod = cls.searchMethodByName("wait");
  ASSERT_NE(waitMethod, nullptr);

  auto form = ArgForm::build(*waitMethod, "obj");
  ASSERT_TRUE(form.has_value());
  form->clearField();
  form->insertText("100");

  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk());
  auto inlined =
    formatInlineInvocation(form->objectName(), form->methodName(), form->method().getArgs(), result.getValue());
  // parseFieldText appends " s" when there's no explicit unit suffix, so 100 → "100 s".
  EXPECT_NE(inlined.find("\"100 s\""), std::string::npos)
    << "inline invocation should carry the typed value, got: " << inlined;
  EXPECT_EQ(inlined.find("\"0 s\""), std::string::npos) << "old fallback '0 s' shouldn't appear, got: " << inlined;
}

/// @test
/// A duration submits as a string carrying its unit, so the unit is not lost.
TEST(ArgFormDuration, DurationSubmitUsesStringWithUnitSuffix)
{
  // Type "1.5" in the duration field and submit, the form should emit a value that adaptVariant
  // on DurationType accepts and that dispatches to 1.5 seconds (not 1.5 ns).
  const auto& cls = *::term::test::TestObjectInterface::meta();
  const auto* waitMethod = cls.searchMethodByName("wait");
  ASSERT_NE(waitMethod, nullptr);

  auto form = ArgForm::build(*waitMethod, "obj");
  ASSERT_TRUE(form.has_value());
  form->clearField();
  form->insertText("1.5");

  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk()) << "submit failed: " << result.getError().message;
  auto values = result.getValue();  // take a mutable copy for adaptVariant
  ASSERT_TRUE(impl::adaptVariant(*DurationType::get(), values[0]).isOk());
  EXPECT_DOUBLE_EQ(values[0].getCopyAs<Duration>().toSeconds(), 1.5);
}

//--------------------------------------------------------------------------------------------------------------
// Context predicates, `focusedIsInsideSequence` / `focusedIsInsideOptional` drive the
// contextual key-binding hint in the status bar.
//--------------------------------------------------------------------------------------------------------------

class ArgFormContext: public ::testing::Test
{
protected:
  [[nodiscard]] const Method& methodByName(std::string_view name) const
  {
    return *::term::test::TestObjectInterface::meta()->searchMethodByName(name);
  }
};

/// @test
/// A plain scalar field reports that it is in neither a sequence nor an optional.
TEST_F(ArgFormContext, ScalarFieldIsNotInsideSequenceOrOptional)
{
  auto form = ArgForm::build(methodByName("add"), "obj");
  ASSERT_TRUE(form.has_value());
  EXPECT_FALSE(form->focusedIsInsideSequence());
  EXPECT_FALSE(form->focusedIsInsideOptional());
}

/// @test
/// An empty sequence argument reports that it is inside a sequence.
TEST_F(ArgFormContext, EmptySequenceArgReportsInsideSequence)
{
  // sumInts takes an IntSeq that starts empty, Ctrl+N should be advertised even though the
  // form has no leaves yet.
  auto form = ArgForm::build(methodByName("sumInts"), "obj");
  ASSERT_TRUE(form.has_value());
  EXPECT_EQ(form->leafCount(), 0U);
  EXPECT_TRUE(form->focusedIsInsideSequence());
  EXPECT_FALSE(form->focusedIsInsideOptional());
}

/// @test
/// An element of a sequence reports that it is inside a sequence.
TEST_F(ArgFormContext, ElementInsideSequenceReportsInsideSequence)
{
  auto form = ArgForm::build(methodByName("sumInts"), "obj");
  ASSERT_TRUE(form.has_value());
  form->addElementToFocusedSequence();
  ASSERT_GT(form->leafCount(), 0U);
  EXPECT_TRUE(form->focusedIsInsideSequence());
}

/// @test
/// An empty optional reports that it is inside an optional.
TEST_F(ArgFormContext, EmptyOptionalReportsInsideOptional)
{
  // `anchor(p: MaybePoint)`, MaybePoint starts empty (no inner leaves) but Ctrl+O should toggle
  // it, so the hint predicate must flag this even when leafCount() == 0.
  auto form = ArgForm::build(methodByName("anchor"), "obj");
  ASSERT_TRUE(form.has_value());
  EXPECT_EQ(form->leafCount(), 0U);
  EXPECT_TRUE(form->focusedIsInsideOptional());
  EXPECT_FALSE(form->focusedIsInsideSequence());
}

/// @test
/// A bounded sequence at its limit refuses another element.
TEST_F(ArgFormContext, BoundedSequenceAtCapacityDisallowsAdd)
{
  // sumBounded takes a sequence<i32, 4>. Add four elements; the fifth add must be rejected and
  // the hint predicates should agree.
  auto form = ArgForm::build(methodByName("sumBounded"), "obj");
  ASSERT_TRUE(form.has_value());
  for (int i = 0; i < 4; ++i)
  {
    form->addElementToFocusedSequence();
  }
  EXPECT_EQ(form->leafCount(), 4U);
  EXPECT_FALSE(form->focusedCanAddElement());
  EXPECT_TRUE(form->focusedCanRemoveElement());

  // Calling add again should be a no-op (already at capacity).
  form->addElementToFocusedSequence();
  EXPECT_EQ(form->leafCount(), 4U);
}

/// @test
/// A fixed size array starts with all its elements and refuses both adding and removing.
TEST_F(ArgFormContext, FixedSizeArrayStartsFullAndRejectsAddRemove)
{
  // sumTriple takes array<i32, 3>. The form must open with exactly 3 elements pre-filled, and
  // both add and remove must be disallowed, the type's cardinality is pinned.
  auto form = ArgForm::build(methodByName("sumTriple"), "obj");
  ASSERT_TRUE(form.has_value());
  ASSERT_EQ(form->fields().size(), 1U);
  const auto& arg = form->fields()[0];
  EXPECT_EQ(arg.kind, FieldKind::sequenceGroup);
  EXPECT_EQ(arg.children.size(), 3U);

  EXPECT_FALSE(form->focusedCanAddElement());
  EXPECT_FALSE(form->focusedCanRemoveElement());

  // Attempted mutations must be no-ops.
  form->addElementToFocusedSequence();
  EXPECT_EQ(form->fields()[0].children.size(), 3U);
  form->removeFocusedSequenceElement();
  EXPECT_EQ(form->fields()[0].children.size(), 3U);
}

/// @test
/// An unbounded sequence allows adding and removing.
TEST_F(ArgFormContext, UnboundedSequenceAllowsAddAndRemove)
{
  auto form = ArgForm::build(methodByName("sumInts"), "obj");
  ASSERT_TRUE(form.has_value());
  EXPECT_TRUE(form->focusedCanAddElement());
  // With zero elements there's nothing to remove yet, predicate honestly reports that.
  EXPECT_FALSE(form->focusedCanRemoveElement());
  form->addElementToFocusedSequence();
  EXPECT_TRUE(form->focusedCanAddElement());
  EXPECT_TRUE(form->focusedCanRemoveElement());
}

/// @test
/// A leaf inside a filled optional reports that it is inside an optional.
TEST_F(ArgFormContext, LeafInsideFilledOptionalReportsInsideOptional)
{
  auto form = ArgForm::build(methodByName("anchor"), "obj");
  ASSERT_TRUE(form.has_value());
  form->toggleFocusedOptional();  // empty → filled
  ASSERT_GT(form->leafCount(), 0U);
  EXPECT_TRUE(form->focusedIsInsideOptional());
}

/// @test
/// A struct formatted inline parses back to the same struct.
TEST_F(ArgFormStruct, InlineArgRoundTripsThroughParser)
{
  // Build a value, render to inline, re-parse via the command-engine parser, should recover the
  // same struct. This is what makes the form's "echo equivalent inline invocation" work.
  VarMap original;
  original.try_emplace("x", int32_t {42});
  original.try_emplace("y", int32_t {-7});
  Var value(std::move(original));

  const auto& p = movePointMethod().getArgs()[0];
  auto inlineStr = formatInlineArg(value, p.type);

  // JSON-wrap and parse, mirroring how command_engine::parseArgs composes the full JSON array.
  auto doc = std::string("{ \"v\": ") + inlineStr + " }";
  auto parsed = fromJson(doc).get<VarMap>();
  auto& roundTrip = parsed.at("v");

  ASSERT_TRUE(impl::adaptVariant(*p.type, roundTrip).isOk());
  const auto& map = roundTrip.get<VarMap>();
  EXPECT_EQ(map.at("x").get<int32_t>(), 42);
  EXPECT_EQ(map.at("y").get<int32_t>(), -7);
}

}  // namespace
}  // namespace sen::components::term
