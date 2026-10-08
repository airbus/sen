// === config_test.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "component.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/kernel/test_kernel.h"

// google test
#include <gtest/gtest.h>

// std
#include <string>

/// @test
/// Loads the REST component from a YAML configuration and exposes the configured listen
/// address 127.0.0.1 and port 12345.
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
/// Aborts kernel construction when the REST configuration carries an unparseable listen
/// address.
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

/// @test
/// Falls back to the default update frequency when the configuration does not set freqHz.
/// @requirements(SEN-1061)
TEST(Rest, config_default_update_freq)
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

  ASSERT_EQ(component->getUpdateFreq(), sen::components::rest::defaultRestAPIUpdateFreq);
}

/// @test
/// Applies the configured freqHz of 60 as the component's update frequency.
/// @requirements(SEN-1061)
TEST(Rest, config_custom_update_freq)
{
  std::string configString = R"(
    load:
    - name: rest
      group: 3
      address: "127.0.0.1"
      port: 12345
      freqHz: 60.0
  )";

  auto kernel = sen::kernel::TestKernel::fromYamlString(configString);
  auto context = kernel.getComponentContext("rest");
  ASSERT_TRUE(context.has_value());

  auto component = dynamic_cast<const sen::components::rest::RestAPIComponent*>(context.value()->instance);

  ASSERT_FLOAT_EQ(component->getUpdateFreq().toSeconds(), sen::Duration::fromHertz(60.0).toSeconds());
}
