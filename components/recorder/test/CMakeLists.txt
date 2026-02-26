# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

set(recorder_tests recorder_crash_test.cpp)

add_sen_unit_test_suite(
  recorder_test
  ${recorder_tests}
  LINK_DEPS
  sen::core
  sen::kernel
  sen::db
)

target_include_directories(
  recorder_test SYSTEM
  INTERFACE
  PRIVATE ${CMAKE_SOURCE_DIR}/components/recorder/src
)

target_compile_definitions(recorder_test PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

set_target_properties(
  recorder_test PROPERTIES FOLDER "test" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)
