// === config_test.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "component.h"

// sen
#include "sen/kernel/test_kernel.h"

// google test
#include <gtest/gtest.h>

// std
#include <string>

/// @test
/// Check validation of the REST API configuration
/// @requirements(SEN-1061)
TEST(Rest, success_config)
{
  std::string configString = R"(
    load:
    - name: rest
      group: 3
      address: "127.0.0.1"
      port: 12345
  )";

  auto kernel = sen::kernel::TestKernel::fromYamlString(configString);
  auto context = kernel.getComponentContext("rest");
  ASSERT_TRUE(context.has_value());

  auto component = dynamic_cast<const sen::components::rest::RestAPIComponent*>(context.value()->instance);

  ASSERT_EQ(component->getListenAddress(), "127.0.0.1");
  ASSERT_EQ(component->getListenPort(), 12345);
}

/// @test
/// Check REST API with a default configuration is correct
/// @requirements(SEN-1061)
TEST(Rest, success_default_config)
{
  std::string configString = R"(
    load:
    - name: rest
      group: 3
      address: "127.0.0.1"
      port: 12345
  )";

  auto kernel = sen::kernel::TestKernel::fromYamlString(configString);
  auto context = kernel.getComponentContext("rest");
  ASSERT_TRUE(context.has_value());

  auto component = dynamic_cast<const sen::components::rest::RestAPIComponent*>(context.value()->instance);

  ASSERT_EQ(component->getListenAddress(), "127.0.0.1");
  ASSERT_EQ(component->getListenPort(), 12345);
}

/// @test
/// Check REST API with an invalid configuration results in an error
/// @requirements(SEN-1061)
TEST(Rest, invalid_config_address)
{
  std::string configString = R"(
    load:
    - name: rest
      group: 3
      address: "invalid"
      port: 12345
  )";

  // NOLINTNEXTLINE(hicpp-vararg, hicpp-avoid-goto)
  EXPECT_DEATH(sen::kernel::TestKernel::fromYamlString(configString), "");
}
