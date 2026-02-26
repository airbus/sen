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

/// Convenience helper for doing sen::Ok(sen::kernel::OpState {sen::kernel::OpFinished {}})
[[nodiscard]] inline PassResult done() { return Ok(OpState {OpFinished {}}); }

/// Base class for implementing sen kernel components.
///
/// Users shall define the SEN_COMPONENT(name) macro *once*
/// somewhere in their shared lib (in the global namespace)
/// where 'name' shall be the name of the subclass.
/// \ingroup kernel
class Component
{
  SEN_NOCOPY_NOMOVE(Component)

public:  // special members
  Component() noexcept = default;
  virtual ~Component() = default;

public:
  /// Preload the component and initialize all self-contained resources.
  /// This function is just called once.
  virtual FuncResult preload(PreloadApi&& /* api */) { return Ok(); }

  /// Load the component and initialize all self-contained resources.
  /// This function is just called once.
  virtual FuncResult load(LoadApi&& /* api */) { return Ok(); }

  /// Initialize the component and perform kernel-related operations.
  /// This may include dependency resolution, and resource registration.
  /// This function will be continuously called until it returns that it's done.
  virtual PassResult init(InitApi&& /* api */) { return done(); }

  /// Runs the component in a dedicated thread.
  /// This function is called once and only when the kernel
  /// has reached the desired group of this component.
  /// Implementations shall end the execution thread when
  /// the stopFlag is set to true.
  virtual FuncResult run(RunApi& /* api */) { return Ok(); }

  /// Unload any self-contained resources.
  /// This function is just called once.
  virtual FuncResult unload(UnloadApi&& /* api */) { return Ok(); }

  /// Do an additional (and final) clean up step after unloading the component.
  /// This function is just called once for all components and independently of the chosen group.
  virtual void postUnloadCleanup() {}

  /// Return true here if your component is not designed to work with virtualized time.
  [[nodiscard]] virtual bool isRealTimeOnly() const noexcept { return false; }

protected:  // helpers
  /// Convenience function to return an operation delay request.
  [[nodiscard]] static PassResult delay(Duration time) noexcept { return Ok(OpState {OpNotFinished {time}}); }

  /// Convenience function to return a finished operation result.
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

/// Use this macro to make your library loadable.
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
