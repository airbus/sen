// === component_failure_test.cpp ======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/result.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/kernel.h"
#include "sen/kernel/kernel_config.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
#include <exception>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace
{

/// Where the component gives up, and how. The kernel's answer depends on the kind of failure
/// rather than the place, and the tests below cover both kinds.
enum class Where
{
  nowhere,
  preload,
  load,
  init,
  run,
  unload
};

enum class How
{
  throwing,
  returningError
};

/// Written out here instead of read from the kernel. A test that takes the value from the code it
/// checks agrees with whatever that value becomes.
constexpr int componentFailureExitCode = 70;
constexpr const char* failingComponentName = "failing";

class FailingComponent: public sen::kernel::Component
{
  SEN_NOCOPY_NOMOVE(FailingComponent)

public:
  FailingComponent(Where where, How how): where_ {where}, how_ {how} {}
  ~FailingComponent() override = default;

  sen::kernel::FuncResult preload(sen::kernel::PreloadApi&& /*api*/) override
  {
    return where_ == Where::preload ? giveUp("preload") : sen::Ok();
  }

  sen::kernel::FuncResult load(sen::kernel::LoadApi&& /*api*/) override
  {
    return where_ == Where::load ? giveUp("load") : sen::Ok();
  }

  sen::kernel::PassResult init(sen::kernel::InitApi&& /*api*/) override
  {
    if (where_ != Where::init)
    {
      return sen::kernel::done();
    }
    if (how_ == How::throwing)
    {
      throw std::runtime_error("deliberate failure in init");
    }
    return sen::Err(sen::kernel::ExecError {sen::kernel::ErrorCategory::runtimeError, "deliberate failure in init"});
  }

  sen::kernel::FuncResult run(sen::kernel::RunApi& api) override
  {
    if (where_ == Where::run)
    {
      return giveUp("run");
    }

    // The unload cases need a shutdown to reach unload, and zero so a non-zero code there can only
    // come from the failure. The call sits inside the loop because a stop requested while the
    // kernel is still coming up is dropped, which would leave this waiting forever. See B-165.
    return api.execLoop(std::chrono::milliseconds(1), [&api] { api.requestKernelStop(0); });
  }

  sen::kernel::FuncResult unload(sen::kernel::UnloadApi&& /*api*/) override
  {
    return where_ == Where::unload ? giveUp("unload") : sen::Ok();
  }

private:
  sen::kernel::FuncResult giveUp(const char* what)
  {
    const std::string reason = std::string("deliberate failure in ") + what;
    if (how_ == How::throwing)
    {
      throw std::runtime_error(reason);
    }
    return sen::Err(sen::kernel::ExecError {sen::kernel::ErrorCategory::runtimeError, reason});
  }

  Where where_;
  How how_;
};

/// Runs a kernel holding one failing component and returns what run() returned. A startup failure
/// leaves by throwing, so the callers that expect one catch it instead.
int runToCompletion(Where where, How how)
{
  FailingComponent component {where, how};

  sen::kernel::KernelConfig::ComponentToLoad toLoad;
  toLoad.config.group = 2;
  toLoad.component.instance = &component;
  toLoad.component.config.group = 2;
  toLoad.component.info.name = failingComponentName;

  sen::kernel::KernelConfig config;
  config.addToLoad(toLoad);

  // The bus is named here because the kernel's own component passes it straight to getSource,
  // which throws on an empty address. Left unset, every assertion below would be reading that
  // throw instead.
  auto params = config.getParams();
  params.bus = "local.kernel";
  config.setParams(params);

  sen::kernel::Kernel kernel(config);
  return kernel.run();
}

/// The message of whatever escaped the kernel, or an empty string if nothing did.
std::string whatEscaped(Where where, How how)
{
  try
  {
    std::ignore = runToCompletion(where, how);
  }
  catch (const std::exception& err)
  {
    return err.what();
  }
  return {};
}

bool mentions(const std::string& message, const std::string& fragment)
{
  return message.find(fragment) != std::string::npos;
}

}  // namespace

// A failing component is named whichever entry point it failed in. What follows depends on the
// kind of failure. A reported error brings the kernel down with a non-zero exit code. A throw is
// left in flight, so the process ends on it and the crash reporter can record it.

/// @test
/// A startup hook that reports an error leaves the kernel with the component and the operation
/// named. The sentence is Executor's.
/// @requirements(SEN-362)
TEST(ComponentFailure, AStartupErrorNamesTheComponentAndTheOperation)
{
  for (const auto& [where, operation]: {std::pair {Where::preload, "preloading"},
                                        std::pair {Where::load, "loading"},
                                        std::pair {Where::init, "initializing"}})
  {
    const std::string message = whatEscaped(where, How::returningError);

    EXPECT_FALSE(message.empty()) << "a startup error should leave the kernel, at " << operation;
    EXPECT_TRUE(mentions(message, failingComponentName)) << "should name the component: " << message;
    EXPECT_TRUE(mentions(message, operation)) << "should name the operation: " << message;
    EXPECT_TRUE(mentions(message, "deliberate failure")) << "should carry the reason: " << message;
  }
}

/// @test
/// A startup hook that throws leaves the kernel carrying its own exception, unconverted, so a
/// caller catching by type still sees what it threw. The naming goes to the log instead.
/// @requirements(SEN-362)
TEST(ComponentFailure, AStartupThrowReachesTheCallerUnchanged)
{
  for (const auto where: {Where::preload, Where::load, Where::init})
  {
    const std::string message = whatEscaped(where, How::throwing);

    EXPECT_FALSE(message.empty()) << "a startup throw should leave the kernel";
    EXPECT_TRUE(mentions(message, "deliberate failure")) << "should be the component's own: " << message;
  }
}

/// @test
/// An error reported while the component is running stops the kernel and sets the process exit
/// code, rather than aborting.
/// @requirements(SEN-362)
TEST(ComponentFailure, AnErrorWhileRunningStopsTheKernelWithANonZeroExitCode)
{
  EXPECT_EQ(runToCompletion(Where::run, How::returningError), componentFailureExitCode);
}

// A throw out of `run` ends the process with the exception in flight. This suite does not assert
// that. It needs a second process, and a death test forks one out of a binary holding live kernel
// threads, which segfaults the suite a test or two later. `test/integration/crash_report` covers
// it from outside.

/// @test
/// A throw while unloading is caught, so the shutdown finishes and the process exits with the code
/// it already had. Unload is the one place a throw is not left in flight, because an exception
/// leaving the shutdown walk abandons the rest of it and the kernel is destroyed on top of that.
/// @requirements(SEN-362)
TEST(ComponentFailure, AThrowWhileUnloadingDoesNotAbortTheProcess)
{
  EXPECT_EQ(runToCompletion(Where::unload, How::throwing), 0);
}

/// @test
/// An error reported while unloading does not abort the process. A late error cannot change an exit
/// code that whoever asked for the shutdown has already settled, so this asks only that the run
/// completed and returned it.
/// @requirements(SEN-362)
TEST(ComponentFailure, AnErrorWhileUnloadingDoesNotAbortTheProcess)
{
  EXPECT_EQ(runToCompletion(Where::unload, How::returningError), 0);
}

/// @test
/// A component that does not fail runs, asks the kernel to stop, and the process exits zero. This
/// Without it the suite would read the same against a kernel that refused to start anything.
/// @requirements(SEN-362)
TEST(ComponentFailure, AHealthyComponentRunsAndStopsCleanly)
{
  EXPECT_EQ(runToCompletion(Where::nowhere, How::throwing), 0);
  EXPECT_EQ(runToCompletion(Where::nowhere, How::returningError), 0);
}
