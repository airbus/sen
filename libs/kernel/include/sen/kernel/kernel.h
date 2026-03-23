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

/// Controls whether `Kernel::run()` blocks the calling thread until the kernel stops.
enum class KernelBlockMode
{
  doNotBlock,  ///< Returns immediately; the kernel runs on background threads.
  doBlock,     ///< Blocks the calling thread until `requestStop()` is called and all components finish.
};

/// The central orchestrator of a Sen application.
///
/// A `Kernel` loads components from shared libraries or in-memory instances (as configured by
/// `KernelConfig`), manages their lifecycle (`preload` â†’ `load` â†’ `init` â†’ `run` â†’ `unload`),
/// and coordinates the underlying transport layer.
///
/// Typical usage:
/// @code{.cpp}
/// auto config = sen::kernel::KernelConfig::fromFile("config.yaml");
/// sen::kernel::Kernel kernel(std::move(config));
/// kernel.registerTerminationHandler();       // handle Ctrl+C / SIGTERM
/// auto exitCode = kernel.run();              // blocks until stopped
/// @endcode
/// \ingroup kernel
class Kernel final
{
  SEN_NOCOPY_NOMOVE(Kernel)

public:
  /// Constructs the kernel with the given configuration.
  /// No components are loaded until `run()` is called.
  /// @param config Fully populated `KernelConfig` describing which components to load.
  explicit Kernel(KernelConfig config);
  ~Kernel() noexcept;

public:
  /// Starts the kernel, loading and running all configured components.
  /// Returns immediately if `blockMode` is `doNotBlock`, otherwise blocks until shutdown.
  /// If the kernel is already running, returns `-1` immediately.
  /// Thread-safe.
  /// @param blockMode Whether to block the calling thread until the kernel stops.
  /// @return Process exit code passed to `requestStop()`, or `-1` if already running.
  [[nodiscard]] int run(KernelBlockMode blockMode = KernelBlockMode::doBlock);

  /// Schedules a graceful shutdown of the kernel.
  /// All components will be given a chance to finish their current cycle before stopping.
  /// Subsequent calls are ignored once a stop has been requested.
  /// Thread-safe.
  /// @param exitCode The exit code that `run()` will return after shutdown completes.
  void requestStop(int exitCode = 0);

  /// Returns the configuration used to construct this kernel instance.
  /// Thread-safe.
  /// @return Const reference to the `KernelConfig`; valid for the lifetime of the kernel.
  [[nodiscard]] const KernelConfig& getConfig() const noexcept;

  /// Returns the filesystem path of the YAML configuration file, if one was used.
  /// Thread-safe.
  /// @return The config file path, or an empty path if the kernel was configured programmatically.
  [[nodiscard]] std::filesystem::path getConfigPath() const noexcept;

  /// Returns static build metadata for this kernel binary (version, compiler, Git hash, etc.).
  /// @return Const reference to the `BuildInfo` with process-wide lifetime.
  [[nodiscard]] static const BuildInfo& getBuildInfo() noexcept;

  /// Installs a signal handler for `SIGTERM`/`SIGINT` that calls `requestStop()`.
  /// Call this once before `run()` if you want Ctrl+C to trigger a clean shutdown.
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
