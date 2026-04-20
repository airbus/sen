// === command_engine_test.cpp =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Deep `execute()` coverage is not feasible as a unit test: every code path writes
// to the real FTXUI-backed `App`, which requires a live terminal. What this file
// does cover is the static command surface — the table of built-in command
// descriptors and their metadata — which is enough to catch descriptor typos,
// missing fields, duplicate names, and accidental command removals.

#include "command_engine.h"

// google test
#include <gtest/gtest.h>

// std
#include <set>
#include <string>
#include <string_view>

namespace sen::components::term
{
namespace
{

//--------------------------------------------------------------------------------------------------------------
// commandCategoryName
//--------------------------------------------------------------------------------------------------------------

TEST(CommandCategory, EveryCategoryHasAName)
{
  for (auto cat: {CommandCategory::navigation,
                  CommandCategory::discovery,
                  CommandCategory::queries,
                  CommandCategory::logging,
                  CommandCategory::inspection,
                  CommandCategory::monitoring,
                  CommandCategory::general})
  {
    EXPECT_FALSE(commandCategoryName(cat).empty());
  }
}

TEST(CommandCategory, NamesAreStable)
{
  // The human-readable category labels are surfaced in `help` output. Lock
  // them so renames are caught.
  EXPECT_EQ(commandCategoryName(CommandCategory::navigation), "Navigation");
  EXPECT_EQ(commandCategoryName(CommandCategory::discovery), "Discovery");
  EXPECT_EQ(commandCategoryName(CommandCategory::queries), "Queries");
  EXPECT_EQ(commandCategoryName(CommandCategory::logging), "Logging");
  EXPECT_EQ(commandCategoryName(CommandCategory::inspection), "Inspection");
  EXPECT_EQ(commandCategoryName(CommandCategory::monitoring), "Monitoring");
  EXPECT_EQ(commandCategoryName(CommandCategory::general), "General");
}

//--------------------------------------------------------------------------------------------------------------
// Command table shape
//--------------------------------------------------------------------------------------------------------------

TEST(CommandTable, TableIsNonEmpty) { EXPECT_GT(CommandEngine::getCommandTable().size(), 0U); }

TEST(CommandTable, DescriptorsAndTableMatchInSize)
{
  EXPECT_EQ(CommandEngine::getCommandDescriptors().size(), CommandEngine::getCommandTable().size());
}

TEST(CommandTable, EveryEntryHasANameAndHandler)
{
  for (const auto& e: CommandEngine::getCommandTable())
  {
    EXPECT_FALSE(e.desc.name.empty()) << "descriptor has empty name";
    EXPECT_NE(e.handler, nullptr) << "descriptor for '" << e.desc.name << "' has no handler";
  }
}

TEST(CommandTable, EveryDescriptorFieldIsPopulated)
{
  for (const auto& e: CommandEngine::getCommandTable())
  {
    const auto& d = e.desc;
    EXPECT_FALSE(d.usage.empty()) << "'" << d.name << "' missing usage";
    EXPECT_FALSE(d.detail.empty()) << "'" << d.name << "' missing detail";
    EXPECT_FALSE(d.completionHint.empty()) << "'" << d.name << "' missing completion hint";
    EXPECT_FALSE(commandCategoryName(d.category).empty()) << "'" << d.name << "' has unnamed category";
  }
}

TEST(CommandTable, NamesAreUnique)
{
  std::set<std::string_view> seen;
  for (const auto& e: CommandEngine::getCommandTable())
  {
    auto [_, inserted] = seen.insert(e.desc.name);
    EXPECT_TRUE(inserted) << "duplicate command name: '" << e.desc.name << "'";
  }
}

//--------------------------------------------------------------------------------------------------------------
// Expected built-in commands
//--------------------------------------------------------------------------------------------------------------

bool hasCommand(std::string_view name)
{
  for (const auto& e: CommandEngine::getCommandTable())
  {
    if (e.desc.name == name)
    {
      return true;
    }
  }
  return false;
}

const CommandDescriptor* findDescriptor(std::string_view name)
{
  for (const auto& e: CommandEngine::getCommandTable())
  {
    if (e.desc.name == name)
    {
      return &e.desc;
    }
  }
  return nullptr;
}

TEST(CommandTable, ExpectedBuiltInsArePresent)
{
  for (auto name: {"cd",    "pwd",     "ls",      "open",   "close",    "query",     "queries", "log",
                   "watch", "unwatch", "watches", "listen", "unlisten", "listeners", "inspect", "types",
                   "units", "status",  "version", "help",   "clear",    "theme",     "exit",    "shutdown"})
  {
    EXPECT_TRUE(hasCommand(name)) << "missing built-in command: " << name;
  }
}

TEST(CommandTable, ExitAndShutdownShareCategoryAndUsage)
{
  const auto* exitCmd = findDescriptor("exit");
  const auto* shutCmd = findDescriptor("shutdown");
  ASSERT_NE(exitCmd, nullptr);
  ASSERT_NE(shutCmd, nullptr);
  EXPECT_EQ(exitCmd->category, shutCmd->category);
  EXPECT_EQ(exitCmd->usage, shutCmd->usage);
}

//--------------------------------------------------------------------------------------------------------------
// tuiOnly flag
//--------------------------------------------------------------------------------------------------------------

TEST(CommandTable, TuiOnlyFlagsExactlyWatchFamily)
{
  std::set<std::string_view> tuiOnly;
  for (const auto& e: CommandEngine::getCommandTable())
  {
    if (e.desc.tuiOnly)
    {
      tuiOnly.insert(e.desc.name);
    }
  }
  // The watch pane is TUI-only, so the watch-family commands are gated on it.
  // Nothing else should be marked tuiOnly unless a matching UI affordance lands
  // in the term — which is the point of this test.
  EXPECT_EQ(tuiOnly, std::set<std::string_view>({"watch", "unwatch", "watches"}));
}

//--------------------------------------------------------------------------------------------------------------
// Category assignments
//--------------------------------------------------------------------------------------------------------------

TEST(CommandTable, NavigationCommandsAreCategorizedCorrectly)
{
  for (auto name: {"cd", "pwd"})
  {
    const auto* d = findDescriptor(name);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->category, CommandCategory::navigation) << name;
  }
}

TEST(CommandTable, MonitoringCommandsAreCategorizedCorrectly)
{
  for (auto name: {"watch", "unwatch", "watches", "listen", "unlisten", "listeners"})
  {
    const auto* d = findDescriptor(name);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->category, CommandCategory::monitoring) << name;
  }
}

TEST(CommandTable, InspectionCommandsAreCategorizedCorrectly)
{
  for (auto name: {"inspect", "types", "units"})
  {
    const auto* d = findDescriptor(name);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->category, CommandCategory::inspection) << name;
  }
}

}  // namespace
}  // namespace sen::components::term
