// === component.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_KERNEL_COMPONENT_H
#define SEN_KERNEL_COMPONENT_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/result.h"
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/object_ref.h"

// kernel
#include "sen/kernel/component_api.h"
#include "sen/kernel/kernel.h"

namespace sen::kernel
{

/// Convenience helper that returns a `PassResult` indicating the operation has finished.
/// Equivalent to `sen::Ok(sen::kernel::OpState{sen::kernel::OpFinished{}})`.
/// @return A `PassResult` signalling completion.
[[nodiscard]] inline PassResult done() { return Ok(OpState {OpFinished {}}); }

/// Abstract base class for all Sen kernel components.
///
/// A component is a dynamically loaded (or statically registered) unit of behaviour
/// that participates in the kernel's structured lifecycle:
///
/// 1. **preload** â€” Install transports or tracers; no kernel-aware objects yet.
/// 2. **load**    â€” Allocate self-contained resources that do not touch the kernel.
/// 3. **init**    â€” Register objects on buses; called repeatedly until `done()` is returned.
/// 4. **run**     â€” Execute the main loop on a dedicated thread; exit when `stopRequested()`.
/// 5. **unload**  â€” Release kernel-level resources.
/// 6. **postUnloadCleanup** â€” Final teardown pass, called for every component.
///
/// To make a shared library loadable by the kernel, place the `SEN_COMPONENT(YourClass)` macro
/// once in a `.cpp` file in the global namespace. The macro exports the factory functions the
/// kernel uses to instantiate and inspect the component at runtime.
///
/// \ingroup kernel
class Component
{
  SEN_NOCOPY_NOMOVE(Component)

public:  // special members
  Component() noexcept = default;
  virtual ~Component() = default;

public:
  /// Called once before `load()` to install transports or tracers.
  /// @param api Provides access to kernel services and component configuration.
  /// @return `Ok()` on success, or `Err(ExecError)` to abort the startup sequence.
  virtual FuncResult preload(PreloadApi&& /* api */) { return Ok(); }

  /// Called once to allocate self-contained resources (file handles, threads, etc.).
  /// At this point the kernel type registry is available but no objects are published yet.
  /// @param api Provides access to kernel services and component configuration.
  /// @return `Ok()` on success, or `Err(ExecError)` to abort the startup sequence.
  virtual FuncResult load(LoadApi&& /* api */) { return Ok(); }

  /// Called repeatedly until the component returns `done()`.
  /// Use this phase to register objects on buses and resolve inter-component dependencies.
  /// Return `delay(duration)` to defer re-invocation; return `done()` when ready.
  /// @param api Provides access to kernel services and component configuration.
  /// @return `done()` when initialisation is complete, `delay(t)` to be called again after `t`.
  virtual PassResult init(InitApi&& /* api */) { return done(); }

  /// Main execution entry point; runs on a dedicated kernel thread.
  /// Called exactly once after `init()` completes and the component's group is started.
  /// Implementations must return (or call `Ok()`) when `api.stopRequested()` is set.
  /// @param api Provides access to `drainInputs()`, `commit()`, `execLoop()`, and timing.
  /// @return `Ok()` on clean exit, or `Err(ExecError)` to signal a fatal runtime failure.
  virtual FuncResult run(RunApi& /* api */) { return Ok(); }

  /// Called once after `run()` returns to release kernel-level resources.
  /// @param api Provides access to kernel services and component configuration.
  /// @return `Ok()` on success, or `Err(ExecError)` to signal a cleanup failure.
  virtual FuncResult unload(UnloadApi&& /* api */) { return Ok(); }

  /// Called once for every component after all components have been unloaded.
  /// Use this for final cleanup that must happen regardless of group configuration.
  virtual void postUnloadCleanup() {}

  /// Returns `true` if this component cannot operate with a virtualised (non-wall-clock) time source.
  /// When `true`, the kernel will not allow this component in simulation-mode deployments.
  [[nodiscard]] virtual bool isRealTimeOnly() const noexcept { return false; }

protected:  // helpers
  /// Returns a `PassResult` that asks the kernel to call `init()` again after `time` has elapsed.
  /// @param time How long to wait before the next `init()` invocation.
  /// @return A `PassResult` signalling deferred re-initialisation.
  [[nodiscard]] static PassResult delay(Duration time) noexcept { return Ok(OpState {OpNotFinished {time}}); }

  /// Returns a `PassResult` signalling that `init()` has completed successfully.
  /// @return A `PassResult` signalling completion.
  [[nodiscard]] static PassResult done() noexcept { return kernel::done(); }
};

}  // namespace sen::kernel

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

#define SEN_COMPONENT_MAKER      senMakeKernelComponent      // NOLINT(cppcoreguidelines-macro-usage)
#define SEN_COMPONENT_INFO_MAKER senMakeKernelComponentInfo  // NOLINT(cppcoreguidelines-macro-usage)

namespace sen::kernel
{

constexpr auto* componentMakerFuncName = SEN_STRINGIFY(SEN_COMPONENT_MAKER);
constexpr auto* componentInfoMakerFuncName = SEN_STRINGIFY(SEN_COMPONENT_INFO_MAKER);

//--------------------------------------------------------------------------------------------------------------
// Component
//--------------------------------------------------------------------------------------------------------------

[[nodiscard]] constexpr bool getDebugEnabled() noexcept
{
#if defined(DEBUG) || defined(_DEBUG)
  return true;
#else
  return false;
#endif
}

[[nodiscard]] constexpr WordSize getWordSize() noexcept
{
  if constexpr (sizeof(size_t) == 4U)
  {
    return WordSize::bits32;
  }
  else
  {
    return WordSize::bits64;
  }
}

[[nodiscard]] constexpr const char* getGitRef() noexcept
{
#ifdef GIT_REF_SPEC
  return GIT_REF_SPEC;
#else
  return "";
#endif
}

[[nodiscard]] constexpr const char* getGitHash() noexcept
{
#ifdef GIT_HASH
  return GIT_HASH;
#else
  return "";
#endif
}

[[nodiscard]] inline GitStatus getGitStatus() noexcept
{
#ifdef GIT_STATUS
  const auto* statusStr = GIT_STATUS;
#else
  const auto* statusStr = "";
#endif

  if (std::string(statusStr) == "clean")
  {
    return GitStatus::clean;
  }

  if (std::string(statusStr) == "dirty")
  {
    return GitStatus::modified;
  }

  return GitStatus::unknown;
}

}  // namespace sen::kernel

// declare the functions that will be defined in the shared library
extern "C" SEN_EXPORT sen::kernel::Component* SEN_COMPONENT_MAKER();
extern "C" SEN_EXPORT const sen::kernel::ComponentInfo* SEN_COMPONENT_INFO_MAKER();

/// Exports the factory functions required by the kernel to load this component from a shared library.
/// Place this macro exactly once per shared library, in the global namespace of a `.cpp` file.
/// @param component_name The unqualified name of the `Component` subclass to instantiate.
/// NOLINTNEXTLINE(cppcoreguidelines-macro-usage, bugprone-macro-parentheses)
#define SEN_COMPONENT(component_name)                                                                                  \
                                                                                                                       \
  extern "C" SEN_EXPORT sen::kernel::Component* SEN_COMPONENT_MAKER()                                                  \
  {                                                                                                                    \
    return new component_name(); /* NOLINT(cppcoreguidelines-owning-memory) */                                         \
  }                                                                                                                    \
                                                                                                                       \
  extern "C" SEN_EXPORT const sen::kernel::ComponentInfo* SEN_COMPONENT_INFO_MAKER()                                   \
  {                                                                                                                    \
    static sen::kernel::ComponentInfo info {};                                                                         \
    info.name = SEN_TARGET_NAME;                                                                                       \
    info.description = SEN_TARGET_DESCRIPTION;                                                                         \
    info.buildInfo.maintainer = SEN_TARGET_MAINTAINER;                                                                 \
    info.buildInfo.version = SEN_TARGET_VERSION;                                                                       \
    info.buildInfo.compiler = SEN_COMPILER_STRING;                                                                     \
    info.buildInfo.debugMode = sen::kernel::getDebugEnabled();                                                         \
    info.buildInfo.wordSize = sen::kernel::getWordSize();                                                              \
    info.buildInfo.buildTime = std::string(__DATE__) + " " + __TIME__;                                                 \
    info.buildInfo.gitRef = sen::kernel::getGitRef();                                                                  \
    info.buildInfo.gitHash = sen::kernel::getGitHash();                                                                \
    info.buildInfo.gitStatus = sen::kernel::getGitStatus();                                                            \
    return &info;                                                                                                      \
  }

#endif  // SEN_KERNEL_COMPONENT_H
