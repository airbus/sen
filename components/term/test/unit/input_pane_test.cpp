// === input_pane_test.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "input_pane.h"

// google test
#include <gtest/gtest.h>

// std
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

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
    historyFile_ = std::filesystem::temp_directory_path() / (std::string("term_history_test_") + id + ".txt");
    std::filesystem::remove(historyFile_);
  }

  void TearDown() override { std::filesystem::remove(historyFile_); }

  /// Read the full history file into a vector of lines (oldest first, file order).
  std::vector<std::string> readFileLines() const
  {
    std::vector<std::string> out;
    std::ifstream in(historyFile_);
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
    std::ofstream out(historyFile_);
    for (const auto& l: lines)
    {
      out << l << '\n';
    }
  }

  std::filesystem::path historyFile_;
};

//--------------------------------------------------------------------------------------------------------------
// Persistence: writes
//--------------------------------------------------------------------------------------------------------------

TEST_F(InputPaneHistoryTest, AddToHistoryAppendsToFile)
{
  InputPane pane {[](const std::string&) {}};
  pane.setHistoryFile(historyFile_);

  pane.addToHistory("ls");
  pane.addToHistory("cd /local.main");
  pane.addToHistory("help");

  auto lines = readFileLines();
  ASSERT_EQ(lines.size(), 3U);
  EXPECT_EQ(lines[0], "ls");
  EXPECT_EQ(lines[1], "cd /local.main");
  EXPECT_EQ(lines[2], "help");
}

TEST_F(InputPaneHistoryTest, DuplicateOfMostRecentIsSkippedInFile)
{
  InputPane pane {[](const std::string&) {}};
  pane.setHistoryFile(historyFile_);

  pane.addToHistory("ls");
  pane.addToHistory("ls");  // dedup rule: same as most recent → skipped

  auto lines = readFileLines();
  ASSERT_EQ(lines.size(), 1U);
  EXPECT_EQ(lines[0], "ls");
}

TEST_F(InputPaneHistoryTest, NoFileWritesWhenPathIsEmpty)
{
  InputPane pane {[](const std::string&) {}};
  // Deliberately do not call setHistoryFile, persistence stays off.

  pane.addToHistory("ls");

  EXPECT_FALSE(std::filesystem::exists(historyFile_));
}

//--------------------------------------------------------------------------------------------------------------
// Persistence: reads
//--------------------------------------------------------------------------------------------------------------

TEST_F(InputPaneHistoryTest, LoadHistoryPopulatesInMemoryNavigation)
{
  writeFileLines({"old1", "old2", "old3"});  // chronological: old1 is oldest

  InputPane pane {[](const std::string&) {}};
  pane.setHistoryFile(historyFile_);
  pane.loadHistory();

  // historyUp navigates from newest → oldest. First up should be the last line loaded.
  pane.historyUp();
  EXPECT_EQ(pane.getBuffer(), "old3");
  pane.historyUp();
  EXPECT_EQ(pane.getBuffer(), "old2");
  pane.historyUp();
  EXPECT_EQ(pane.getBuffer(), "old1");
}

TEST_F(InputPaneHistoryTest, LoadHistoryMissingFileIsSilentNoop)
{
  // File doesn't exist yet, loadHistory must not throw or produce side effects.
  ASSERT_FALSE(std::filesystem::exists(historyFile_));

  InputPane pane {[](const std::string&) {}};
  pane.setHistoryFile(historyFile_);
  pane.loadHistory();

  // No history → historyUp leaves the buffer alone.
  pane.historyUp();
  EXPECT_EQ(pane.getBuffer(), "");
}

TEST_F(InputPaneHistoryTest, LoadHistorySkipsEmptyLines)
{
  writeFileLines({"first", "", "second", "", ""});

  InputPane pane {[](const std::string&) {}};
  pane.setHistoryFile(historyFile_);
  pane.loadHistory();

  pane.historyUp();
  EXPECT_EQ(pane.getBuffer(), "second");
  pane.historyUp();
  EXPECT_EQ(pane.getBuffer(), "first");
  pane.historyUp();
  EXPECT_EQ(pane.getBuffer(), "first");  // stays at oldest
}

TEST_F(InputPaneHistoryTest, NewEntriesAppendAfterLoad)
{
  writeFileLines({"existing"});

  InputPane pane {[](const std::string&) {}};
  pane.setHistoryFile(historyFile_);
  pane.loadHistory();

  pane.addToHistory("new1");
  pane.addToHistory("new2");

  auto lines = readFileLines();
  ASSERT_EQ(lines.size(), 3U);
  EXPECT_EQ(lines[0], "existing");
  EXPECT_EQ(lines[1], "new1");
  EXPECT_EQ(lines[2], "new2");
}

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
  pane.setHistoryFile(historyFile_);
  pane.loadHistory();

  auto afterLoad = readFileLines();
  EXPECT_EQ(afterLoad.size(), 2000U);
  // The oldest surviving line should be cmd_500 (dropped cmd_0..cmd_499).
  EXPECT_EQ(afterLoad.front(), "cmd_500");
  EXPECT_EQ(afterLoad.back(), "cmd_2499");
}

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

  auto beforeSize = std::filesystem::file_size(historyFile_);

  InputPane pane {[](const std::string&) {}};
  pane.setHistoryFile(historyFile_);
  pane.loadHistory();

  auto afterSize = std::filesystem::file_size(historyFile_);
  EXPECT_EQ(beforeSize, afterSize);
}

TEST_F(InputPaneHistoryTest, LoadThenAddDuplicateOfLastLoadedIsSkipped)
{
  writeFileLines({"first", "second"});

  InputPane pane {[](const std::string&) {}};
  pane.setHistoryFile(historyFile_);
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

TEST(InputPaneSearch, EmptyQueryReturnsFalse)
{
  InputPane pane {[](const std::string&) {}};
  pane.addToHistory("hello world");
  EXPECT_FALSE(pane.searchHistory(""));
}

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

TEST(InputPaneSearch, NoMatchReturnsFalse)
{
  InputPane pane {[](const std::string&) {}};
  pane.addToHistory("hello");
  EXPECT_FALSE(pane.searchHistory("xyz"));
}

TEST(InputPaneSearch, SubstringMatch)
{
  InputPane pane {[](const std::string&) {}};
  pane.addToHistory("watch local.demo.showcase");
  EXPECT_TRUE(pane.searchHistory("demo"));
  EXPECT_EQ(pane.getBuffer(), "watch local.demo.showcase");
}

}  // namespace
}  // namespace sen::components::term
