# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

find_program(bats_exec NAMES bats)

if(bats_exec)
  add_test(NAME installer_tests COMMAND ${CMAKE_CURRENT_LIST_DIR}/run.sh)
  # It drives installed binaries, which are instrumented like any other.
  add_sanitizer_options(installer_tests)
elseif(LINUX AND DEFINED ENV{CI})
  # The image and setup_build_context both install bats on purpose. If it is
  # missing here, the environment lost it and this suite would disappear
  # without anyone noticing, which is the one thing a check must never do.
  message(FATAL_ERROR "bats not found: the installer test suite cannot run.")
else()
  message(WARNING "bats not found - installer script tests will not run. Install bats to run them locally.")
endif()
