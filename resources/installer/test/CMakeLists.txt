# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

find_program(bats_exec NAMES bats)

if(bats_exec)
  add_test(installer_tests ${CMAKE_CURRENT_LIST_DIR}/run.sh)
else()
  message(WARNING "bats not found - not running installer script tests")
endif()
