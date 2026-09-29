// === main.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================
#include <cstdlib>
#include <optional>

std::optional<int> configuredValue();

int main()
{
  const auto value = configuredValue();
  return value && *value == 17 ? EXIT_SUCCESS : EXIT_FAILURE;
}
