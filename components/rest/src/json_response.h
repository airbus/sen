// === json_response.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_SRC_JSON_RESPONSE_H
#define SEN_COMPONENTS_REST_SRC_JSON_RESPONSE_H

// component
#include "http_header.h"
#include "http_response.h"
#include "response_adapter.h"

// generated code
#include "stl/types.stl.h"

// sen
#include "sen/core/io/util.h"
#include "sen/core/meta/type_traits.h"
#include "sen/core/meta/var.h"

// std
#include <exception>
#include <string>
#include <vector>

namespace sen::components::rest
{

/// JsonResponse is a helper class to handle HTTP response which are JSON based.
class JsonResponse: public HttpResponse
{
public:
  template <typename T>
  JsonResponse(int statusCode, const T& data)
    : HttpResponse(statusCode, std::vector<HttpHeader> {{"Content-Type", "application/json"}})
  {
    auto var = sen::toVariant(data);

    const auto* metaTypePtr = sen::MetaTypeTrait<T>::meta().type();
    if (metaTypePtr)
    {
      adaptForJsonResponse(var, metaTypePtr);
    }

    try
    {
      setBody(sen::toJson(var));
    }
    catch (std::exception& e)
    {
      setBody(sen::toJson(sen::toVariant(Error {"error on invalid body"})));
    }
  }

  explicit JsonResponse(int statusCode = httpInternalServerError, const std::string& data = "")
    : HttpResponse(statusCode, std::vector<HttpHeader> {{"Content-Type", "application/json"}}, data)
  {
    // Left blank intentionally
  }
};

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_JSON_RESPONSE_H
