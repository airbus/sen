// === thread_impl.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_WIN32_THREAD_IMPL_H
#define SEN_LIBS_KERNEL_SRC_WIN32_THREAD_IMPL_H

#ifdef _WIN32

#  include "./thread.h"
#  include "./win32_api.h"
#  include "sen/core/base/result.h"

namespace sen::kernel
{

/// Controls a posix thread.
class ThreadImpl
{
  SEN_MOVE_ONLY(ThreadImpl)

public:
  /// Creates a instance of this class, validating that the configuration is correct
  /// and there are enough resources for creating the thread.
  ///
  /// @param configuration How to run the thread.
  /// @param api To interact with the OS.
  ThreadImpl(ThreadConfig configuration, std::shared_ptr<Win32API> api) noexcept;

  /// Releases the resources acquired to configure and run the thread.
  ~ThreadImpl() noexcept = default;

public:
  [[nodiscard]] Result<void, ThreadCreateErr> run() noexcept;
  [[nodiscard]] bool join() noexcept;
  [[nodiscard]] bool kill() noexcept;
  [[nodiscard]] bool detach() noexcept;

private:
  /// Sets the thread attribute for priority according to the configuration.
  [[nodiscard]] Result<void, ThreadCreateErr> configurePriority() noexcept;

  /// Sets the priority. The thread must be created, so this function is called
  /// during the execution of 'run' and just after 'pthread_create'.
  [[nodiscard]] Result<void, ThreadCreateErr> configureAffinity() noexcept;

  /// This is what we use in pthread_create. The argument is an instance of this class.
  static DWORD WINAPI threadFunction(LPVOID lpParam);

private:
  ThreadConfig config_;
  HANDLE thread_ {};
  std::shared_ptr<Win32API> api_;
};

}  // namespace sen::kernel

#endif

#endif  // SEN_LIBS_KERNEL_SRC_WIN32_THREAD_IMPL_H
