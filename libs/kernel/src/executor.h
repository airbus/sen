// === executor.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_EXECUTOR_H
#define SEN_LIBS_KERNEL_SRC_EXECUTOR_H

#include "./runner.h"
#include "sen/core/base/class_helpers.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace sen::kernel::impl
{

/// Manages the execution of the kernel
class Executor final
{
  SEN_NOCOPY_NOMOVE(Executor)

public:
  explicit Executor(std::vector<std::unique_ptr<Runner>>& runners);

  ~Executor() = default;

public:
  /// The current group
  [[nodiscard]] uint32_t getGroup() const noexcept { return group_.load(); }

  /// Goes through each group, starting all runners.
  /// Throws std::exception on failure.
  void startUp(bool tryToLockPages);

  /// Goes through each group, shutting down all runners.
  /// Throws std::exception on failure.
  void shutDown();

  /// Holds the called thread until the start procedure is finished
  void waitUntilStarted();

private:
  /// The maximum group that has to be reached
  [[nodiscard]] uint32_t computeMaxGroup(uint32_t previous) const noexcept;

  /// Preloads all the runners
  void preloadRunners();

  /// Allows the runners to do final cleanup tasks after unloading.
  void postUnloadCleanupRunners();

  /// Loads all the runners
  void loadRunners(const std::vector<Runner*>& runners);

  /// Initializes all the passed runners
  void initRunners(const std::vector<Runner*>& runners);

  /// Returns true if all runners have been initialized.
  bool initPass(const std::vector<Runner*>& runners);

  /// Runs all the runners
  void runRunners(const std::vector<Runner*>& runners);

  /// Moves all components to their group
  void bringGroupsUp();

private:
  std::vector<std::unique_ptr<Runner>>& runners_;
  std::atomic<uint32_t> group_;
  std::condition_variable startedCondition_;
  std::mutex startedConditionMutex_;
  std::atomic_bool startupFinished_;
  bool memoryLocked_ = false;
};

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_EXECUTOR_H
