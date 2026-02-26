// === participant.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_BUS_PARTICIPANT_H
#define SEN_LIBS_KERNEL_SRC_BUS_PARTICIPANT_H

#include "sen/core/obj/object_filter.h"
#include "sen/kernel/transport.h"

namespace sen::kernel::impl
{

class LocalParticipant;
class RemoteParticipant;

/// Base class for Local and Remote participants.
class Participant
{
public:
  SEN_NOCOPY_NOMOVE(Participant)

public:
  Participant() = default;
  virtual ~Participant() = default;

public:
  [[nodiscard]] virtual ObjectOwnerId getId() const noexcept = 0;
  [[nodiscard]] virtual BusId getBusId() const noexcept = 0;
  [[nodiscard]] virtual ObjectFilterBase& getIncomingInterestManager() noexcept = 0;
};

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_BUS_PARTICIPANT_H
