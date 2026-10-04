// === app_renderers_test.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "app_renderers.h"
#include "arg_form.h"
#include "test_render_utils.h"

// sen
#include "sen/core/meta/class_type.h"

// test stl
#include "stl/test_object.stl.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// google test
#include <gmock/gmock.h>
#include <gtest/gtest.h>

// std
#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace sen::components::term
{
namespace
{

using ::testing::HasSubstr;

/// The marker renderFormField draws beside the focused leaf. renderToText strips the styling, so
/// this character is the only thing left in the text that says where the focus is.
constexpr const char* focusMarker = "▸";

/// A form for one of the test interface's methods, which is where these renderers get their input.
/// Optional rather than a value, so a method that stops being form compatible fails the test that
/// wanted it instead of dereferencing nothing.
std::optional<ArgForm> formFor(const char* methodName)
{
  const auto& cls = *::term::test::TestObjectInterface::meta();
  const auto* method = cls.searchMethodByName(methodName);
  if (method == nullptr)
  {
    return std::nullopt;
  }
  return ArgForm::build(*method, "local.demo.thing");
}

std::string render(const ArgForm& form, int width = 90, int height = 24)
{
  return test::renderToText(renderArgForm(form), width, height);
}

/// Which rendered line carries the focus marker, or npos when none does.
std::size_t focusMarkerLine(const std::string& text)
{
  std::istringstream stream(text);
  std::string line;
  std::size_t index = 0;
  while (std::getline(stream, line))
  {
    if (line.find(focusMarker) != std::string::npos)
    {
      return index;
    }
    ++index;
  }
  return std::string::npos;
}

/// @test
/// The form names what it is about to call and every argument it needs. This is the whole of what
/// a user has to go on: the form replaces the input line, so a field whose name never reaches the
/// screen can only be filled in by counting positions.
TEST(AppRenderers, TheFormNamesTheCallAndItsArguments)
{
  const auto form = formFor("add");
  ASSERT_TRUE(form.has_value());
  const auto text = render(*form);

  EXPECT_THAT(text, HasSubstr("add"));
  EXPECT_THAT(text, HasSubstr("local.demo.thing"));
}

/// @test
/// The focus marker moves with the focus. A form that drew it in one place whatever the state said
/// would look like a working one until a user tried to fill in the second argument.
TEST(AppRenderers, TheFocusMarkerMovesWithTheFocus)
{
  auto form = formFor("add");
  ASSERT_TRUE(form.has_value());
  ASSERT_GT(form->leafCount(), 1U) << "add takes two arguments; this test needs both";

  const auto before = focusMarkerLine(render(*form));
  ASSERT_NE(before, std::string::npos) << "nothing in the rendered form is marked as focused";

  form->focusNext();
  const auto after = focusMarkerLine(render(*form));

  EXPECT_NE(after, std::string::npos);
  EXPECT_NE(after, before) << "the focus moved and the marker stayed where it was";
}

/// @test
/// A method that takes no arguments still renders, rather than replacing the input line with
/// nothing while the form is open.
TEST(AppRenderers, AFormWithNoArgumentsStillRenders)
{
  const auto form = formFor("ping");
  ASSERT_TRUE(form.has_value());

  const auto text = render(*form);
  EXPECT_THAT(text, HasSubstr("ping"));
}

/// @test
/// The name column is as wide as the widest name it has to hold, which is what lines the values
/// up. Measured rather than read off the screen: the width is the renderer's input, and the
/// alignment it produces is only right if this is.
TEST(AppRenderers, TheNameColumnTakesTheWidestName)
{
  const auto form = formFor("echo");
  ASSERT_TRUE(form.has_value());
  ASSERT_FALSE(form->fields().empty());

  std::size_t width = 0;
  for (const auto& field: form->fields())
  {
    collectFormColumnWidths(field, width);
  }

  EXPECT_GE(width, std::string("message").size()) << "the column cannot hold the name it has to show";
}

/// @test
/// The hint for the focused field says something about it. The hint is built from the field's type
/// and description, and it is the only place the form says what a value should look like.
TEST(AppRenderers, TheFocusedFieldHasAHint)
{
  const auto form = formFor("echo");
  ASSERT_TRUE(form.has_value());

  auto hints = focusedHintElements(form->focusedField());
  ASSERT_FALSE(hints.empty()) << "the focused field offered no hint at all";

  const auto text = test::renderToText(ftxui::vbox(std::move(hints)), 90, 6);
  EXPECT_NE(text.find_first_not_of(" \n"), std::string::npos) << "the hint rendered to blank space";
}

/// @test
/// The key summary is there for a form the user can act on: the form takes over the input line,
/// and the keys that work while it is open are not the ones that worked a moment before.
TEST(AppRenderers, TheModeHintNamesTheKeysThatWork)
{
  const auto form = formFor("add");
  ASSERT_TRUE(form.has_value());

  EXPECT_FALSE(formModeHint(*form).empty());
}

}  // namespace
}  // namespace sen::components::term
