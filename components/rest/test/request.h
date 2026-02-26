// === request.h =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_TEST_REQUEST_H
#define SEN_COMPONENTS_REST_TEST_REQUEST_H

// auto-generated code
#include "stl/types.stl.h"

// json
#include <nlohmann/json.hpp>

// std
#include <optional>
#include <string>

using HttpMethod = sen::components::rest::HttpMethod;
using Json = nlohmann::json;

struct HttpResponse
{
  int statusCode;
  std::string body;
};

HttpResponse request(const HttpMethod& method,
                     const std::string& host,
                     const std::string& port,
                     const std::string& path,
                     const Json& data = Json(),
                     const std::string& token = "",
                     bool isSSE = false);

std::optional<std::string> authenticate();

#endif  // SEN_COMPONENTS_REST_TEST_REQUEST_H
