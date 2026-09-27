// === logger_relay.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_LOGGER_RELAY_H
#define SEN_LIBS_KERNEL_SRC_LOGGER_RELAY_H

namespace sen::kernel::impl
{

/// Attach the kernel's relay sink to every logger that already exists.
///
/// Called once, from `KernelImpl::configure`, after `configureSpdlog` has replaced the loggers' sinks and
/// before any component thread exists, which is what makes appending to a sink vector safe here and nowhere
/// else. Kept out of the public component header: a component has no use for it, and having it there would
/// invite a component to call it on its own thread, which is the hazard the relay removes.
void installLoggerRelay();

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_LOGGER_RELAY_H
