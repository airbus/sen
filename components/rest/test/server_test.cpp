// === server_test.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/kernel/test_kernel.h"

// google test
#include <gtest/gtest.h>

// std
#include <atomic>
#include <chrono>
#include <string>
#include <string_view>
#include <thread>

constexpr std::string_view configString = R"(
    load:
    - name: rest
      group: 3
      address: "127.0.0.1"
      port: 12345
      threadPoolSize: 5
  )";

/// @test
/// Test allocation and deallocation of the HTTP server
/// @requirements(SEN-1061)
TEST(Rest, server_allocation)
{
  std::atomic<bool> stop;
  std::thread testThread(
    [&stop]
    {
      while (!stop)
      {
        auto kernel = sen::kernel::TestKernel::fromYamlString(std::string(configString));
        kernel.step();
      }
    });

  std::this_thread::sleep_for(std::chrono::seconds(5));
  stop = true;

  testThread.join();
}
