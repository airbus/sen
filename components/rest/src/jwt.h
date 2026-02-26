// === jwt.h ===========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_SRC_JWT_H
#define SEN_COMPONENTS_REST_SRC_JWT_H

// std
#include <string>

namespace sen::components::rest
{

enum class JWTError
{
  noError = 0,
  tokenExpired,
  tokenDecodingError,
  invalidTokenFormat,
  invalidSignatureFormat,
  invalidExpirationTime,
  invalidSignature,
  invalidPayload,
};

struct JWT
{
  bool valid = false;
  JWTError error = JWTError::noError;

  std::string header;
  std::string payload;
};

std::string encodeJWT(const std::string& payload);
JWT decodeJWT(const std::string& token);
std::string toString(const JWTError& jwtError);

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_JWT_H
