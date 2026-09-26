// === signature_renderer_test.cpp =====================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "signature_renderer.h"
#include "test_object_impl.h"
#include "test_render_utils.h"

// sen
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/var.h"

// google test
#include <gtest/gtest.h>

namespace sen::components::term
{
namespace
{

using test::findTestMethod;
using test::findTestProperty;
using test::renderToText;

//--------------------------------------------------------------------------------------------------------------
// renderMethodSignature
//--------------------------------------------------------------------------------------------------------------

TEST(SignatureRenderer, MethodHeaderAndObjectName)
{
  auto* method = findTestMethod("add");
  ASSERT_NE(method, nullptr);

  auto out = renderToText(renderMethodSignature("testObj", *method));

  EXPECT_THAT(out, ::testing::HasSubstr("METHOD"));
  EXPECT_THAT(out, ::testing::HasSubstr("testObj."));
  EXPECT_THAT(out, ::testing::HasSubstr("add"));
}

TEST(SignatureRenderer, MethodMarksConst)
{
  // `add` is declared `[const]` in the stl.
  auto* method = findTestMethod("add");
  ASSERT_NE(method, nullptr);
  ASSERT_EQ(method->getConstness(), Constness::constant);

  auto out = renderToText(renderMethodSignature("testObj", *method));
  EXPECT_THAT(out, ::testing::HasSubstr("[const]"));
}

TEST(SignatureRenderer, MethodSkipsConstMarkerForNonConstMethods)
{
  // `reset` is not const.
  auto* method = findTestMethod("reset");
  ASSERT_NE(method, nullptr);
  ASSERT_NE(method->getConstness(), Constness::constant);

  auto out = renderToText(renderMethodSignature("testObj", *method));
  EXPECT_THAT(out, ::testing::Not(::testing::HasSubstr("[const]")));
}

TEST(SignatureRenderer, MethodArgumentsSection)
{
  auto* method = findTestMethod("add");
  ASSERT_NE(method, nullptr);

  auto out = renderToText(renderMethodSignature("testObj", *method));

  EXPECT_THAT(out, ::testing::HasSubstr("ARGUMENTS"));
  EXPECT_THAT(out, ::testing::HasSubstr("a"));
  EXPECT_THAT(out, ::testing::HasSubstr("b"));
  // i32 is the declared type
  EXPECT_THAT(out, ::testing::HasSubstr("i32"));
}

TEST(SignatureRenderer, MethodNoArgsSkipsArgumentsSection)
{
  auto* method = findTestMethod("ping");
  ASSERT_NE(method, nullptr);
  ASSERT_TRUE(method->getArgs().empty());

  auto out = renderToText(renderMethodSignature("testObj", *method));
  EXPECT_THAT(out, ::testing::Not(::testing::HasSubstr("ARGUMENTS")));
}

TEST(SignatureRenderer, MethodReturnsSection)
{
  // `ping` returns string → the RETURNS section should appear.
  auto* method = findTestMethod("ping");
  ASSERT_NE(method, nullptr);

  auto out = renderToText(renderMethodSignature("testObj", *method));
  EXPECT_THAT(out, ::testing::HasSubstr("RETURNS"));
  EXPECT_THAT(out, ::testing::HasSubstr("string"));
}

TEST(SignatureRenderer, MethodVoidReturnOmitsReturnsSection)
{
  // `reset` returns void → no RETURNS line.
  auto* method = findTestMethod("reset");
  ASSERT_NE(method, nullptr);
  ASSERT_TRUE(method->getReturnType()->isVoidType());

  auto out = renderToText(renderMethodSignature("testObj", *method));
  EXPECT_THAT(out, ::testing::Not(::testing::HasSubstr("RETURNS")));
}

//--------------------------------------------------------------------------------------------------------------
// renderPropertyValue
//--------------------------------------------------------------------------------------------------------------

TEST(SignatureRenderer, PropertyValueInlineForScalar)
{
  auto* prop = findTestProperty("counter");
  ASSERT_NE(prop, nullptr);

  auto out = renderToText(renderPropertyValue(*prop, Var(int32_t {42})));
  EXPECT_THAT(out, ::testing::HasSubstr("counter"));
  EXPECT_THAT(out, ::testing::HasSubstr("42"));
}

TEST(SignatureRenderer, PropertyValueEmpty)
{
  // An empty Var should render inline as <empty> (the formatter's fallback).
  auto* prop = findTestProperty("status");
  ASSERT_NE(prop, nullptr);

  auto out = renderToText(renderPropertyValue(*prop, Var {}));
  EXPECT_THAT(out, ::testing::HasSubstr("status"));
  EXPECT_THAT(out, ::testing::HasSubstr("<empty>"));
}

//--------------------------------------------------------------------------------------------------------------
// renderMethodResult
//--------------------------------------------------------------------------------------------------------------

TEST(SignatureRenderer, MethodResultVoidShowsOK)
{
  auto out = renderToText(renderMethodResult(Var {}, *VoidType::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("OK"));
}

TEST(SignatureRenderer, MethodResultValueRendersValue)
{
  auto out = renderToText(renderMethodResult(Var(int32_t {7}), *Int32Type::get()));
  EXPECT_THAT(out, ::testing::HasSubstr("7"));
  EXPECT_THAT(out, ::testing::Not(::testing::HasSubstr("OK")));
}

//--------------------------------------------------------------------------------------------------------------
// renderError
//--------------------------------------------------------------------------------------------------------------

TEST(SignatureRenderer, ErrorBoxContainsTitleAndMessage)
{
  auto out = renderToText(renderError("Unknown Method", "no such method: fooBar"));
  EXPECT_THAT(out, ::testing::HasSubstr("Error:"));
  EXPECT_THAT(out, ::testing::HasSubstr("Unknown Method"));
  EXPECT_THAT(out, ::testing::HasSubstr("no such method: fooBar"));
}

}  // namespace
}  // namespace sen::components::term
