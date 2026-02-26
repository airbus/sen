// === property_flags_test.cpp =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/obj/detail/property_flags.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <string>
#include <tuple>

namespace sen::impl
{

namespace
{

void checkPropertyBits(const PropertyFlags& flags, const uint8_t mask)
{
  uint8_t propMask = 0U;

  if (flags.currentIndex() == 1U)
  {
    propMask |= currMask;  // NOLINT(hicpp-signed-bitwise)
  }

  if (flags.nextIndex() == 1U)
  {
    propMask |= nextMask;  // NOLINT(hicpp-signed-bitwise)
  }

  if (flags.changedInLastCycle())
  {
    propMask |= dirtMask;  // NOLINT(hicpp-signed-bitwise)
  }

  if (flags.getTypesInSync())
  {
    propMask |= syncMask;  // NOLINT(hicpp-signed-bitwise)
  }

  EXPECT_EQ(propMask, mask);
}

/// @test
/// Check creation of PropertyFlags class and mask bits at creation time
/// @requirements(SEN-579)
TEST(PropertyFlags, make)
{
  const auto flags = PropertyFlags();
  const auto flags2 = flags;

  checkPropertyBits(flags, 0U);
  checkPropertyBits(flags2, 0U);
}

/// @test
/// Check advance current method
/// @requirements(SEN-579)
TEST(PropertyFlags, advanceCurrent)
{
  constexpr uint8_t nCalls = 12U;

  auto flags = PropertyFlags();
  checkPropertyBits(flags, 0U);

  // call advance current and check all bits still untouched
  flags.advanceCurrent();
  checkPropertyBits(flags, 0U);

  // re-call advance current multiple times and check all bits still untouched
  for (auto i = 0; i < nCalls; i++)
  {
    flags.advanceCurrent();
  }
  checkPropertyBits(flags, 0U);
}

/// @test
/// Check advance next method
/// @requirements(SEN-579)
TEST(PropertyFlags, advanceNext)
{
  constexpr uint8_t nCalls = 12U;

  auto flags = PropertyFlags();
  checkPropertyBits(flags, 0U);

  // call advance next and check next bit set to 1
  EXPECT_EQ(flags.advanceNext(), 1U);
  checkPropertyBits(flags, nextMask);

  // re-call advance next and check no longer changes were made
  for (auto i = 0; i < nCalls; i++)
  {
    EXPECT_EQ(flags.advanceNext(), 1U);
  }
  checkPropertyBits(flags, nextMask);
}

/// @test
/// CHeck property advance current and next bit methods
/// @requirements(SEN-579)
TEST(PropertyFlags, advanceCurrNext)
{
  auto flags = PropertyFlags();
  std::ignore = flags.advanceNext();
  flags.advanceCurrent();

  // check current = next = dirt = 1
  checkPropertyBits(flags, currMask | nextMask | dirtMask);  // NOLINT(hicpp-signed-bitwise)

  // advance current again for multiple times
  for (auto i = 0; i < 20U; i++)
  {
    flags.advanceCurrent();
  }
  // check current = next = 1, dirt = 0 (no changes)
  checkPropertyBits(flags, currMask | nextMask);  // NOLINT(hicpp-signed-bitwise)

  // advance next and check next = 0
  EXPECT_EQ(flags.advanceNext(), 0U);
  checkPropertyBits(flags, currMask);

  // advance next again for multiple times
  for (auto i = 0; i < 20U; i++)
  {
    EXPECT_EQ(flags.advanceNext(), 0U);
  }
  // check current = 1, next = 0, dir = 0 (no changes)
  checkPropertyBits(flags, currMask);

  // advance current and check current = 0, next = 0, dirt = 1 (changes)
  flags.advanceCurrent();
  checkPropertyBits(flags, dirtMask);
}

/// @test
/// Check set sync method
/// @requirements(SEN-579)
TEST(PropertyFlags, setSync)
{
  auto flags = PropertyFlags();
  EXPECT_FALSE(flags.getTypesInSync());

  // set sync bit to 1
  flags.setTypesInSync(true);
  EXPECT_TRUE(flags.getTypesInSync());

  // set sync bit to 0
  flags.setTypesInSync(false);
  EXPECT_FALSE(flags.getTypesInSync());
}

/// @test
/// Check set value method
/// @requirements(SEN-579)
TEST(PropertyFlags, setValues)
{
  // case cur = 0, next = 0
  {
    PropBuffer<std::string> buff({"aircraft.name", "aircraft.latitude"});
    auto flags = PropertyFlags();
    const std::string value = "aircraft.altitude";

    flags.setValue(buff, value);
    EXPECT_EQ(buff[1], value);
  }

  // case cur = 0, next = 0, cur same value
  {
    PropBuffer<std::string> buff({"aircraft.name", "aircraft.latitude"});
    auto flags = PropertyFlags();
    const std::string value = "aircraft.name";

    flags.setValue(buff, value);
    EXPECT_NE(buff[1], value);
  }

  // case cur = 1, next = 1
  {
    PropBuffer<std::string> buff({"aircraft.name", "aircraft.latitude"});
    auto flags = PropertyFlags();
    std::ignore = flags.advanceNext();
    flags.advanceCurrent();

    const std::string value = "aircraft.size";

    flags.setValue(buff, value);
    EXPECT_EQ(buff[0], value);
  }

  // case cur = 1, next = 1, cur same value
  {
    PropBuffer<std::string> buff({"aircraft.name", "aircraft.latitude"});
    auto flags = PropertyFlags();
    std::ignore = flags.advanceNext();
    flags.advanceCurrent();

    const std::string value = "aircraft.latitude";

    flags.setValue(buff, value);
    EXPECT_NE(buff[0], value);
  }

  // case cur = 0, next = 1
  {
    PropBuffer<std::string> buff({"aircraft.name", "aircraft.latitude"});
    auto flags = PropertyFlags();
    std::ignore = flags.advanceNext();

    const std::string value = "aircraft.description";

    flags.setValue(buff, value);
    EXPECT_EQ(buff[1], value);
  }

  // case cur = 1, next = 0
  {
    PropBuffer<std::string> buff({"aircraft.name", "aircraft.latitude"});
    auto flags = PropertyFlags();
    std::ignore = flags.advanceNext();
    flags.advanceCurrent();
    std::ignore = flags.advanceNext();

    const std::string value = "aircraft.country";

    flags.setValue(buff, value);
    EXPECT_EQ(buff[0], value);
  }
}

}  // namespace

}  // namespace sen::impl
