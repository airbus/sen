# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_subdirectory(unit)

# Disable integration tests in Windows
if(NOT MSVC)
  add_subdirectory(integration/test_helpers)
  add_subdirectory(integration/crash_report)

  # integration tests require ether and python
  if(SEN_BUILD_ETHER AND SEN_BUILD_PY)
    add_subdirectory(integration/transport)
    add_subdirectory(integration/runtime_compatibility)
    add_subdirectory(integration/type_clash)
  endif()
endif()
