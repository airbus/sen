// === command_engine_test.cpp =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Deep `execute()` coverage is not feasible as a unit test: every code path writes to the real
// FTXUI-backed `App`, which needs a live terminal. What this file covers is the static command surface,
// the table of built-in descriptors and their metadata, which catches typos, missing fields, duplicate
// names and accidental removals.

#include "command_engine.h"

// google test
#include <gtest/gtest.h>

// std
#include <set>
#include <string_view>

namespace sen::components::term
{
namespace
{

//--------------------------------------------------------------------------------------------------------------
// commandCategoryName
//--------------------------------------------------------------------------------------------------------------

/// @test
/// Every command category has a non-empty name.
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

/// @test
/// The category names are the ones help prints, so a rename is caught here.
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

/// @test
/// The command table holds commands.
TEST(CommandTable, TableIsNonEmpty) { EXPECT_GT(CommandEngine::getCommandTable().size(), 0U); }

/// @test
/// The descriptor list and the command table hold the same number of entries.
TEST(CommandTable, DescriptorsAndTableMatchInSize)
{
  EXPECT_EQ(CommandEngine::getCommandDescriptors().size(), CommandEngine::getCommandTable().size());
}

/// @test
/// Every command has a name and a handler.
TEST(CommandTable, EveryEntryHasANameAndHandler)
{
  for (const auto& e: CommandEngine::getCommandTable())
  {
    EXPECT_FALSE(e.desc.name.empty()) << "descriptor has empty name";
    EXPECT_NE(e.handler, nullptr) << "descriptor for '" << e.desc.name << "' has no handler";
  }
}

/// @test
/// Every command has a usage line, a detail line, a completion hint and a named category.
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

/// @test
/// No two commands share a name.
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

/// @test
/// The built-in commands a user expects are all registered.
TEST(CommandTable, ExpectedBuiltInsArePresent)
{
  for (auto name: {"cd",     "pwd",     "ls",       "open",      "close",   "query", "queries",
                   "log",    "listen",  "unlisten", "listeners", "inspect", "types", "units",
                   "status", "version", "help",     "clear",     "theme",   "exit",  "shutdown"})
  {
    EXPECT_TRUE(hasCommand(name)) << "missing built-in command: " << name;
  }
}

/// @test
/// exit and shutdown carry the same category and usage, since they differ only in what they stop.
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
// Category assignments
//--------------------------------------------------------------------------------------------------------------

/// @test
/// cd and pwd are in the navigation category.
TEST(CommandTable, NavigationCommandsAreCategorizedCorrectly)
{
  for (auto name: {"cd", "pwd"})
  {
    const auto* d = findDescriptor(name);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->category, CommandCategory::navigation) << name;
  }
}

/// @test
/// listen, unlisten and listeners are in the monitoring category.
TEST(CommandTable, MonitoringCommandsAreCategorizedCorrectly)
{
  for (auto name: {"listen", "unlisten", "listeners"})
  {
    const auto* d = findDescriptor(name);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->category, CommandCategory::monitoring) << name;
  }
}

/// @test
/// The inspection commands are in the inspection category.
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
