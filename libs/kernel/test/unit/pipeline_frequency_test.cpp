// === pipeline_frequency_test.cpp =====================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// kernel
#include "sen/kernel/bootloader.h"
#include "sen/kernel/kernel_config.h"

// gtest
#include <gtest/gtest.h>

// std
#include <chrono>
#include <exception>
#include <string>

namespace
{

/// A pipeline with nothing in it, so only the frequency is under test.
std::string configWith(const std::string& frequencyLine)
{
  return "build:\n  - name: p\n    imports: []\n    objects: []\n" + frequencyLine;
}

}  // namespace

/// @test
/// Zero is refused.
TEST(APipelineFrequency, refusesZero)
{
  EXPECT_THROW(static_cast<void>(sen::kernel::Bootloader::fromYamlString(configWith("    freqHz: 0\n"), false)),
               std::exception);
}

/// @test
/// A negative frequency is refused; it used to give a negative period.
TEST(APipelineFrequency, refusesANegativeValue)
{
  EXPECT_THROW(static_cast<void>(sen::kernel::Bootloader::fromYamlString(configWith("    freqHz: -5\n"), false)),
               std::exception);
}

/// @test
/// Below one hertz is legal: 0.5 is a two second cycle.
TEST(APipelineFrequency, acceptsLessThanOneHertz)
{
  const auto bootloader = sen::kernel::Bootloader::fromYamlString(configWith("    freqHz: 0.5\n"), false);
  EXPECT_EQ(bootloader->getConfig().getPipelinesToLoad().front().period, sen::Duration {std::chrono::seconds {2}});
}

/// @test
/// Omitting it gives the default rather than an error.
TEST(APipelineFrequency, defaultsWhenOmitted)
{
  const auto bootloader = sen::kernel::Bootloader::fromYamlString(configWith(""), false);
  EXPECT_EQ(bootloader->getConfig().getPipelinesToLoad().front().period, sen::Duration::fromHertz(30.0F));
}
