// === win32_os.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_WIN32_WIN32_OS_H
#define SEN_LIBS_KERNEL_SRC_WIN32_WIN32_OS_H

#ifdef _WIN32

#  include "../operating_system.h"
#  include "./thread_impl.h"
#  include "./win32_api.h"

#  include <list>

namespace sen::kernel
{

/// Implementation of OperatingSystem on Win32.
class Win32OS: public OperatingSystem
{
public:
  /// Takes the api to funnel system calls (mainly for testability).
  explicit Win32OS(std::shared_ptr<Win32API> api) noexcept;

public:
  void closeSharedLibrary(SharedLibrary& library) noexcept final;
  [[nodiscard]] SharedLibrary openSharedLibrary(std::string_view path) final;
  [[nodiscard]] void* getSymbol(SharedLibrary library, std::string_view symbolName) final;
  [[nodiscard]] Result<Thread, ThreadCreateErr> createThread(const ThreadConfig& config) final;
  [[nodiscard]] bool joinThread(Thread thread) noexcept final;
  [[nodiscard]] bool killThread(Thread thread) noexcept final;
  [[nodiscard]] bool detachThread(Thread& thread) noexcept final;

private:
  void destroyThread(Thread& thread) noexcept;

private:
  std::shared_ptr<Win32API> api_;
  std::list<std::unique_ptr<ThreadImpl>> threads_;
};

}  // namespace sen::kernel

#endif

#endif  // SEN_LIBS_KERNEL_SRC_WIN32_WIN32_OS_H
