// === kernel_fwd.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_KERNEL_DETAIL_KERNEL_FDW_H
#define SEN_KERNEL_DETAIL_KERNEL_FDW_H

#include "sen/core/meta/type_registry.h"
#include "sen/core/obj/native_object.h"

namespace sen::kernel
{

class Connection;
class Component;
class Kernel;

namespace impl
{

class LocalParticipant;
class SessionManager;
class Runner;
class KernelImpl;

//--------------------------------------------------------------------------------------------------------------
// functions that provide access to implementation details
// without exposing the internal interfaces
//--------------------------------------------------------------------------------------------------------------

CustomTypeRegistry& getTypes(KernelImpl* kernel) noexcept;

void drainInputs(Runner* runner);

void update(Runner* runner);

void commit(Runner* runner);

}  // namespace impl

}  // namespace sen::kernel

#endif  // SEN_KERNEL_DETAIL_KERNEL_FDW_H
