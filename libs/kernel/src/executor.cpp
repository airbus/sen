// === executor.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "./executor.h"

// implementation
#include "kernel_impl.h"
#include "runner.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/result.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// spdlog
#include <spdlog/logger.h>

// std
#include <algorithm>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <variant>
#include <vector>

// os
#ifdef __linux__
#  include <sys/mman.h>  // for mlockall()
#endif

namespace sen::kernel::impl
{

namespace
{

std::vector<Runner*> getRunnersAt(std::vector<std::unique_ptr<Runner>>& runners, uint32_t group)
{
  std::vector<Runner*> result;
  result.reserve(runners.size());

  for (const auto& elem: runners)
  {
    if (elem->getGroup() == group)
    {
      result.push_back(elem.get());
    }
  }
  return result;
}

struct InitVisitor
{
  std::tuple<bool, Duration> operator()(const OpFinished& done) const
  {
    std::ignore = done;
    return {true, {}};
  }

  std::tuple<bool, Duration> operator()(const OpNotFinished& notDone) const { return {false, notDone.waitHint}; }
};

}  // namespace

Executor::Executor(std::vector<std::unique_ptr<Runner>>& runners)
  : runners_(runners), group_(0U), startupFinished_(false)
{
}

void Executor::startUp(bool tryToLockPages)
{
  // early exit
  if (runners_.empty())
  {
    return;
  }

  auto logger = KernelImpl::getKernelLogger();

  try
  {
    // try to preload all the runners
    preloadRunners();

    bringGroupsUp();

#ifdef __linux__
    if (tryToLockPages && !memoryLocked_)
    {
      memoryLocked_ = mlockall(MCL_CURRENT | MCL_FUTURE) != -1;
      if (!memoryLocked_)
      {
        logger->warn("could not call mlockall (use setcap cap_ipc_lock,cap_sys_nice=+eip <path_to_cli_run>)");
      }
    }
#else
    std::ignore = tryToLockPages;
#endif

    startupFinished_ = true;
    startedCondition_.notify_all();
  }
  catch (const std::exception& err)
  {
    logger->error(err.what());
    shutDown();  // abort the execution
    throw;
  }
}

void Executor::bringGroupsUp()
{
  std::shared_ptr<spdlog::logger> logger = KernelImpl::getKernelLogger();

  auto maxGroup = computeMaxGroup(0);

  for (uint32_t group = 1U; group <= maxGroup; ++group)  // NOSONAR
  {
    // compute the max group that we have to reach
    maxGroup = computeMaxGroup(maxGroup);

    auto runnersInThisLevel = getRunnersAt(runners_, group);
    if (runnersInThisLevel.empty())
    {
      continue;
    }

    logger->debug("advancing to group {}", group);

    // try to load all the runners at this group
    loadRunners(runnersInThisLevel);

    // try to initialize the runners in this group until all are ready to run
    initRunners(runnersInThisLevel);

    // run all the runners in this group in one go
    runRunners(runnersInThisLevel);

    // we reached this group
    group_ = group;

    logger->debug("reached group {}", group_.load());
  }
}

uint32_t Executor::computeMaxGroup(uint32_t previous) const noexcept
{
  auto maxGroup = previous;

  // compute the max group that we have to reach
  for (const auto& runner: runners_)
  {
    maxGroup = std::max(maxGroup, runner->getGroup());
  }

  return maxGroup;
}

void Executor::shutDown()
{
  auto logger = KernelImpl::getKernelLogger();

  while (true)
  {
    // 0 is the last group, so stop here
    if (group_ == 0U)
    {
      break;
    }

    // bring all the runners of this group down
    auto runnersInThisGroup = getRunnersAt(runners_, group_);

    // stop the execution
    for (auto* runner: runnersInThisGroup)
    {
      // ignore faulted runners
      if (runner->getState() == ComponentState::error)
      {
        continue;
      }

      logger->debug("stopping {}", runner->getComponentContext().info.name);
      runner->stopThread();
    }

    // unload them
    for (auto* runner: runnersInThisGroup)
    {
      // ignore faulted runners
      if (runner->getState() == ComponentState::error)
      {
        continue;
      }

      logger->debug("unloading {}", runner->getComponentContext().info.name);
      runner->unload();
    }

    // continue going down in the group chain
    group_--;
    logger->debug("reached group {}", group_.load());
  }

  for (auto& runner: runners_)
  {
    runner->postUnloadCleanup();
  }

#ifdef __linux__
  if (memoryLocked_)
  {
    munlockall();
  }
#endif
}

void Executor::preloadRunners()  // NOSONAR
{
  auto logger = KernelImpl::getKernelLogger();

  for (auto& runner: runners_)
  {
    // ignore faulted or preloaded runners
    if (auto runnerState = runner->getState();
        runnerState == ComponentState::error || runnerState == ComponentState::preloaded)
    {
      continue;
    }

    logger->debug("preloading {}", runner->getComponentContext().info.name);
    const auto preloadResult = runner->preload();

    if (preloadResult.isError())
    {
      std::string err;
      err.append("error preloading component '");
      err.append(runner->getComponentContext().info.name);
      err.append("': ");
      err.append(preloadResult.getError().explanation);
      throwRuntimeError(err);
    }
  }
}

void Executor::loadRunners(const std::vector<Runner*>& runners)  // NOSONAR
{
  auto logger = KernelImpl::getKernelLogger();

  for (auto* runner: runners)
  {
    // ignore faulted or loaded runners
    if (auto runnerState = runner->getState();
        runnerState == ComponentState::error || runnerState == ComponentState::loaded)
    {
      continue;
    }

    logger->debug("loading {}", runner->getComponentContext().info.name);
    const auto loadResult = runner->load();

    if (loadResult.isError())
    {
      std::string err;
      err.append("error loading component '");
      err.append(runner->getComponentContext().info.name);
      err.append("': ");
      err.append(loadResult.getError().explanation);
      throwRuntimeError(err);
    }
  }
}

void Executor::initRunners(const std::vector<Runner*>& runners)
{
  // try to initialize the runners in this group until all are ready to run
  while (!initPass(runners))
  {
    // no code needed
  }
}

bool Executor::initPass(const std::vector<Runner*>& runners)  // NOSONAR
{
  auto logger = KernelImpl::getKernelLogger();
  bool allInitialized = true;  // start assuming that we are done
  Duration maxSleepTime;

  for (auto* runner: runners)
  {
    // ignore faulted or initialized runners
    if (auto runnerState = runner->getState();
        runnerState == ComponentState::error || runnerState == ComponentState::initialized)
    {
      continue;
    }

    // ask the runner to initialize
    logger->debug("initializing {}", runner->getComponentContext().info.name);
    auto initResult = runner->init();
    if (initResult.isError())
    {
      const auto& initError = initResult.getError();

      std::string msg;
      msg.append("error initializing component '");
      msg.append(runner->getComponentContext().info.name);
      msg.append("': ");
      msg.append(StringConversionTraits<ErrorCategory>::toString(initError.category));
      msg.append(" ");
      msg.append(initError.explanation);

      throwRuntimeError(msg);
    }

    const auto [done, sleepTime] = std::visit(InitVisitor(), initResult.getValue());
    allInitialized = allInitialized && done;
    maxSleepTime = Duration(std::max(maxSleepTime.getNanoseconds(), sleepTime.getNanoseconds()));
  }

  // maybe we need to sleep until the next iteration
  if (!allInitialized && maxSleepTime.getNanoseconds() != 0)
  {
    std::this_thread::sleep_for(maxSleepTime.toChrono());
  }

  return allInitialized;
}

void Executor::runRunners(const std::vector<Runner*>& runners)  // NOSONAR
{
  auto logger = KernelImpl::getKernelLogger();
  for (auto* runner: runners)
  {
    // ignore faulted or running runners
    if (auto runnerState = runner->getState();
        runnerState == ComponentState::error || runnerState == ComponentState::running)
    {
      continue;
    }

    logger->debug("running {}", runner->getComponentContext().info.name);
    auto runResult = runner->run();
    if (runResult.isError())
    {
      const auto& runError = runResult.getError();

      std::string msg;
      msg.append("error running component '");
      msg.append(runner->getComponentContext().info.name);
      msg.append("': ");
      msg.append(StringConversionTraits<ErrorCategory>::toString(runError.category));
      msg.append(" ");
      msg.append(runError.explanation);

      throwRuntimeError(msg);
    }
  }
}

void Executor::waitUntilStarted()
{
  std::unique_lock<std::mutex> lock(startedConditionMutex_);  // NOSONAR
  startedCondition_.wait(lock, [this] { return startupFinished_.load(); });
}

}  // namespace sen::kernel::impl
