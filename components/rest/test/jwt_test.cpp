// === jwt_test.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "jwt.h"

// google test
#include <gtest/gtest.h>

// std
#include <string>

/// @test
/// Check JWT generation
/// @requirements(SEN-1061)
TEST(Rest, unsigned_jwt)
{
  const std::string payload = R"({ "user": "admin" })";

  std::string token = sen::components::rest::encodeJWT(payload);
  auto jwt = sen::components::rest::decodeJWT(token);

  ASSERT_TRUE(jwt.valid);
  ASSERT_EQ(jwt.payload, payload);
}
