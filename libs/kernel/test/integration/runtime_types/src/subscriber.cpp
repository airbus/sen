// === subscriber.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/component.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/result.h"
#include "sen/core/lang/fom_parser.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_list.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/detail/generic_remote_object.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// std
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

using sen::kernel::ErrorCategory;

namespace
{

/// How long to wait for an aircraft, in cycles: eight seconds, within runner.py's budget.
constexpr uint32_t cyclesAllowed = 80U;

/// The entity type DummyAircraft sets on every aircraft (dummy_entities/src/util.cpp).
constexpr std::array<std::pair<std::string_view, uint32_t>, 7U> dummyEntityType {{{"entityKind", 0U},
                                                                                  {"domain", 1U},
                                                                                  {"countryCode", 2U},
                                                                                  {"category", 3U},
                                                                                  {"subcategory", 4U},
                                                                                  {"specific", 5U},
                                                                                  {"extra", 6U}}};

/// Whether value is the entity type DummyAircraft sends.
[[nodiscard]] bool isTheDummyEntityType(const sen::Var& value)
{
  if (!value.holds<sen::VarMap>())
  {
    return false;
  }

  const auto fields = sen::getCopyAs<sen::VarMap>(value);
  return std::all_of(dummyEntityType.begin(),
                     dummyEntityType.end(),
                     [&fields](const auto& expected)
                     {
                       const auto itr = fields.find(expected.first);
                       return itr != fields.end() && sen::getCopyAs<uint32_t>(itr->second) == expected.second;
                     });
}

}  // namespace

/// Subscribes to rpr.Aircraft from the RPR FOM imported at runtime, without generated code. Passes
/// (exit 0) when the publisher's aircraft arrives as a GenericRemoteObject with its entity type.
struct RuntimeFomSubscriber: public sen::kernel::Component
{
  sen::kernel::FuncResult run(sen::kernel::RunApi& api) override
  {
    const auto& config = api.getConfig();
    const auto fomDirectory = sen::findElementAs<std::string>(config, "fomDirectory", "no fomDirectory configured");
    const auto mappingsFile = sen::findElementAs<std::string>(config, "mappingsFile", "no mappingsFile configured");

    // the inputs the generated package is made from, so names and layouts match
    sen::CustomTypeRegistry types;
    const auto typeSets = sen::lang::parseFomDocuments({fomDirectory}, {}, {mappingsFile}, {});
    for (const auto& typeSet: typeSets)
    {
      for (const auto& type: typeSet.types)
      {
        types.add(type);
      }
    }

    const auto aircraftClass = types.get("rpr.Aircraft");
    if (!aircraftClass)
    {
      return sen::Err(sen::kernel::ExecError {ErrorCategory::runtimeError, "rpr.Aircraft was not imported"});
    }

    const auto* entityType = aircraftClass.value()->asClassType()->searchPropertyByName("entityType");
    if (entityType == nullptr)
    {
      return sen::Err(sen::kernel::ExecError {ErrorCategory::runtimeError, "rpr.Aircraft has no entityType"});
    }

    auto bus = api.getSource("session.bus");
    sen::ObjectList<sen::Object> aircraft;
    bus->addSubscriber(sen::Interest::make("SELECT rpr.Aircraft FROM session.bus", types), &aircraft, true);

    uint32_t cycles = 0U;
    bool arrivedUnread = false;
    bool decided = false;
    auto result = api.execLoop(
      sen::Duration::fromHertz(10.0),
      [&]()
      {
        // the loop can run another cycle after the stop is requested
        if (decided)
        {
          return;
        }

        for (const auto& object: aircraft.getUntypedObjects())
        {
          const auto* generic = dynamic_cast<const sen::kernel::impl::GenericRemoteObject*>(object.get());
          if (object->getClass().type() != aircraftClass->type() || generic == nullptr)
          {
            continue;
          }

          // the values must arrive too, not only the object
          if (!isTheDummyEntityType(object->getPropertyUntyped(entityType)))
          {
            arrivedUnread = true;
            continue;
          }

          const auto writer = generic->getWriterSchema();
          std::cout << "OK: " << object->getName() << " arrived as the imported rpr.Aircraft, hash "
                    << aircraftClass.value()->getHash().get() << ", and its entityType reads as sent. "
                    << (writer ? "The publisher's class has hash " + std::to_string(writer.value()->getHash().get()) +
                                   ", so it was read through the writer's schema.\n"
                               : std::string("The publisher's class has the same hash.\n"));
          decided = true;
          api.requestKernelStop(0);
          return;
        }

        if (++cycles == cyclesAllowed)
        {
          std::cout << (arrivedUnread ? "FAILED: an rpr.Aircraft arrived through the fallback, but its entityType did "
                                        "not read as DummyAircraft sends it\n"
                                      : "FAILED: no rpr.Aircraft of the imported class arrived through the fallback\n");
          decided = true;
          api.requestKernelStop(1);
        }
      });

    bus->removeSubscriber(&aircraft, false);
    return result;
  }
};

SEN_COMPONENT(RuntimeFomSubscriber)
