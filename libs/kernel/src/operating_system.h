// === operating_system.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_OPERATING_SYSTEM_H
#define SEN_LIBS_KERNEL_SRC_OPERATING_SYSTEM_H

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/result.h"

// implementation
#include "shared_library.h"
#include "thread.h"

namespace sen::kernel
{

/// Represents the underlying operating system and provides methods
/// for performing basic actions and fetching information.
class OperatingSystem
{
  SEN_NOCOPY_NOMOVE(OperatingSystem)

public:  // defaults
  OperatingSystem() noexcept = default;
  virtual ~OperatingSystem() noexcept = default;

public:  // error handling
  /// Installs a process-wide signal handler.
  /// This is an optional method. Implementations might ignore it.
  [[nodiscard]] virtual void* installDeathHandler();

public:  // shared libraries
  /// Opens a shared library.
  /// Throws std::exception on failure.
  [[nodiscard]] virtual SharedLibrary openSharedLibrary(std::string_view path) = 0;

  /// Closes a shared library.
  /// Does nothing if the library is invalid.
  /// Throws std::exception on failure.
  virtual void closeSharedLibrary(SharedLibrary& library) noexcept = 0;

  /// Retrieve exported symbol pointer.
  /// @return  a valid pointer on success, or nullptr otherwise.
  [[nodiscard]] virtual void* getSymbol(SharedLibrary library, std::string_view symbolName) = 0;

public:  // threading
  /// Creates a thread with a given configuration.
  /// @param config The thread configuration.
  /// @return A valid thread handle, or a detailed error.
  [[nodiscard]] virtual Result<Thread, ThreadCreateErr> createThread(const ThreadConfig& config) = 0;

  /// Blocks the caller until the given thread is finished. It is the
  /// caller's responsibility to ensure that the thread can be joined.
  ///
  /// @return true  - if join is successful.
  ///         false - in case of error or an invalid thread.
  [[nodiscard]] virtual bool joinThread(Thread thread) noexcept = 0;

  /// Immediately stops the thread execution.
  ///
  /// @return true  - if the kill operation was successful.
  ///         false - in case of error or an invalid thread.
  [[nodiscard]] virtual bool killThread(Thread thread) noexcept = 0;

  /// Dettaches the thread and invalidates the current object
  ///
  /// @return true  - if the kill operation was successful.
  ///         false - in case of error or an invalid thread.
  [[nodiscard]] virtual bool detachThread(Thread& thread) noexcept = 0;
};

/// The native operating system.
std::shared_ptr<OperatingSystem> makeNativeOS();

inline void* OperatingSystem::installDeathHandler() { return nullptr; }

}  // namespace sen::kernel

#endif  // SEN_LIBS_KERNEL_SRC_OPERATING_SYSTEM_H
