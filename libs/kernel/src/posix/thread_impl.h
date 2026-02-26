// === thread_impl.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_POSIX_THREAD_IMPL_H
#define SEN_LIBS_KERNEL_SRC_POSIX_THREAD_IMPL_H

#include "./../thread.h"
#include "./posix_api.h"
#include "posix/thread_impl.h"
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/result.h"
#include "stl/sen/kernel/basic_types.stl.h"

#include <pthread.h>

namespace sen::kernel
{

/// Controls a posix thread.
class ThreadImpl
{
  SEN_COPY_MOVE(ThreadImpl)

public:
  /// Creates a instance of this class, validating that the configuration is correct
  /// and there are enough resources for creating the thread. This function does not
  /// start the execution of the thread, which is postponed until the scheduler is
  /// started.
  ///
  /// @param configuration How to run the thread.
  /// @param barrier The barrier on which the thread will wait until the scheduler starts.
  /// @param api To interact with the OS.
  /// @return The thread implementation or an specific error.
  [[nodiscard]] static Result<ThreadImpl, ThreadCreateErr> make(const ThreadConfig& configuration,
                                                                std::shared_ptr<PosixAPI> api) noexcept;

  /// Releases the resources acquired to configure and run the thread.
  ~ThreadImpl() noexcept = default;

public:
  /// Calls pthread_create and sets the affinity (if any).
  [[nodiscard]] Result<void, ThreadCreateErr> run() noexcept;
  [[nodiscard]] bool join() const noexcept;
  [[nodiscard]] bool kill() const noexcept;
  [[nodiscard]] bool detach() const noexcept;

private:
  /// Sets the thread attribute for priority according to the configuration.
  [[nodiscard]] Result<void, ThreadCreateErr> configurePriority() noexcept;

  /// Sets the stack size attribute for priority according to the configuration.
  [[nodiscard]] Result<void, ThreadCreateErr> configureStackSize() noexcept;

  /// Sets the priority. The thread must be created, so this function is called
  /// during the execution of 'run' and just after 'pthread_create'.
  [[nodiscard]] Result<void, ThreadCreateErr> configureAffinity() const noexcept;

  /// This is what we use in pthread_create. The argument is an instance of this class.
  static void* threadFunction(void* arg);

  /// Constructor is private to avoid direct (unprotected) instantiation. Just stores the arguments.
  ThreadImpl(ThreadConfig configuration, std::shared_ptr<PosixAPI> api) noexcept;

private:
  ThreadConfig config_;
  pthread_attr_t attributes_ {};
  pthread_t thread_ {};
  std::shared_ptr<PosixAPI> api_;
};

}  // namespace sen::kernel

#endif  // SEN_LIBS_KERNEL_SRC_POSIX_THREAD_IMPL_H
