// === status_bar_test.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "status_bar.h"
#include "test_render_utils.h"

// google test
#include <gtest/gtest.h>

namespace sen::components::term
{
namespace
{

std::string render(const StatusBarData& data, int width = 120)
{
  return test::renderToText(renderStatusBar(data), width, 1);
}

TEST(StatusBar, ShowsAppNameAndHost)
{
  StatusBarData data;
  data.appName = "myApp";
  data.hostName = "myHost";
  auto out = render(data);
  EXPECT_THAT(out, ::testing::HasSubstr("myApp@myHost"));
}

TEST(StatusBar, ShowsAppNameWithoutHost)
{
  StatusBarData data;
  data.appName = "myApp";
  auto out = render(data);
  EXPECT_THAT(out, ::testing::HasSubstr("myApp"));
  EXPECT_THAT(out, ::testing::Not(::testing::HasSubstr("@")));
}

TEST(StatusBar, ShowsScopePath)
{
  StatusBarData data;
  data.scopePath = "/local.main";
  auto out = render(data);
  EXPECT_THAT(out, ::testing::HasSubstr("/local.main"));
}

//--------------------------------------------------------------------------------------------------------------
// Object counts
//--------------------------------------------------------------------------------------------------------------

TEST(StatusBar, ShowsTotalObjectCountAtRoot)
{
  StatusBarData data;
  data.isRootScope = true;
  data.objectsTotal = 42;
  auto out = render(data);
  EXPECT_THAT(out, ::testing::HasSubstr("objects"));
  EXPECT_THAT(out, ::testing::HasSubstr("42"));
  // At root scope, no slash separator
  EXPECT_THAT(out, ::testing::Not(::testing::HasSubstr("/42")));
}

TEST(StatusBar, ShowsScopeAndTotalCountsWhenNotRoot)
{
  StatusBarData data;
  data.isRootScope = false;
  data.objectsInScope = 5;
  data.objectsTotal = 42;
  auto out = render(data);
  EXPECT_THAT(out, ::testing::HasSubstr("5"));
  EXPECT_THAT(out, ::testing::HasSubstr("/"));
  EXPECT_THAT(out, ::testing::HasSubstr("42"));
}

//--------------------------------------------------------------------------------------------------------------
// Watch indicators
//--------------------------------------------------------------------------------------------------------------

TEST(StatusBar, WatchCountVisibleWhenPaneOpen)
{
  StatusBarData data;
  data.watchPaneVisible = true;
  data.watchCount = 3;
  auto out = render(data);
  EXPECT_THAT(out, ::testing::HasSubstr("watches"));
  EXPECT_THAT(out, ::testing::HasSubstr("3"));
  // No F5 hint when pane is visible
  EXPECT_THAT(out, ::testing::Not(::testing::HasSubstr("F5")));
}

TEST(StatusBar, WatchCountHighlightedWhenPaneHidden)
{
  StatusBarData data;
  data.watchPaneVisible = false;
  data.watchCount = 5;
  auto out = render(data);
  EXPECT_THAT(out, ::testing::HasSubstr("watches"));
  EXPECT_THAT(out, ::testing::HasSubstr("5"));
  EXPECT_THAT(out, ::testing::HasSubstr("F5"));
}

TEST(StatusBar, WatchOffWhenPaneHiddenAndNoWatches)
{
  StatusBarData data;
  data.watchPaneVisible = false;
  data.watchCount = 0;
  auto out = render(data);
  EXPECT_THAT(out, ::testing::HasSubstr("off"));
  EXPECT_THAT(out, ::testing::HasSubstr("F5"));
}

//--------------------------------------------------------------------------------------------------------------
// Listeners and sources
//--------------------------------------------------------------------------------------------------------------

TEST(StatusBar, ShowsListenerCount)
{
  StatusBarData data;
  data.listenerCount = 7;
  auto out = render(data);
  EXPECT_THAT(out, ::testing::HasSubstr("listeners"));
  EXPECT_THAT(out, ::testing::HasSubstr("7"));
}

TEST(StatusBar, ShowsSourceCount)
{
  StatusBarData data;
  data.sourceCount = 2;
  auto out = render(data);
  EXPECT_THAT(out, ::testing::HasSubstr("sources"));
  EXPECT_THAT(out, ::testing::HasSubstr("2"));
}

//--------------------------------------------------------------------------------------------------------------
// Pending calls
//--------------------------------------------------------------------------------------------------------------

TEST(StatusBar, PendingCallsHiddenWhenZero)
{
  StatusBarData data;
  data.pendingCalls = 0;
  auto out = render(data);
  EXPECT_THAT(out, ::testing::Not(::testing::HasSubstr("pending")));
}

TEST(StatusBar, PendingCallsShownWhenNonZero)
{
  StatusBarData data;
  data.pendingCalls = 3;
  auto out = render(data);
  EXPECT_THAT(out, ::testing::HasSubstr("pending"));
  EXPECT_THAT(out, ::testing::HasSubstr("3"));
}

//--------------------------------------------------------------------------------------------------------------
// Log pane indicator
//--------------------------------------------------------------------------------------------------------------

TEST(StatusBar, LogIndicatorHiddenWhenPaneVisible)
{
  StatusBarData data;
  data.logPaneVisible = true;
  data.unreadLogCount = 10;
  auto out = render(data);
  EXPECT_THAT(out, ::testing::Not(::testing::HasSubstr("logs")));
}

TEST(StatusBar, LogIndicatorShowsUnreadCountWhenHidden)
{
  StatusBarData data;
  data.logPaneVisible = false;
  data.unreadLogCount = 10;
  auto out = render(data);
  EXPECT_THAT(out, ::testing::HasSubstr("logs"));
  EXPECT_THAT(out, ::testing::HasSubstr("10"));
  EXPECT_THAT(out, ::testing::HasSubstr("F4"));
}

TEST(StatusBar, LogIndicatorShowsOffWhenHiddenNoUnread)
{
  StatusBarData data;
  data.logPaneVisible = false;
  data.unreadLogCount = 0;
  auto out = render(data);
  EXPECT_THAT(out, ::testing::HasSubstr("off"));
  EXPECT_THAT(out, ::testing::HasSubstr("F4"));
}

//--------------------------------------------------------------------------------------------------------------
// Time
//--------------------------------------------------------------------------------------------------------------

TEST(StatusBar, ShowsTime)
{
  StatusBarData data;
  data.timeStr = "14:32:05 UTC";
  auto out = render(data);
  EXPECT_THAT(out, ::testing::HasSubstr("14:32:05 UTC"));
}

//--------------------------------------------------------------------------------------------------------------
// Mode hint takeover
//--------------------------------------------------------------------------------------------------------------

TEST(StatusBar, ModeHintReplacesNormalContent)
{
  StatusBarData data;
  data.appName = "myApp";
  data.objectsTotal = 42;
  data.modeHint = "Tab/Shift+Tab: navigate  Enter: submit  Esc: cancel";
  data.timeStr = "14:32:05 UTC";
  auto out = render(data);

  // Mode hint is shown
  EXPECT_THAT(out, ::testing::HasSubstr("Tab/Shift+Tab"));
  // Time is preserved
  EXPECT_THAT(out, ::testing::HasSubstr("14:32:05 UTC"));
  // Normal content is replaced
  EXPECT_THAT(out, ::testing::Not(::testing::HasSubstr("myApp")));
  EXPECT_THAT(out, ::testing::Not(::testing::HasSubstr("objects")));
}

TEST(StatusBar, ModeHintWithoutTime)
{
  StatusBarData data;
  data.modeHint = "Editing form";
  auto out = render(data);
  EXPECT_THAT(out, ::testing::HasSubstr("Editing form"));
}

}  // namespace
}  // namespace sen::components::term
