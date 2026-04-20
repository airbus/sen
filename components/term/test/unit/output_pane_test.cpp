// === output_pane_test.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "output_pane.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// google test
#include <gtest/gtest.h>

namespace sen::components::term
{
namespace
{

//--------------------------------------------------------------------------------------------------------------
// hasPendingCalls bookkeeping
//--------------------------------------------------------------------------------------------------------------

TEST(OutputPane, NoPendingCallsInitially)
{
  OutputPane pane;
  EXPECT_FALSE(pane.hasPendingCalls());
}

TEST(OutputPane, AppendPendingMakesItPending)
{
  OutputPane pane;
  pane.appendPendingCall(1, "call");
  EXPECT_TRUE(pane.hasPendingCalls());
}

TEST(OutputPane, ReplacePendingClearsTheFlag)
{
  OutputPane pane;
  pane.appendPendingCall(1, "call");
  ASSERT_TRUE(pane.hasPendingCalls());

  pane.replacePendingCall(1, ftxui::text("done"));
  EXPECT_FALSE(pane.hasPendingCalls());
}

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

TEST(OutputPane, ClearResetsPendingCount)
{
  OutputPane pane;
  pane.appendPendingCall(1, "a");
  pane.appendPendingCall(2, "b");
  ASSERT_TRUE(pane.hasPendingCalls());

  pane.clear();
  EXPECT_FALSE(pane.hasPendingCalls());
}

}  // namespace
}  // namespace sen::components::term
