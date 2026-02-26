// === types.h =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_SRC_TYPES_H
#define SEN_COMPONENTS_REST_SRC_TYPES_H

// component
#include "locators.h"

// sen
#include "sen/core/base/result.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/core/obj/object.h"

// generated code
#include "stl/types.stl.h"

// json
#include "nlohmann/json.hpp"

// std
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace sen::components::rest
{

using InterestName = std::string;
using InvokeId = uint32_t;
using InterestCallback = std::function<void(const InterestName& interestName, sen::ObjectId objectId)>;

/// Abstracts an invocation to a method of a Sen object
struct Invoke
{
  std::optional<InvokeId> id = std::nullopt;
  InvokeStatus status = InvokeStatus::finished;
  MethodResult<Var> result = sen::Ok(Var());
};

[[nodiscard]] std::string toJson(const Invoke& i);

[[nodiscard]] std::string toJson(const sen::Object& object, const BusLocator& busLocator);

[[nodiscard]] std::string toJson(const sen::Object& object, const PropertyLocator& propertyLocator);

[[nodiscard]] Interest fromJson(const nlohmann::json& j);

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_TYPES_H
