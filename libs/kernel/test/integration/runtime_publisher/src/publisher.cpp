// === publisher.cpp ===================================================================================================
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
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/detail/generic_local_object.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// std
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>

using sen::kernel::ErrorCategory;

namespace
{

/// The aircraft's entity type, which the subscriber checks.
sen::Var entityType()
{
  return sen::VarMap {{"entityKind", uint8_t {1U}},
                      {"domain", uint8_t {2U}},
                      {"countryCode", uint16_t {198U}},
                      {"category", uint8_t {11U}},
                      {"subcategory", uint8_t {3U}},
                      {"specific", uint8_t {7U}},
                      {"extra", uint8_t {9U}}};
}

sen::Var entityIdentifier()
{
  return sen::VarMap {{"federateIdentifier", sen::VarMap {{"siteID", uint16_t {1U}}, {"applicationID", uint16_t {2U}}}},
                      {"entityNumber", uint16_t {3U}}};
}

/// Where the acoustic signature index stops counting, within its i16 range.
constexpr int16_t lastIndex = 30000;

}  // namespace

/// Publishes one rpr.Aircraft as a GenericLocalObject of the RPR FOM imported at runtime. Each cycle
/// sets acousticSignatureIndex to a count and tentDeployed to whether it is odd, for the subscriber.
struct RuntimeFomPublisher: public sen::kernel::Component
{
  sen::kernel::FuncResult run(sen::kernel::RunApi& api) override
  {
    const auto& config = api.getConfig();
    const auto fomDirectory = sen::findElementAs<std::string>(config, "fomDirectory", "no fomDirectory configured");
    const auto mappingsFile = sen::findElementAs<std::string>(config, "mappingsFile", "no mappingsFile configured");

    sen::CustomTypeRegistry types;
    const auto typeSets = sen::lang::parseFomDocuments({fomDirectory}, {}, {mappingsFile}, {});
    for (const auto& typeSet: typeSets)
    {
      for (const auto& type: typeSet.types)
      {
        types.add(type);
      }
    }

    const auto registered = types.get("rpr.Aircraft");
    const auto aircraftClass =
      registered ? sen::dynamicTypeHandleCast<const sen::ClassType>(registered.value()) : std::nullopt;
    if (!aircraftClass)
    {
      return sen::Err(sen::kernel::ExecError {ErrorCategory::runtimeError, "rpr.Aircraft was not imported"});
    }

    const auto& type = aircraftClass.value();
    const auto* acousticSignatureIndex = type->searchPropertyByName("acousticSignatureIndex");
    const auto* tentDeployed = type->searchPropertyByName("tentDeployed");
    const auto* entityTypeProperty = type->searchPropertyByName("entityType");
    const auto* alternateEntityType = type->searchPropertyByName("alternateEntityType");
    const auto* entityIdentifierProperty = type->searchPropertyByName("entityIdentifier");
    if (acousticSignatureIndex == nullptr || tentDeployed == nullptr || entityTypeProperty == nullptr ||
        alternateEntityType == nullptr || entityIdentifierProperty == nullptr)
    {
      return sen::Err(
        sen::kernel::ExecError {ErrorCategory::runtimeError, "rpr.Aircraft lacks the properties this test sets"});
    }

    // static properties are set before the object is added to a bus
    auto aircraft = std::make_shared<sen::kernel::impl::GenericLocalObject>("runtime_aircraft", type);
    aircraft->setNextPropertyUntyped(entityTypeProperty, entityType());
    aircraft->setNextPropertyUntyped(alternateEntityType, entityType());
    aircraft->setNextPropertyUntyped(entityIdentifierProperty, entityIdentifier());

    auto bus = api.getSource("session.bus");
    std::ignore = bus->add(aircraft);

    int16_t count = 0;
    auto result = api.execLoop(sen::Duration::fromHertz(10.0),
                               [&]()
                               {
                                 if (count == lastIndex)
                                 {
                                   return;
                                 }
                                 ++count;
                                 aircraft->setNextPropertyUntyped(acousticSignatureIndex, count);
                                 aircraft->setNextPropertyUntyped(tentDeployed, count % 2 == 1);
                               });

    bus->remove(aircraft);
    return result;
  }
};

SEN_COMPONENT(RuntimeFomPublisher)
