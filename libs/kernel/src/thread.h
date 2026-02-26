// === thread.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_THREAD_H
#define SEN_LIBS_KERNEL_SRC_THREAD_H

#include "stl/sen/kernel/basic_types.stl.h"

#include <string>
#include <type_traits>

namespace sen::kernel
{

/// We use an old-fashioned function pointer for easier integration with the OS.
using ThreadFunction = std::add_pointer_t<void(void*)>;

/// Up to 64 cores.
using ThreadAffinityMask = std::uint64_t;

/// Parameters that define how a thread should run.
struct ThreadConfig
{
  std::string name;                   ///< Human-readable name, mainly for introspection.
  ThreadAffinityMask affinity = 0U;   ///< 0 means: use the default.
  std::size_t stackSize = 0U;         ///< 0 means use: default
  ThreadFunction function = nullptr;  ///< function to be called by the thread.
  void* arg = nullptr;                ///< Argument that the function will take.
  Priority priority = Priority::nominalMin;
};

/// Opaque container of an implementation-defined handle.
struct Thread
{
  void* nativeHandle = nullptr;  ///< A nullptr handle indicates an invalid thread.
};

}  // namespace sen::kernel

#endif  // SEN_LIBS_KERNEL_SRC_THREAD_H
