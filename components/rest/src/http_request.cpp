// === http_request.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "http_request.h"

// component
#include "jwt.h"

// std
#include <optional>
#include <string>
#include <string_view>

namespace sen::components::rest
{

constexpr std::string_view bearerPrefix = "Bearer ";

std::optional<JWT> HttpRequest::sessionToken() const
{
  // First, try to get token from cookie header
  auto cookieHeaderIt = headers_.find("cookie");
  if (cookieHeaderIt != headers_.cend())
  {
    auto cookieHeader = std::string(cookieHeaderIt->second.value().data());
    const std::string tokenPrefix = "token=";
    auto tokenPos = cookieHeader.find(tokenPrefix);

    if (tokenPos != std::string::npos)
    {
      auto tokenStart = tokenPos + tokenPrefix.length();
      auto tokenEnd = cookieHeader.find(';', tokenStart);
      std::string token;

      if (tokenEnd != std::string::npos)
      {
        token = cookieHeader.substr(tokenStart, tokenEnd - tokenStart);
      }
      else
      {
        token = cookieHeader.substr(tokenStart);
      }

      if (!token.empty())
      {
        return decodeJWT(token);
      }
    }
  }

  // Fall back to Authorization header
  auto authHeaderIt = headers_.find("authorization");
  if (authHeaderIt == headers_.cend())
  {
    return std::nullopt;
  }

  auto authHeader = std::string(authHeaderIt->second.value().data());
  if (authHeader.rfind(bearerPrefix) != 0)
  {
    return std::nullopt;
  }

  const std::string authToken = authHeader.substr(bearerPrefix.length());
  return decodeJWT(authToken);
}

}  // namespace sen::components::rest
