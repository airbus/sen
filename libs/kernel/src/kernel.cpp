// === kernel.cpp ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/kernel.h"

// implementation
#include "crash_reporter.h"
#include "kernel_impl.h"
#include "operating_system.h"

// sen
#include "sen/kernel/component.h"
#include "sen/kernel/kernel_config.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// std
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace sen::kernel
{

Kernel::Kernel(KernelConfig config)
  : pimpl_(std::make_unique<impl::KernelImpl>(makeNativeOS(), *this, std::move(config)))
{
}

Kernel::~Kernel() noexcept = default;

const KernelConfig& Kernel::getConfig() const noexcept { return pimpl_->getConfig(); }

std::filesystem::path Kernel::getConfigPath() const noexcept { return pimpl_->getConfigFilePath(); }

int Kernel::run(KernelBlockMode blockMode) { return pimpl_->run(blockMode); }

void Kernel::requestStop(int exitCode) { pimpl_->requestStop(exitCode); }

const BuildInfo& Kernel::getBuildInfo() noexcept
{
  static BuildInfo result = {"Enrique Parodi Spalazzi (enrique.parodi@airbus.com)",
                             SEN_KERNEL_VERSION,
                             SEN_COMPILER_STRING,
                             getDebugEnabled(),
                             std::string(__DATE__) + " " + __TIME__,
                             getWordSize(),
                             getGitRef(),
                             getGitHash(),
                             getGitStatus()};

  return result;
}

void Kernel::registerTerminationHandler()
{
  impl::CrashReporter::get().install(impl::KernelImpl::getKernelLogger().get());
}

}  // namespace sen::kernel
