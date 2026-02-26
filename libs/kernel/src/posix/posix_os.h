// === posix_os.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_POSIX_POSIX_OS_H
#define SEN_LIBS_KERNEL_SRC_POSIX_POSIX_OS_H

// sen
#include "sen/core/base/result.h"

// implementation
#include "../operating_system.h"
#include "./posix_api.h"
#include "./shared_library.h"
#include "./thread_impl.h"
#include "posix/../thread.h"

// generated
#include "stl/sen/kernel/basic_types.stl.h"

// std
#include <list>
#include <string>

namespace sen::kernel
{

/// Implementation of OperatingSystem on Posix.
class PosixOS: public OperatingSystem
{
public:
  /// Takes the api to funnel system calls (mainly for testability).
  explicit PosixOS(std::shared_ptr<PosixAPI> api) noexcept;

public:  // error handling
  [[nodiscard]] void* installDeathHandler() final;

public:  // shared libraries
  [[nodiscard]] SharedLibrary openSharedLibrary(std::string_view path) final;
  void closeSharedLibrary(SharedLibrary& library) noexcept final;
  [[nodiscard]] void* getSymbol(SharedLibrary library, std::string_view symbolName) final;

public:  // threading
  [[nodiscard]] Result<Thread, ThreadCreateErr> createThread(const ThreadConfig& config) noexcept final;
  [[nodiscard]] bool joinThread(Thread thread) noexcept final;
  [[nodiscard]] bool killThread(Thread thread) noexcept final;
  [[nodiscard]] bool detachThread(Thread& thread) noexcept final;

private:
  void destroyThread(Thread& thread) noexcept;

private:
  std::shared_ptr<PosixAPI> api_;
  std::list<ThreadImpl> threads_;
};

}  // namespace sen::kernel

#endif  // SEN_LIBS_KERNEL_SRC_POSIX_POSIX_OS_H
