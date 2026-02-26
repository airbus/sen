# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_subdirectory(unit)

# Disable integration tests due to windows build agent problems
if(NOT MSVC)
  add_subdirectory(integration/test_helpers)
  add_subdirectory(integration/transport)
  add_subdirectory(integration/runtime_compatibility)
endif()
