# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# configure_exportable_packages(TARGET_NAME_CMAKEDIRS) has no caller in this repository -- Sen
# exports its own configs without the flag -- so nothing here exercises the layout it produces for a
# consumer, which is the layout the how-to guides teach. This installs a package that uses it and
# resolves it back, so a change to the helper fails here rather than in someone else's project.
#
# A script rather than a target: what is under test is where files land at install time and whether
# find_package reaches them, which needs a real install and a second configure.
add_test(
  NAME exportable_packages
  COMMAND
    ${CMAKE_COMMAND} -DSEN_CMAKE_UTILS_DIR=${PROJECT_SOURCE_DIR}/cmake/util
    -DFIXTURE_DIR=${CMAKE_CURRENT_LIST_DIR}/fixture -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/work -P
    ${CMAKE_CURRENT_LIST_DIR}/run.cmake
)
