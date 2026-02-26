// === util.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_KERNEL_UTIL_H
#define SEN_KERNEL_UTIL_H

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// std
#include <cstdint>
#include <string_view>

namespace sen::kernel
{

/// Utility function to fetch our process info.
ProcessInfo getOwnProcessInfo(std::string_view sessionName);

/// Utility function to fetch our host ID.
[[nodiscard]] uint32_t getHostId();

/// Utility function to fetch our process ID.
[[nodiscard]] uint32_t getUniqueSenProcessId();

}  // namespace sen::kernel

#endif  // SEN_KERNEL_UTIL_H
