// === ether_transport_test.cpp ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "discovery.h"
#include "ether_transport.h"
#include "network_exclusion.h"
#include "network_footprint.h"

// generated code
#include "stl/configuration.stl.h"

// google test
#include <gtest/gtest.h>

// asio
#include <asio/io_context.hpp>

// std
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace sen::components::ether
{

namespace
{

class EtherTransportTest: public testing::Test
{
protected:
  EtherTransportTest()
    : discovery_(DiscoverySystem::make(config_, discoveryIo_))
    , runtimeFootprint_(std::make_shared<RuntimeNetworkFootprintState>(config_, exclusions_))
  {
  }

  [[nodiscard]] static Configuration makeConfiguration()
  {
    Configuration config {};
    config.discovery = TcpDiscovery {};
    return config;
  }

  [[nodiscard]] std::unique_ptr<EtherTransport> makeTransport() const
  {
    return std::make_unique<EtherTransport>(
      config_, "etherTransportTest", "etherTransportTest", discovery_, nullptr, exclusions_, runtimeFootprint_);
  }

private:
  Configuration config_ = makeConfiguration();
  NetworkExclusions exclusions_;
  asio::io_context discoveryIo_;
  std::shared_ptr<DiscoverySystem> discovery_;
  std::shared_ptr<RuntimeNetworkFootprintState> runtimeFootprint_;
};

}  // namespace

/// @test
/// Clears a pending one-hour timer on stop, so stop completes without throwing and a later
/// cancelTimer with the old timer id returns false.
/// @requirements(SEN-365, SEN-908)
TEST_F(EtherTransportTest, ClearPendingTimers)
{
  auto transport = makeTransport();
  transport->start(nullptr);

  auto timerId = transport->startTimer(std::chrono::hours(1), []() {});

  EXPECT_NO_THROW(transport->stop());
  EXPECT_FALSE(transport->cancelTimer(timerId));
}

/// @test
/// Stops from a callback running in the transport executor without freeing the io_context under
/// asio::io_context::run(). The executor can then return and be joined by final cleanup.
/// @requirements(SEN-365, SEN-908)
TEST_F(EtherTransportTest, StopFromExecutorThreadKeepsSchedulerAlive)
{
  auto transport = makeTransport();
  transport->start(nullptr);

  std::promise<void> stopped;
  auto stoppedFuture = stopped.get_future();
  ASSERT_NE(transport->startTimer(std::chrono::milliseconds(0),
                                  [&transport, &stopped]()
                                  {
                                    transport->stop();
                                    stopped.set_value();
                                  }),
            TimerId {0ULL});

  ASSERT_EQ(stoppedFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);
  EXPECT_NO_THROW(transport->stop());
}

/// @test
/// Waits for a running handler before destroying scheduler-owned timers, while handlers already
/// queued behind it are abandoned after io_context::stop().
/// @requirements(SEN-365, SEN-908)
TEST_F(EtherTransportTest, StopWaitsForRunningHandlerAndDropsQueuedHandlers)
{
  auto transport = makeTransport();
  transport->start(nullptr);

  std::mutex callbackMutex;
  std::condition_variable callbackCondition;
  bool callbackEntered = false;
  bool releaseCallback = false;
  std::atomic<unsigned int> callbacks {0U};

  ASSERT_NE(transport->startTimer(std::chrono::milliseconds(0),
                                  [&]()
                                  {
                                    ++callbacks;
                                    std::unique_lock lock(callbackMutex);
                                    callbackEntered = true;
                                    callbackCondition.notify_one();
                                    callbackCondition.wait(lock, [&]() { return releaseCallback; });
                                  }),
            TimerId {0ULL});

  {
    std::unique_lock lock(callbackMutex);
    if (!callbackCondition.wait_for(lock, std::chrono::seconds(5), [&]() { return callbackEntered; }))
    {
      releaseCallback = true;
      lock.unlock();
      callbackCondition.notify_one();
      transport->stop();
      FAIL() << "transport executor did not invoke the timer callback";
    }
  }

  constexpr unsigned int queuedHandlerCount = 64U;
  for (unsigned int index = 0; index < queuedHandlerCount; ++index)
  {
    ASSERT_NE(transport->startTimer(std::chrono::milliseconds(0), [&callbacks]() { ++callbacks; }), TimerId {0ULL});
  }

  auto stopFuture = std::async(std::launch::async, [&transport]() { transport->stop(); });
  EXPECT_EQ(stopFuture.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);

  {
    std::lock_guard lock(callbackMutex);
    releaseCallback = true;
  }
  callbackCondition.notify_one();

  ASSERT_EQ(stopFuture.wait_for(std::chrono::seconds(5)), std::future_status::ready);
  EXPECT_EQ(callbacks, 1U);
}

/// @test
/// Races timer creation and cancellation with expiry and shutdown. Timer creation either completes
/// before the context is cleared or returns the invalid id after shutdown; it never dereferences a
/// context concurrently being destroyed.
/// @requirements(SEN-365, SEN-908)
TEST_F(EtherTransportTest, TimerOperationsRemainSafeDuringShutdown)
{
  auto transport = makeTransport();
  transport->start(nullptr);

  constexpr unsigned int timerCount = 512U;
  std::atomic<unsigned int> callbacks {0U};
  std::vector<TimerId> timerIds;
  timerIds.reserve(timerCount);
  for (unsigned int index = 0; index < timerCount; ++index)
  {
    timerIds.push_back(transport->startTimer(std::chrono::milliseconds(index % 2U), [&callbacks]() { ++callbacks; }));
  }

  std::thread canceller(
    [&]()
    {
      for (const auto id: timerIds)
      {
        std::ignore = transport->cancelTimer(id);
      }
    });

  std::atomic<bool> stopCreatingTimers {false};
  std::thread creator(
    [&]()
    {
      while (!stopCreatingTimers)
      {
        const auto id = transport->startTimer(std::chrono::hours(1), []() {});
        if (id != TimerId {0ULL})
        {
          std::ignore = transport->cancelTimer(id);
        }
      }
    });

  transport->stop();
  stopCreatingTimers = true;
  canceller.join();
  creator.join();

  EXPECT_EQ(transport->startTimer(std::chrono::hours(1), []() {}), TimerId {0ULL});
  EXPECT_LE(callbacks, timerCount);
}

}  // namespace sen::components::ether
