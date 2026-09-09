// === build_provenance_test.cpp =======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Nothing else checks what a binary reports about its own build, and the ways it goes wrong are
// quiet: without git the values are empty strings, and a crash report carrying an empty hash
// still looks like a crash report. Asserts the values are well formed rather than pinning a
// commit, which would fail on the next one for the wrong reason.

// kernel
#include "sen/kernel/component.h"

// gtest
#include <gtest/gtest.h>

// std
#include <algorithm>
#include <cctype>
#include <string>

namespace
{

TEST(BuildProvenance, gitHashIsAFullHexSha)
{
  const std::string hash {sen::kernel::getGitHash()};
  ASSERT_FALSE(hash.empty()) << "empty hash: built where git could not be read, so no crash "
                                "report from this binary names a commit";
  EXPECT_EQ(hash.size(), 40U) << "not a full sha: " << hash;
  EXPECT_TRUE(std::all_of(hash.begin(), hash.end(), [](unsigned char c) { return std::isxdigit(c) != 0; }))
    << "not hexadecimal: " << hash;
}

TEST(BuildProvenance, gitRefIsNamed) { EXPECT_FALSE(std::string {sen::kernel::getGitRef()}.empty()); }

TEST(BuildProvenance, buildTimeIsTheCommitTimestamp)
{
  // ISO-8601 from `git log -1 --format=%cI`. Checked by shape rather than parsed: what matters
  // is that something dated is there.
  const std::string when {sen::kernel::getBuildTime()};
  ASSERT_FALSE(when.empty()) << "empty build time";
  EXPECT_GE(when.size(), 20U) << "not an ISO-8601 timestamp: " << when;
  EXPECT_EQ(when[4], '-') << when;
  EXPECT_EQ(when[7], '-') << when;
  EXPECT_EQ(when[10], 'T') << when;
}

TEST(BuildProvenance, gitStatusIsKnown)
{
  // unknown means the status reached the binary as neither clean nor dirty, which is how an
  // empty value arrives when git could not be read.
  EXPECT_NE(sen::kernel::getGitStatus(), sen::kernel::GitStatus::unknown);
}

}  // namespace
