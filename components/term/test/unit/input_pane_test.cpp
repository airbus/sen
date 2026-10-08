// === input_pane_test.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "input_pane.h"
#include "util.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <vector>

namespace sen::components::term
{
namespace
{

//--------------------------------------------------------------------------------------------------------------
// Fixture: each test gets its own temporary history file so they can't interfere with each other
// or with the user's real ~/.sen_history.txt.
//--------------------------------------------------------------------------------------------------------------

class InputPaneHistoryTest: public ::testing::Test
{
protected:
  void SetUp() override
  {
    auto id = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    historyFile = std::filesystem::temp_directory_path() / (std::string("term_history_test_") + id + ".txt");
    std::filesystem::remove(historyFile);
  }

  void TearDown() override { std::filesystem::remove(historyFile); }

  /// Read the history file's exact bytes, so a rewrite that produces the same lines is still visible.
  [[nodiscard]] std::string readFileContent() const
  {
    std::ifstream in(historyFile, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  }

  /// Read the full history file into a vector of lines (oldest first, file order).
  [[nodiscard]] std::vector<std::string> readFileLines() const
  {
    std::vector<std::string> out;
    std::ifstream in(historyFile);
    std::string line;
    while (std::getline(in, line))
    {
      out.push_back(line);
    }
    return out;
  }

  /// Pre-populate the history file with the given lines (chronological order).
  void writeFileLines(const std::vector<std::string>& lines)
  {
    std::ofstream out(historyFile);
    for (const auto& l: lines)
    {
      out << l << '\n';
    }
  }

  // A fixture's members are its tests' locals, so the encapsulation the check asks for has no
  // owner to protect them from. Same reading as libs/core's test fixtures.
  std::filesystem::path historyFile;  // NOLINT(misc-non-private-member-variables-in-classes)
};

//--------------------------------------------------------------------------------------------------------------
// Persistence: writes
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A command added to history is appended to the history file.
TEST_F(InputPaneHistoryTest, AddToHistoryAppendsToFile)
{
  InputPane pane {[](const std::string&) {}};
  pane.setHistoryFile(historyFile);

  pane.addToHistory("ls");
  pane.addToHistory("cd /local.main");
  pane.addToHistory("help");

  auto lines = readFileLines();
  ASSERT_EQ(lines.size(), 3U);
  EXPECT_EQ(lines[0], "ls");
  EXPECT_EQ(lines[1], "cd /local.main");
  EXPECT_EQ(lines[2], "help");
}

/// @test
/// A command identical to the most recent one is not written again.
TEST_F(InputPaneHistoryTest, DuplicateOfMostRecentIsSkippedInFile)
{
  InputPane pane {[](const std::string&) {}};
  pane.setHistoryFile(historyFile);

  pane.addToHistory("ls");
  pane.addToHistory("ls");  // dedup rule: same as most recent → skipped

  auto lines = readFileLines();
  ASSERT_EQ(lines.size(), 1U);
  EXPECT_EQ(lines[0], "ls");
}

/// @test
/// With no history path set, nothing is written anywhere, including under the home directory.
TEST_F(InputPaneHistoryTest, NoFileWritesWhenPathIsEmpty)
{
  // Asserting that `historyFile` does not exist could not fail: nothing had told the pane that name, so
  // the file could not appear however addToHistory behaved. The regression this names is the pane
  // acquiring a default path of its own, so HOME points at an empty directory and the assertion is that
  // nothing is written anywhere under it.
  const auto sandbox = std::filesystem::temp_directory_path() / "term_history_no_default_probe";
  std::filesystem::remove_all(sandbox);
  std::filesystem::create_directories(sandbox);

#ifndef _WIN32
  // POSIX only, because that is where the home variable can be moved. setenv and unsetenv are POSIX and
  // declared in <stdlib.h>; the C++ spelling need not declare them, which is what the NOLINTs are for.
  const char* savedHome = std::getenv("HOME");
  const std::string home = (savedHome != nullptr) ? savedHome : std::string {};
  ASSERT_EQ(::setenv("HOME", sandbox.c_str(), 1), 0);  // NOLINT(misc-include-cleaner)

  {
    InputPane pane {[](const std::string&) {}};
    // Deliberately no setHistoryFile: persistence stays off.
    pane.addToHistory("ls");
  }

  const bool sandboxIsEmpty = std::filesystem::is_empty(sandbox);

  if (home.empty())
  {
    ASSERT_EQ(::unsetenv("HOME"), 0);  // NOLINT(misc-include-cleaner)
  }
  else
  {
    ASSERT_EQ(::setenv("HOME", home.c_str(), 1), 0);  // NOLINT(misc-include-cleaner)
  }

  EXPECT_TRUE(sandboxIsEmpty) << "the pane wrote a history file without being given a path";
#else
  // The same probe needs USERPROFILE and _putenv_s here; until someone can compile and run that, this
  // platform keeps the weaker assertion below and the stronger one above covers the behaviour.
  InputPane pane {[](const std::string&) {}};
  pane.addToHistory("ls");
#endif

  std::filesystem::remove_all(sandbox);
  EXPECT_FALSE(std::filesystem::exists(historyFile));
}

//--------------------------------------------------------------------------------------------------------------
// Persistence: reads
//--------------------------------------------------------------------------------------------------------------

/// @test
/// Loading a history file makes its lines reachable with history up, newest first.
TEST_F(InputPaneHistoryTest, LoadHistoryPopulatesInMemoryNavigation)
{
  writeFileLines({"old1", "old2", "old3"});  // chronological: old1 is oldest

  InputPane pane {[](const std::string&) {}};
  pane.setHistoryFile(historyFile);
  pane.loadHistory();

  // historyUp navigates from newest → oldest. First up should be the last line loaded.
  pane.historyUp();
  EXPECT_EQ(pane.getBuffer(), "old3");
  pane.historyUp();
  EXPECT_EQ(pane.getBuffer(), "old2");
  pane.historyUp();
  EXPECT_EQ(pane.getBuffer(), "old1");
}

/// @test
/// Loading a history file that does not exist does nothing and leaves the buffer alone.
TEST_F(InputPaneHistoryTest, LoadHistoryMissingFileIsSilentNoop)
{
  // File doesn't exist yet, loadHistory must not throw or produce side effects.
  ASSERT_FALSE(std::filesystem::exists(historyFile));

  InputPane pane {[](const std::string&) {}};
  pane.setHistoryFile(historyFile);
  pane.loadHistory();

  // No history → historyUp leaves the buffer alone.
  pane.historyUp();
  EXPECT_EQ(pane.getBuffer(), "");
}

/// @test
/// Empty lines in the history file are not loaded.
TEST_F(InputPaneHistoryTest, LoadHistorySkipsEmptyLines)
{
  writeFileLines({"first", "", "second", "", ""});

  InputPane pane {[](const std::string&) {}};
  pane.setHistoryFile(historyFile);
  pane.loadHistory();

  pane.historyUp();
  EXPECT_EQ(pane.getBuffer(), "second");
  pane.historyUp();
  EXPECT_EQ(pane.getBuffer(), "first");
  pane.historyUp();
  EXPECT_EQ(pane.getBuffer(), "first");  // stays at oldest
}

/// @test
/// A command added after a load is appended after the loaded lines.
TEST_F(InputPaneHistoryTest, NewEntriesAppendAfterLoad)
{
  writeFileLines({"existing"});

  InputPane pane {[](const std::string&) {}};
  pane.setHistoryFile(historyFile);
  pane.loadHistory();

  pane.addToHistory("new1");
  pane.addToHistory("new2");

  auto lines = readFileLines();
  ASSERT_EQ(lines.size(), 3U);
  EXPECT_EQ(lines[0], "existing");
  EXPECT_EQ(lines[1], "new1");
  EXPECT_EQ(lines[2], "new2");
}

/// @test
/// A history file longer than the cap is trimmed in place to the most recent lines.
TEST_F(InputPaneHistoryTest, LoadTrimsFileWhenOverCap)
{
  // Write 2500 lines, more than the 2000-line cap. Load should trim the file in place so
  // only the most-recent 2000 survive.
  std::vector<std::string> fat;
  fat.reserve(2500);
  for (int i = 0; i < 2500; ++i)
  {
    fat.push_back("cmd_" + std::to_string(i));
  }
  writeFileLines(fat);

  InputPane pane {[](const std::string&) {}};
  pane.setHistoryFile(historyFile);
  pane.loadHistory();

  auto afterLoad = readFileLines();
  EXPECT_EQ(afterLoad.size(), 2000U);
  // The oldest surviving line should be cmd_500 (dropped cmd_0..cmd_499).
  EXPECT_EQ(afterLoad.front(), "cmd_500");
  EXPECT_EQ(afterLoad.back(), "cmd_2499");
}

/// @test
/// A history file exactly at the cap is left as it is.
TEST_F(InputPaneHistoryTest, LoadAtExactlyCapDoesNotRewrite)
{
  // When the file has exactly the cap, no rewrite is needed.
  std::vector<std::string> exact;
  exact.reserve(2000);
  for (int i = 0; i < 2000; ++i)
  {
    exact.push_back("e_" + std::to_string(i));
  }
  writeFileLines(exact);

  // Written again without the final newline, so a rewrite is visible. Comparing file_size could not see
  // one: a rewrite at exactly the cap writes back the identical 2,000 lines, so the size is the same
  // either way and the test could not distinguish the thing it exists to check.
  {
    auto content = readFileContent();
    ASSERT_FALSE(content.empty());
    if (content.back() == '\n')
    {
      content.pop_back();
    }
    std::ofstream out(historyFile, std::ios::trunc | std::ios::binary);
    out << content;
  }
  const auto before = readFileContent();

  InputPane pane {[](const std::string&) {}};
  pane.setHistoryFile(historyFile);
  pane.loadHistory();

  EXPECT_EQ(before, readFileContent()) << "the file was rewritten when it was already at the cap";
}

/// @test
/// After a load, a command identical to the last loaded line is not appended to the file.
TEST_F(InputPaneHistoryTest, LoadThenAddDuplicateOfLastLoadedIsSkipped)
{
  writeFileLines({"first", "second"});

  InputPane pane {[](const std::string&) {}};
  pane.setHistoryFile(historyFile);
  pane.loadHistory();

  // Most-recent in history is "second" (from loaded file). Adding the same string again should
  // be a no-op for both in-memory history and the file.
  pane.addToHistory("second");

  auto lines = readFileLines();
  ASSERT_EQ(lines.size(), 2U);
  EXPECT_EQ(lines[1], "second");
}

//--------------------------------------------------------------------------------------------------------------
// History search
//--------------------------------------------------------------------------------------------------------------

/// @test
/// An empty search matches nothing.
TEST(InputPaneSearch, EmptyQueryReturnsFalse)
{
  InputPane pane {[](const std::string&) {}};
  pane.addToHistory("hello world");
  EXPECT_FALSE(pane.searchHistory(""));
}

/// @test
/// A search loads the most recent matching entry into the buffer when several entries match.
TEST(InputPaneSearch, MatchesMostRecentFirst)
{
  InputPane pane {[](const std::string&) {}};
  pane.addToHistory("first match");
  pane.addToHistory("second match");
  pane.addToHistory("unrelated");

  EXPECT_TRUE(pane.searchHistory("match"));
  // Most recent matching entry is "second match" (addToHistory pushes to front).
  EXPECT_EQ(pane.getBuffer(), "second match");
}

/// @test
/// A search for text found in no history entry reports no match.
TEST(InputPaneSearch, NoMatchReturnsFalse)
{
  InputPane pane {[](const std::string&) {}};
  pane.addToHistory("hello");
  EXPECT_FALSE(pane.searchHistory("xyz"));
}

/// @test
/// A search matches on a substring, not only a prefix.
TEST(InputPaneSearch, SubstringMatch)
{
  InputPane pane {[](const std::string&) {}};
  pane.addToHistory("watch local.demo.showcase");
  EXPECT_TRUE(pane.searchHistory("demo"));
  EXPECT_EQ(pane.getBuffer(), "watch local.demo.showcase");
}

}  // namespace
/// @test
/// A line longer than the limit is refused by the in-memory history and the file alike, so it
/// cannot be recallable in one and absent from the other.
TEST_F(InputPaneHistoryTest, BothStoresAnswerTheSameWayForAnOverlongLine)
{
  // Gating only the file leaves an over-long line recallable in this session and absent from the next. The
  // input enforces the same bound before calling, which makes the two agree, but that is the caller's
  // discipline rather than this function's invariant, so it is pinned here.
  const std::string tooLong(maxLineBytes + 1U, 'x');

  InputPane pane {[](const std::string&) {}};
  pane.setHistoryFile(historyFile);
  pane.addToHistory(tooLong);

  pane.historyUp();
  EXPECT_NE(pane.getBuffer(), tooLong) << "kept in memory but not on disk";
  EXPECT_TRUE(readFileLines().empty()) << "written to disk after being refused in memory";
}

/// @test
/// A line exactly at the limit is written and loaded back, so a command the input accepted is
/// still in the next session's history.
TEST_F(InputPaneHistoryTest, ALineAtTheLimitSurvivesARestart)
{
  // Three constants said how long a line may be, in two files, and raising two of them left the third
  // behind: a command the line accepted and ran was silently absent from the next session's history,
  // recallable today and gone tomorrow. One bound now, and this is the invariant that pins it.
  const std::string atTheLimit(maxLineBytes, 'x');

  {
    InputPane pane {[](const std::string&) {}};
    pane.setHistoryFile(historyFile);
    pane.addToHistory(atTheLimit);
    pane.historyUp();
    EXPECT_EQ(pane.getBuffer(), atTheLimit) << "not recallable in the session that ran it";
  }

  InputPane next {[](const std::string&) {}};
  next.setHistoryFile(historyFile);
  next.loadHistory();
  next.historyUp();
  EXPECT_EQ(next.getBuffer(), atTheLimit) << "accepted and run, then missing from the next session";
}

/// @test
/// Walking down through history comes back to the line that was being typed.
///
/// Going up is covered. Coming back down is the half that has to restore something rather than
/// just read it. The unsent line is put aside on the way up and has to reappear at the bottom, or
/// a user who presses Up to check a previous command loses what they were half way through typing.
TEST(InputPaneHistory, WalkingDownComesBackToTheUnsentLine)
{
  InputPane pane {[](const std::string&) {}};
  pane.addToHistory("ls");
  pane.addToHistory("status");
  pane.getBuffer() = "half typed";

  pane.historyUp();
  EXPECT_EQ(pane.getBuffer(), "status") << "the most recent entry comes first";
  pane.historyUp();
  EXPECT_EQ(pane.getBuffer(), "ls");

  pane.historyDown();
  EXPECT_EQ(pane.getBuffer(), "status");
  pane.historyDown();
  EXPECT_EQ(pane.getBuffer(), "half typed") << "the unsent line did not come back";
}

/// @test
/// Down at the bottom of the history does nothing, rather than walking off the end of it.
TEST(InputPaneHistory, DownAtTheBottomIsANoop)
{
  InputPane pane {[](const std::string&) {}};
  pane.addToHistory("ls");
  pane.getBuffer() = "untouched";

  pane.historyDown();
  EXPECT_EQ(pane.getBuffer(), "untouched");

  // And again after a round trip, which is where the index could be left somewhere it should not be.
  pane.historyUp();
  pane.historyDown();
  pane.historyDown();
  EXPECT_EQ(pane.getBuffer(), "untouched");
}

}  // namespace sen::components::term
