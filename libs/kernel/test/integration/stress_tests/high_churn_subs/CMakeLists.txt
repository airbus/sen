# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET high_churn_subs
  STL_FILES stl/high_churn_subs.stl
  SOURCES src/high_churn_subs.cpp
  NO_SCHEMA
  PRIVATE_DEPS spdlog::spdlog test_helpers
)

set(_working_dir $<TARGET_FILE_DIR:sen::cli_run>)
find_package(Python REQUIRED COMPONENTS Interpreter)

add_sen_integration_test(
  stress_test_high_churn_subs
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../run.py
  ${_working_dir}
  ${CMAKE_CURRENT_SOURCE_DIR}/config/process_1.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/config/process_2.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/config/process_3.yaml
  REQ_DEPS interest_filtering py ether kernel FLAKY USE_TESTCONTAINERS
)

# NOTE: this test is temporarily disabled until the data-races in the ObjectProvider are solved
set_tests_properties(stress_test_high_churn_subs PROPERTIES DISABLED TRUE)
