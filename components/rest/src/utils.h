// === utils.h =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_SRC_UTILS_H
#define SEN_COMPONENTS_REST_SRC_UTILS_H

// component
#include "json_response.h"
#include "locators.h"

// generated code
#include "stl/types.stl.h"

// sen
#include "sen/core/base/span.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/var.h"

// spdlog
#include <spdlog/logger.h>

// std
#include <memory>
#include <optional>
#include <string>

namespace sen::components::rest
{

using ArgsError = std::optional<std::string>;

[[nodiscard]] std::shared_ptr<spdlog::logger> getLogger();

[[nodiscard]] JsonResponse getErrorLocatorParams(const LocatorError& error);

[[nodiscard]] const JsonResponse& getErrorInvalidParams();

[[nodiscard]] const JsonResponse& getErrorNotFound();

[[nodiscard]] const JsonResponse& getErrorNotFoundOrOpen();

[[nodiscard]] std::string urlSanitizeLocalName(std::string name);

[[nodiscard]] ArgsError checkValidExpectedArgs(const Span<const Arg> expectedArgs, const VarList& args);

[[nodiscard]] std::string toSSE(const Notification& notification);

[[nodiscard]] std::string tolower(std::string value);

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_UTILS_H
