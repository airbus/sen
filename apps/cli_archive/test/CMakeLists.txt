# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/cli11)

add_sen_unit_test_suite(
  cli_archive_test
  cli_archive_test.cpp
  LINK_DEPS
  CLI11::CLI11
  cli_archive_for_testing
)

target_include_directories(
  cli_archive_test SYSTEM PRIVATE cli_archive CLI11::CLI11 ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_definitions(cli_archive_test PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

sen_enable_static_analysis(cli_archive_test)
