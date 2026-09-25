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
#include "sen/core/base/span.h"
#include "sen/core/meta/class_type.h"

// kernel
#include "sen/kernel/kernel_config.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/sen/kernel/network_footprint.stl.h"

// std
#include <filesystem>
#include <memory>
#include <optional>

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

  /// Builds the process network footprint after preloading the configured components.
  ///
  /// Returns the generated network footprint.
  /// @param suppliedBusAddresses: additional bus addresses to include in the footprint.
  [[nodiscard]] NetworkFootprint getNetworkFootprint(Span<const BusAddress> suppliedBusAddresses);

  /// Gets the configuration used to construct the kernel.
  /// This method is thread-safe.
  [[nodiscard]] const KernelConfig& getConfig() const noexcept;

  /// Gets the path to the configuration file used to construct the kernel.
  /// This method is thread-safe.
  [[nodiscard]] std::filesystem::path getConfigPath() const noexcept;

  /// Gets the kernel build information.
  [[nodiscard]] static const BuildInfo& getBuildInfo() noexcept;

private:
  friend class KernelApi;
  friend class PreloadApi;
  friend class UnloadApi;
  friend class impl::Runner;
  friend class TestKernel;

private:
  std::unique_ptr<impl::KernelImpl> pimpl_;
};

/// Crash reporting, which belongs to the process rather than to any one kernel: a process has one
/// crash reporter however many kernels it builds.
namespace crash
{

/// Turns on crash reporting for this process: a terminate handler for an uncaught exception, and
/// an out-of-process Crashpad handler for a fault. Both write under `reportDirectory`, or under
/// the system temporary directory when it is empty.
///
/// Call it while the process is still single threaded. On POSIX the handler is started by forking,
/// and a fork keeps every lock another thread was holding at that moment, which can leave the
/// handler unable to start. Sen warns if it is armed in a process that already runs more than one
/// thread.
///
/// Returns true when the handler is running. False means a fault will not be dumped, and the
/// reason has been logged. The terminate handler is still in place either way, so an uncaught
/// exception is still reported.
///
/// The handler process outlives this call and stays for the life of the process that armed it. Sen also creates and
/// registers a process-global spdlog logger named "kernel" at trace level when no logger by that
/// name exists, and takes over an existing one of that name.
///
/// On Windows, runHandlerIfRequested() must already have been called from main, or no dump is
/// written and Sen warns as it arms.
///
/// A second call does not move a running handler: the first directory is kept, and Sen warns.
bool arm(const std::filesystem::path& reportDirectory = {});

/// Give main() its own arguments here, before anything else, and leave with the value it returns
/// when it returns one. Sen has no separate handler program, so on Windows the handler is this
/// executable run again, and this call is what recognises that and becomes it. On other platforms
/// it never claims the process, so the call costs nothing and the code stays the same.
[[nodiscard]] std::optional<int> runHandlerIfRequested(int argc, char* argv[]);

}  // namespace crash

}  // namespace sen::kernel

#endif  // SEN_KERNEL_KERNEL_H
