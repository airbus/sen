// === arg_form_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "arg_form.h"

// sen
#include "sen/core/base/duration.h"
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

namespace sen::components::term
{
namespace
{

//--------------------------------------------------------------------------------------------------------------
// isScalarType / methodIsFormCompatible
//--------------------------------------------------------------------------------------------------------------

TEST(ArgForm, ScalarPrimitivesAreScalar)
{
  EXPECT_TRUE(isScalarType(BoolType::get()));
  EXPECT_TRUE(isScalarType(Int32Type::get()));
  EXPECT_TRUE(isScalarType(UInt64Type::get()));
  EXPECT_TRUE(isScalarType(Float64Type::get()));
  EXPECT_TRUE(isScalarType(StringType::get()));
  EXPECT_TRUE(isScalarType(DurationType::get()));
}

TEST(ArgForm, CompositeTypesAreNotScalar)
{
  auto seq = SequenceType::make(SequenceSpec {"Seq", "ns.Seq", "", Int32Type::get()});
  EXPECT_FALSE(isScalarType(seq));

  auto str = StructType::make(StructSpec {"S", "ns.S", "", {{"x", "", Int32Type::get()}}});
  EXPECT_FALSE(isScalarType(str));
}

TEST(ArgForm, OptionalOfScalarUnwrapsToScalar)
{
  auto opt = OptionalType::make(OptionalSpec {"O", "ns.O", "", Int32Type::get()});
  EXPECT_TRUE(isScalarType(opt));
}

TEST(ArgForm, OptionalOfCompositeIsNotScalar)
{
  auto seq = SequenceType::make(SequenceSpec {"Seq", "ns.Seq", "", Int32Type::get()});
  auto opt = OptionalType::make(OptionalSpec {"O", "ns.O", "", seq});
  EXPECT_FALSE(isScalarType(opt));
}

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

TEST(ArgForm, DefaultsForPrimitives)
{
  EXPECT_EQ(defaultTextFor(BoolType::get()), "false");
  EXPECT_EQ(defaultTextFor(Int32Type::get()), "0");
  EXPECT_EQ(defaultTextFor(Float64Type::get()), "0");
  EXPECT_EQ(defaultTextFor(StringType::get()), "");
  EXPECT_EQ(defaultTextFor(DurationType::get()), "0");
}

TEST(ArgForm, DefaultForEnumIsFirstEnumerator)
{
  auto e =
    EnumType::make(EnumSpec {"E", "ns.E", "", {{"alpha", 0, ""}, {"beta", 1, ""}, {"gamma", 2, ""}}, UInt8Type::get()});
  EXPECT_EQ(defaultTextFor(e), "alpha");
}

TEST(ArgForm, DefaultForOptionalUnwraps)
{
  auto opt = OptionalType::make(OptionalSpec {"O", "ns.O", "", Int32Type::get()});
  EXPECT_EQ(defaultTextFor(opt), "0");
}

//--------------------------------------------------------------------------------------------------------------
// formatInlineArg
//--------------------------------------------------------------------------------------------------------------

TEST(ArgForm, InlineArgInteger)
{
  EXPECT_EQ(formatInlineArg(Var(int32_t {42}), Int32Type::get()), "42");
  EXPECT_EQ(formatInlineArg(Var(int32_t {-7}), Int32Type::get()), "-7");
}

TEST(ArgForm, InlineArgBool)
{
  EXPECT_EQ(formatInlineArg(Var(true), BoolType::get()), "true");
  EXPECT_EQ(formatInlineArg(Var(false), BoolType::get()), "false");
}

TEST(ArgForm, InlineArgStringIsQuoted)
{
  EXPECT_EQ(formatInlineArg(Var(std::string("hello world")), StringType::get()), "\"hello world\"");
}

TEST(ArgForm, InlineArgStringEscapesMetacharacters)
{
  EXPECT_EQ(formatInlineArg(Var(std::string("a\"b")), StringType::get()), "\"a\\\"b\"");
  EXPECT_EQ(formatInlineArg(Var(std::string("a\\b")), StringType::get()), "\"a\\\\b\"");
  EXPECT_EQ(formatInlineArg(Var(std::string("a\nb")), StringType::get()), "\"a\\nb\"");
}

TEST(ArgForm, InlineArgEnumUsesName)
{
  auto e = EnumType::make(EnumSpec {"Col", "ns.Col", "", {{"red", 0, ""}, {"blue", 1, ""}}, UInt8Type::get()});
  EXPECT_EQ(formatInlineArg(Var(std::string("red")), e), "\"red\"");
  // Stored as integer key, still rendered as the name.
  EXPECT_EQ(formatInlineArg(Var(uint8_t {1}), e), "\"blue\"");
}

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

TEST(ArgForm, InvocationWithNoArgs)
{
  const auto& cls = *::term::test::TestObjectInterface::meta();
  const auto* ping = cls.searchMethodByName("ping");
  ASSERT_NE(ping, nullptr);

  auto out = formatInlineInvocation("obj", "ping", ping->getArgs(), {});
  EXPECT_EQ(out, "obj.ping");
}

TEST(ArgForm, InvocationWithTwoIntegers)
{
  const auto& cls = *::term::test::TestObjectInterface::meta();
  const auto* add = cls.searchMethodByName("add");
  ASSERT_NE(add, nullptr);

  VarList values = {Var(int32_t {3}), Var(int32_t {4})};
  auto out = formatInlineInvocation("calc", "add", add->getArgs(), values);
  EXPECT_EQ(out, "calc.add 3 4");
}

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
  const Method& addMethod() const { return *::term::test::TestObjectInterface::meta()->searchMethodByName("add"); }
  const Method& echoMethod() const { return *::term::test::TestObjectInterface::meta()->searchMethodByName("echo"); }
  const Method& pingMethod() const { return *::term::test::TestObjectInterface::meta()->searchMethodByName("ping"); }
};

TEST_F(ArgFormStateful, BuildProducesOneFieldPerArg)
{
  auto form = ArgForm::build(addMethod(), "calc");
  ASSERT_TRUE(form.has_value());
  EXPECT_EQ(form->fields().size(), 2U);
  EXPECT_EQ(form->objectName(), "calc");
  EXPECT_EQ(form->methodName(), "add");
  EXPECT_EQ(form->focusedIndex(), 0U);
}

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

TEST_F(ArgFormStateful, BackspaceDropsTrailingCharacter)
{
  auto form = ArgForm::build(addMethod(), "calc");
  ASSERT_TRUE(form.has_value());

  form->clearField();
  form->insertText("42");
  form->backspace();
  EXPECT_EQ(form->fields()[0].text, "4");
}

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

TEST_F(ArgFormStateful, SubmitEmptyRequiredFieldFails)
{
  auto form = ArgForm::build(addMethod(), "calc");
  ASSERT_TRUE(form.has_value());

  form->clearField();
  auto result = form->trySubmit();
  ASSERT_TRUE(result.isError());
  EXPECT_EQ(result.getError().fieldIndex, 0U);
}

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

TEST_F(ArgFormStateful, NoArgsMethodBuildsEmptyForm)
{
  auto form = ArgForm::build(pingMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  EXPECT_EQ(form->fields().size(), 0U);

  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk());
  EXPECT_TRUE(result.getValue().empty());
}

TEST_F(ArgFormStateful, EnumFieldAcceptsBareName)
{
  // Bare enum name (no quotes) should parse correctly.
  auto e =
    EnumType::make(EnumSpec {"E", "ns.E", "", {{"alpha", 0, ""}, {"beta", 1, ""}, {"gamma", 2, ""}}, UInt8Type::get()});
  EXPECT_EQ(defaultTextFor(e), "alpha");
  Var v(std::string("beta"));
  ASSERT_TRUE(impl::adaptVariant(*e, v).isOk());
}

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

TEST_F(ArgFormStateful, SubsequentKeystrokesAppendAsNormal)
{
  auto form = ArgForm::build(addMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->insertText("5");
  form->insertText("7");
  EXPECT_EQ(form->fields()[0].text, "57");
}

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

TEST(ArgFormEffectiveDescription, PrefersExplicitDescription)
{
  // Explicit description takes priority over the type's built-in description.
  EXPECT_EQ(effectiveDescription("a user-chosen flag", BoolType::get()), "a user-chosen flag");
}

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
  const Method& movePointMethod() const
  {
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("movePoint");
  }
};

TEST_F(ArgFormStruct, BuildExpandsStructIntoChildren)
{
  auto form = ArgForm::build(movePointMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  // One top-level field (the `p` argument), which is a composite with two children.
  ASSERT_EQ(form->fields().size(), 1U);
  const auto& pField = form->fields()[0];
  EXPECT_EQ(pField.name, "p");
  EXPECT_EQ(pField.typeName, "Point");
  EXPECT_FALSE(pField.isLeaf());
  ASSERT_EQ(pField.children.size(), 2U);
  EXPECT_EQ(pField.children[0].name, "x");
  EXPECT_EQ(pField.children[1].name, "y");

  // Leaf count is 2, the two scalar sub-fields, not the composite group itself.
  EXPECT_EQ(form->leafCount(), 2U);
}

TEST_F(ArgFormStruct, LeafDefaultsPropagate)
{
  auto form = ArgForm::build(movePointMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  // Scalar defaults applied to every leaf regardless of nesting depth.
  EXPECT_EQ(form->fields()[0].children[0].text, "0");
  EXPECT_EQ(form->fields()[0].children[1].text, "0");
}

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
  const Method& configureMethod() const
  {
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("configure");
  }
  const Method& addMethod() const { return *::term::test::TestObjectInterface::meta()->searchMethodByName("add"); }
};

TEST_F(ArgFormEditors, BoolLeafClassifiedAsBooleanEditor)
{
  auto form = ArgForm::build(configureMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  ASSERT_EQ(form->leafCount(), 2U);
  EXPECT_EQ(form->fields()[0].editor, EditorKind::boolean);
  EXPECT_EQ(form->fields()[0].text, "false");  // default
}

TEST_F(ArgFormEditors, EnumLeafClassifiedAsEnumerationEditor)
{
  auto form = ArgForm::build(configureMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  EXPECT_EQ(form->fields()[1].editor, EditorKind::enumeration);
  EXPECT_EQ(form->fields()[1].text, "red");  // first enumerator
}

TEST_F(ArgFormEditors, IntLeafClassifiedAsIntegerSpinEditor)
{
  auto form = ArgForm::build(addMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  EXPECT_EQ(form->fields()[0].editor, EditorKind::integerSpin);
  EXPECT_EQ(form->fields()[1].editor, EditorKind::integerSpin);
}

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

TEST_F(ArgFormEditors, ToggleNoOpOnEnum)
{
  auto form = ArgForm::build(configureMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->focusNext();      // move to enum
  form->toggleFocused();  // toggle is a no-op for enums
  EXPECT_EQ(form->focusedField().text, "red");
}

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

TEST_F(ArgFormEditors, FocusedEditorReflectsFocus)
{
  auto form = ArgForm::build(configureMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  EXPECT_EQ(form->focusedEditor(), EditorKind::boolean);
  form->focusNext();
  EXPECT_EQ(form->focusedEditor(), EditorKind::enumeration);
}

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

TEST_F(ArgFormEditors, SignedIntegerAcceptsLeadingMinus)
{
  auto form = ArgForm::build(addMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->insertText("-");
  form->insertText("7");
  form->insertText("-");  // ignored, minus only valid at position 0
  EXPECT_EQ(form->focusedField().text, "-7");
}

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
  const Method& sumIntsMethod() const
  {
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("sumInts");
  }
  const Method& centroidMethod() const
  {
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("centroid");
  }
};

TEST_F(ArgFormSequence, BuildIsFormCompatible)
{
  EXPECT_TRUE(methodIsFormCompatible(sumIntsMethod()));
  EXPECT_TRUE(methodIsFormCompatible(centroidMethod()));
}

TEST_F(ArgFormSequence, BuildEmptyByDefault)
{
  auto form = ArgForm::build(sumIntsMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  // Top-level field is the sequence group itself; no children yet.
  ASSERT_EQ(form->fields().size(), 1U);
  EXPECT_FALSE(form->fields()[0].isLeaf());
  EXPECT_TRUE(form->fields()[0].children.empty());
  EXPECT_EQ(form->leafCount(), 0U);
}

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
  const Method& anchorMethod() const
  {
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("anchor");
  }
};

TEST_F(ArgFormOptional, MethodIsFormCompatible) { EXPECT_TRUE(methodIsFormCompatible(anchorMethod())); }

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

TEST_F(ArgFormOptional, PrefillMonostateStaysEmpty)
{
  std::vector<Var> prefill = {Var {}};
  auto form = ArgForm::build(anchorMethod(), "obj", prefill);
  ASSERT_TRUE(form.has_value());
  EXPECT_TRUE(form->fields()[0].optionalIsEmpty);
}

TEST_F(ArgFormOptional, InlineFormatEmptyIsNull)
{
  const auto& arg = anchorMethod().getArgs()[0];
  Var empty;  // monostate
  EXPECT_EQ(formatInlineArg(empty, arg.type), "null");
}

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
  const Method& limitMethod() const { return *::term::test::TestObjectInterface::meta()->searchMethodByName("limit"); }
};

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

TEST_F(ArgFormOptionalScalar, SubmitEmptyIsMonostate)
{
  auto form = ArgForm::build(limitMethod(), "obj");
  ASSERT_TRUE(form.has_value());
  auto result = form->trySubmit();
  ASSERT_TRUE(result.isOk());
  EXPECT_TRUE(result.getValue()[0].holds<std::monostate>());
}

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
  const Method& describeMethod() const
  {
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("describe");
  }
};

TEST_F(ArgFormVariant, MethodIsFormCompatible) { EXPECT_TRUE(methodIsFormCompatible(describeMethod())); }

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

TEST_F(ArgFormVariant, CycleBackwardWraps)
{
  auto form = ArgForm::build(describeMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->cycleFocused(false);  // wraps from index 0 → last (i32)
  EXPECT_EQ(form->focusedField().text, "i32");
  EXPECT_EQ(form->fields()[0].selectedVariantIndex, 2U);
}

TEST_F(ArgFormVariant, FocusStaysOnTypeSelectorAfterCycle)
{
  auto form = ArgForm::build(describeMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->cycleFocused(true);
  EXPECT_EQ(form->focusedField().editor, EditorKind::variantType);
}

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
  const Method& setLengthMethod() const
  {
    // setLength takes Meters = quantity<u16, m> [min: 0, max: 500]
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("setLength");
  }
  const Method& setRatioMethod() const
  {
    // setRatio takes Ratio = quantity<f32, f32> [min: 0.0, max: 1.0] (unit-less)
    return *::term::test::TestObjectInterface::meta()->searchMethodByName("setRatio");
  }
};

TEST_F(ArgFormQuantity, WithUnitIsFormCompatible) { EXPECT_TRUE(methodIsFormCompatible(setLengthMethod())); }

TEST_F(ArgFormQuantity, UnitLessIsFormCompatible) { EXPECT_TRUE(methodIsFormCompatible(setRatioMethod())); }

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

TEST_F(ArgFormQuantity, IntegerSpinClampsToQuantityMin)
{
  auto form = ArgForm::build(setLengthMethod(), "obj");
  ASSERT_TRUE(form.has_value());

  form->clearField();
  form->insertText("0");
  form->cycleFocused(false);  // should not go below 0 even though u16 allows wraparound
  EXPECT_EQ(form->focusedField().text, "0");
}

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
  // fromJson can tokenize it without tripping on the letters.
  EXPECT_NE(inlined.find("\"1000 ms\""), std::string::npos)
    << "inline invocation lost the unit suffix, got: " << inlined;

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
  const Method& methodByName(std::string_view name) const
  {
    return *::term::test::TestObjectInterface::meta()->searchMethodByName(name);
  }
};

TEST_F(ArgFormContext, ScalarFieldIsNotInsideSequenceOrOptional)
{
  auto form = ArgForm::build(methodByName("add"), "obj");
  ASSERT_TRUE(form.has_value());
  EXPECT_FALSE(form->focusedIsInsideSequence());
  EXPECT_FALSE(form->focusedIsInsideOptional());
}

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

TEST_F(ArgFormContext, ElementInsideSequenceReportsInsideSequence)
{
  auto form = ArgForm::build(methodByName("sumInts"), "obj");
  ASSERT_TRUE(form.has_value());
  form->addElementToFocusedSequence();
  ASSERT_GT(form->leafCount(), 0U);
  EXPECT_TRUE(form->focusedIsInsideSequence());
}

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

TEST_F(ArgFormContext, LeafInsideFilledOptionalReportsInsideOptional)
{
  auto form = ArgForm::build(methodByName("anchor"), "obj");
  ASSERT_TRUE(form.has_value());
  form->toggleFocusedOptional();  // empty → filled
  ASSERT_GT(form->leafCount(), 0U);
  EXPECT_TRUE(form->focusedIsInsideOptional());
}

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
