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

/// Builds a `ProcessInfo` descriptor for the local process on the given session.
/// @param sessionName  Name of the session to associate with the returned info.
/// @return A `ProcessInfo` populated with the local process identity and build metadata.
ProcessInfo getOwnProcessInfo(std::string_view sessionName);

/// Returns a hardware-derived identifier for the local host (typically based on the MAC address).
/// @return A 32-bit host identifier unique within the local network.
[[nodiscard]] uint32_t getHostId();

/// Returns a process-lifetime-unique numeric identifier for this Sen process instance.
/// @return A 32-bit identifier that is unique across all Sen processes on the same host.
[[nodiscard]] uint32_t getUniqueSenProcessId();

}  // namespace sen::kernel

#endif  // SEN_KERNEL_UTIL_H
