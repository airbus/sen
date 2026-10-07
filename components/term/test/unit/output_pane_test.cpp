// === output_pane_test.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "output_pane.h"
#include "test_render_utils.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// google test
#include <gmock/gmock.h>
#include <gtest/gtest.h>

// std
#include <string>

// std
#include <tuple>

namespace sen::components::term
{
namespace
{

//--------------------------------------------------------------------------------------------------------------
// hasPendingCalls bookkeeping
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A new pane has no pending calls.
TEST(OutputPane, NoPendingCallsInitially)
{
  OutputPane pane;
  EXPECT_FALSE(pane.hasPendingCalls());
}

/// @test
/// Appending a pending entry marks the pane as having pending calls.
TEST(OutputPane, AppendPendingMakesItPending)
{
  OutputPane pane;
  pane.appendPendingCall(1, "call");
  EXPECT_TRUE(pane.hasPendingCalls());
}

/// @test
/// Replacing the pending entry with its result clears the flag.
TEST(OutputPane, ReplacePendingClearsTheFlag)
{
  OutputPane pane;
  pane.appendPendingCall(1, "call");
  ASSERT_TRUE(pane.hasPendingCalls());

  pane.replacePendingCall(1, ftxui::text("done"));
  EXPECT_FALSE(pane.hasPendingCalls());
}

/// @test
/// Two pending entries are tracked separately, so replacing one leaves the other pending.
TEST(OutputPane, MultiplePendingTrackedIndependently)
{
  OutputPane pane;
  pane.appendPendingCall(1, "first");
  pane.appendPendingCall(2, "second");
  pane.appendPendingCall(3, "third");
  EXPECT_TRUE(pane.hasPendingCalls());

  pane.replacePendingCall(2, ftxui::text("second done"));
  EXPECT_TRUE(pane.hasPendingCalls());

  pane.replacePendingCall(1, ftxui::text("first done"));
  EXPECT_TRUE(pane.hasPendingCalls());

  pane.replacePendingCall(3, ftxui::text("third done"));
  EXPECT_FALSE(pane.hasPendingCalls());
}

/// @test
/// Replacing an id the pane does not hold changes nothing.
TEST(OutputPane, ReplaceWithUnknownIdIsNoop)
{
  OutputPane pane;
  pane.appendPendingCall(1, "call");

  // Unknown id: nothing should change.
  pane.replacePendingCall(999, ftxui::text("stray"));
  EXPECT_TRUE(pane.hasPendingCalls());

  // The real id should still resolve.
  pane.replacePendingCall(1, ftxui::text("done"));
  EXPECT_FALSE(pane.hasPendingCalls());
}

/// @test
/// Replacing the same id twice does not take the pending count below zero.
TEST(OutputPane, ReplaceTwiceDoesNotUnderflowPendingCount)
{
  // Replacing the same id twice is a no-op the second time.
  OutputPane pane;
  pane.appendPendingCall(1, "call");

  pane.replacePendingCall(1, ftxui::text("first"));
  EXPECT_FALSE(pane.hasPendingCalls());

  pane.replacePendingCall(1, ftxui::text("again"));
  EXPECT_FALSE(pane.hasPendingCalls());

  // A fresh pending call should still be trackable.
  pane.appendPendingCall(2, "another");
  EXPECT_TRUE(pane.hasPendingCalls());
}

//--------------------------------------------------------------------------------------------------------------
// trimLines interacts with pendingCount
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A pending entry dropped by trimming stops being counted as pending.
TEST(OutputPane, TrimmingPendingEntryDecrementsCount)
{
  OutputPane pane;
  pane.setMaxLines(2);

  // Fill with a pending entry at the top.
  pane.appendPendingCall(1, "spinner");
  pane.appendText("a");
  ASSERT_TRUE(pane.hasPendingCalls());

  // Third append forces trim, the pending entry (oldest) is dropped.
  pane.appendText("b");
  EXPECT_FALSE(pane.hasPendingCalls());
}

/// @test
/// Trimming an entry that was not pending leaves the pending count alone.
TEST(OutputPane, TrimmingNonPendingEntryKeepsCount)
{
  OutputPane pane;
  pane.setMaxLines(2);

  // Pending entry in the middle, a non-pending entry is the one dropped.
  pane.appendText("top");
  pane.appendPendingCall(1, "spinner");
  // Adding a third entry drops "top" (non-pending). Pending count unchanged.
  pane.appendText("bottom");

  EXPECT_TRUE(pane.hasPendingCalls());

  // And replacing the pending entry still works, i.e. the count is accurate.
  pane.replacePendingCall(1, ftxui::text("done"));
  EXPECT_FALSE(pane.hasPendingCalls());
}

/// @test
/// Replacing an entry that trimming already dropped changes nothing.
TEST(OutputPane, ReplaceAfterTrimIsNoop)
{
  OutputPane pane;
  pane.setMaxLines(1);

  pane.appendPendingCall(1, "spinner");
  // Push it off the end.
  pane.appendText("replacement");
  ASSERT_FALSE(pane.hasPendingCalls());

  // Replace targeting the trimmed id should be a quiet no-op.
  pane.replacePendingCall(1, ftxui::text("too late"));
  EXPECT_FALSE(pane.hasPendingCalls());
}

//--------------------------------------------------------------------------------------------------------------
// clear() resets pending state
//--------------------------------------------------------------------------------------------------------------

/// @test
/// Clearing the pane leaves no pending calls.
TEST(OutputPane, ClearResetsPendingCount)
{
  OutputPane pane;
  pane.appendPendingCall(1, "a");
  pane.appendPendingCall(2, "b");
  ASSERT_TRUE(pane.hasPendingCalls());

  pane.clear();
  EXPECT_FALSE(pane.hasPendingCalls());
}

//--------------------------------------------------------------------------------------------------------------
// Scrolling
//--------------------------------------------------------------------------------------------------------------
//
// Every case here renders before it scrolls, and that is the point. The scroll step is
// `rows / contentHeight_`, and contentHeight_ starts at 1 and is only ever assigned inside render(),
// which returns early on an empty pane. So a test that scrolls a pane it has not rendered gets a step of
// `rows`, saturates immediately, and passes for any step arithmetic whatsoever, including none.

namespace
{

/// Fill a pane with numbered lines and render it once, so contentHeight_ is the real row count.
void fillAndRender(OutputPane& pane, int lines, int width = 40, int height = 10)
{
  for (int i = 0; i < lines; ++i)
  {
    pane.appendText("line_" + std::to_string(i));
  }
  std::ignore = test::renderToText(pane.render(), width, height);
}

/// What the pane draws right now, at a viewport smaller than its content.
std::string visible(OutputPane& pane, int width = 40, int height = 10)
{
  return test::renderToText(pane.render(), width, height);
}

}  // namespace

/// @test
/// A pane that has not been scrolled shows the newest lines.
TEST(OutputPaneScroll, AFreshPaneFollowsTheBottom)
{
  OutputPane pane;
  fillAndRender(pane, 60);
  auto out = visible(pane);
  EXPECT_THAT(out, ::testing::HasSubstr("line_59")) << "a pane that has not been scrolled shows the newest";
  EXPECT_THAT(out, ::testing::Not(::testing::HasSubstr("line_0")));
}

/// @test
/// Scrolling up shows earlier lines and stops following the newest.
TEST(OutputPaneScroll, ScrollingUpShowsEarlierLinesAndStopsFollowing)
{
  OutputPane pane;
  fillAndRender(pane, 60);

  pane.scrollUp(20);
  auto out = visible(pane);
  EXPECT_THAT(out, ::testing::Not(::testing::HasSubstr("line_59"))) << "scrolling up did not move the view";

  // And it stays put when more arrives, which is what followBottom_ going false means.
  pane.appendText("arrived_later");
  EXPECT_THAT(visible(pane), ::testing::Not(::testing::HasSubstr("arrived_later")))
    << "the view jumped to the bottom after scrolling up";
}

/// @test
/// A scroll step moves a fraction of the content rather than a fixed number of rows, so the same
/// request moves further through short content than through long content.
TEST(OutputPaneScroll, TheStepIsProportionalToTheContentNotTheRequest)
{
  // The property the saturating fresh-pane test could not see: on 600 lines, scrolling up 20 rows moves
  // a twentieth of the way, so line_0 is nowhere near the view. On 30 lines the same request reaches the
  // top. A step of `rows` rather than `rows / contentHeight_` makes both of these show line_0.
  OutputPane tall;
  fillAndRender(tall, 600);
  tall.scrollUp(20);
  EXPECT_THAT(visible(tall), ::testing::Not(::testing::HasSubstr("line_0")))
    << "one scroll of 20 rows reached the top of 600 lines, so the step ignores the content height";

  OutputPane shortPane;
  fillAndRender(shortPane, 30);
  shortPane.scrollUp(40);
  EXPECT_THAT(visible(shortPane), ::testing::HasSubstr("line_0")) << "scrolling past the top did not clamp there";
}

/// @test
/// Scrolling to the bottom shows the newest lines and follows them again.
TEST(OutputPaneScroll, ScrollToBottomComesBackAndFollowsAgain)
{
  OutputPane pane;
  fillAndRender(pane, 60);
  pane.scrollUp(20);
  ASSERT_THAT(visible(pane), ::testing::Not(::testing::HasSubstr("line_59")));

  pane.scrollToBottom();
  EXPECT_THAT(visible(pane), ::testing::HasSubstr("line_59"));

  pane.appendText("arrived_later");
  EXPECT_THAT(visible(pane), ::testing::HasSubstr("arrived_later")) << "following the bottom did not resume";
}

/// @test
/// Scrolling down from the top returns to following the newest lines.
TEST(OutputPaneScroll, ScrollingDownFromTheTopReturnsToFollowing)
{
  OutputPane pane;
  fillAndRender(pane, 60);
  pane.scrollUp(200);  // well past the top
  ASSERT_THAT(visible(pane), ::testing::HasSubstr("line_0"));

  pane.scrollDown(200);
  EXPECT_THAT(visible(pane), ::testing::HasSubstr("line_59"));
  pane.appendText("arrived_later");
  EXPECT_THAT(visible(pane), ::testing::HasSubstr("arrived_later")) << "reaching the bottom did not resume following";
}

}  // namespace
}  // namespace sen::components::term
