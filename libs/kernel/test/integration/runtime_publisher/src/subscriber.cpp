// === subscriber.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/component.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_list.h"
#include "sen/kernel/component_api.h"

// generated code
#include "rpr/rpr-base_v2.0.xml.h"
#include "rpr/rpr-physical_v2.0.xml.h"

// std
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <set>
#include <string>

namespace
{

/// How long to wait, in cycles: eight seconds, within runner.py's budget.
constexpr uint32_t cyclesAllowed = 80U;

/// How many of the publisher's changes must arrive for the test to pass.
constexpr std::size_t changesWanted = 3U;

/// Whether the entity type is the one the publisher sets (publisher.cpp, entityType).
[[nodiscard]] bool isThePublishersEntityType(const rpr::EntityTypeStruct& value)
{
  return value.entityKind == 1U && value.domain == 2U && value.countryCode == 198U && value.category == 11U &&
         value.subcategory == 3U && value.specific == 7U && value.extra == 9U;
}

}  // namespace

/// Reads the other process's GenericLocalObject through dummy_fom's generated proxy. Passes (exit 0)
/// when enough changes arrive with tentDeployed matching the count's parity and the entity type.
struct GeneratedSubscriber: public sen::kernel::Component
{
  sen::kernel::FuncResult run(sen::kernel::RunApi& api) override
  {
    // the generated class, from the generated code this component links against
    api.getTypes().add(sen::MetaTypeTrait<rpr::AircraftInterface>::meta());

    auto bus = api.getSource("session.bus");
    sen::ObjectList<rpr::AircraftInterface> aircraft;
    bus->addSubscriber(sen::Interest::make("SELECT rpr.Aircraft FROM session.bus", api.getTypes()), &aircraft, true);

    std::set<int16_t> seen;
    uint32_t cycles = 0U;
    bool decided = false;
    auto decide = [&api, &decided](int code, const std::string& message)
    {
      std::cout << message << "\n";
      decided = true;
      api.requestKernelStop(code);
    };

    auto result = api.execLoop(
      sen::Duration::fromHertz(10.0),
      [&]()
      {
        // the loop can run another cycle after the stop is requested
        if (decided)
        {
          return;
        }

        for (const auto* object: aircraft.getObjects())
        {
          if (object->asObject().getName() != "runtime_aircraft")
          {
            continue;
          }

          const auto index = object->getAcousticSignatureIndex();
          if (index <= 0)
          {
            continue;  // no change has come through yet
          }

          if (object->getTentDeployed() != (index % 2 == 1))
          {
            decide(1,
                   "FAILED: acousticSignatureIndex reads " + std::to_string(index) +
                     ", and tentDeployed does not read as set with it");
            return;
          }

          if (!isThePublishersEntityType(object->getEntityType()))
          {
            decide(1, "FAILED: the entity type does not read as the publisher set it");
            return;
          }

          seen.insert(index);
          if (seen.size() == changesWanted)
          {
            decide(0,
                   "OK: runtime_aircraft, a GenericLocalObject of the class imported at runtime, came through "
                   "the generated proxy, with " +
                     std::to_string(changesWanted) + " changes read as set");
            return;
          }
        }

        if (++cycles == cyclesAllowed)
        {
          decide(1,
                 seen.empty() ? "FAILED: no change of runtime_aircraft came through the generated proxy"
                              : "FAILED: only " + std::to_string(seen.size()) + " changes came through");
        }
      });

    bus->removeSubscriber(&aircraft, false);
    return result;
  }
};

SEN_COMPONENT(GeneratedSubscriber)
