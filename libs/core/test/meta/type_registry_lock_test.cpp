// === type_registry_lock_test.cpp =====================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// ======================================================================================================================

#include "sen/core/meta/class_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/meta/var.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <optional>
#include <string>
#include <thread>
#include <tuple>

namespace
{

/// Set while a maker is running, so the other thread knows when to try the lock.
std::atomic<bool> makerRunning {false};
/// Set once the other thread has taken and released the registry lock.
std::atomic<bool> lockTaken {false};
/// Whether the lock was free while the maker ran. Read after the fact, lockTaken is always
/// true: the other thread gets the lock as soon as makeInstance returns.
std::atomic<bool> lockWasFreeDuringMaker {false};

/// Stands in for a component constructor, which may take locks of its own. Waits to see whether
/// the registry's lock is free while it runs.
void probeMaker(const std::string& /*name*/, const sen::VarMap& /*properties*/, sen::InstanceStorageType& /*out*/)
{
  makerRunning = true;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!lockTaken && std::chrono::steady_clock::now() < deadline)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  lockWasFreeDuringMaker = lockTaken.load();
  // left null on purpose: makeInstance throws afterwards
}

}  // namespace

/// The registry must not hold usageMutex_ while calling a maker -- see
/// CustomTypeRegistry::makeInstance. This cannot hang: the second thread blocks only until
/// makeInstance returns.
TEST(TypeRegistryLock, makerRunsWithoutTheRegistryLockHeld)
{
  sen::CustomTypeRegistry registry;
  registry.add(sen::Int32Type::get());

  const sen::ClassSpec spec {"LockProbe", "LockProbe", "", {}, {}, {}, std::nullopt, {}, false, {}, {}};
  const auto classType = sen::ClassType::make(spec);
  registry.addInstanceMaker(*classType, &probeMaker);

  makerRunning = false;
  lockTaken = false;
  lockWasFreeDuringMaker = false;

  std::thread taker(
    [&registry]()
    {
      while (!makerRunning)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      std::ignore = registry.getAll();  // takes usageMutex_
      lockTaken = true;
    });

  EXPECT_ANY_THROW(std::ignore = registry.makeInstance(classType->asClassType(), "probe", {}));

  taker.join();

  EXPECT_TRUE(lockWasFreeDuringMaker)
    << "the registry lock was still held while the maker ran: another thread holding a lock the "
       "maker wants would deadlock here";
}
