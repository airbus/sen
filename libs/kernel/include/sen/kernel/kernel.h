// === kernel.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_KERNEL_KERNEL_H
#define SEN_KERNEL_KERNEL_H

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/meta/class_type.h"

// kernel
#include "sen/kernel/kernel_config.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// std
#include <filesystem>
#include <memory>

namespace sen::kernel
{

// Forward declarations
class TestKernel;

/// How to deal with the thread that calls the Kernel::run() function.
enum class KernelBlockMode
{
  doNotBlock,  ///< Does not block the caller thread
  doBlock,     ///< Blocks the caller thread.
};

/// Main entry point of a sen microkernel. \ingroup kernel
class Kernel final
{
  SEN_NOCOPY_NOMOVE(Kernel)

public:
  explicit Kernel(KernelConfig config);
  ~Kernel() noexcept;

public:
  /// Fire up the execution of the kernel.
  /// In case the is already running, -1 is returned.
  /// This method is thread-safe.
  [[nodiscard]] int run(KernelBlockMode blockMode = KernelBlockMode::doBlock);

  /// Stops the execution and deallocates runtime resources.
  /// This method is thread-safe.
  void requestStop(int exitCode = 0);

  /// Gets the configuration used to construct the kernel.
  /// This method is thread-safe.
  [[nodiscard]] const KernelConfig& getConfig() const noexcept;

  /// Gets the path to the configuration file used to construct the kernel.
  /// This method is thread-safe.
  [[nodiscard]] std::filesystem::path getConfigPath() const noexcept;

  /// Gets the kernel build information.
  [[nodiscard]] static const BuildInfo& getBuildInfo() noexcept;

  /// Registers the kernel termination handler.
  static void registerTerminationHandler();

private:
  friend class KernelApi;
  friend class PreloadApi;
  friend class UnloadApi;
  friend class impl::Runner;
  friend class TestKernel;

private:
  std::unique_ptr<impl::KernelImpl> pimpl_;
};

}  // namespace sen::kernel

#endif  // SEN_KERNEL_KERNEL_H
