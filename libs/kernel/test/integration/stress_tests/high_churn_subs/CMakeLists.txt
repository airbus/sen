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

# 1. Determine frequency and suffix based on sanitizer status
if(SEN_USE_SANITIZER
   AND NOT
       SEN_USE_SANITIZER
       STREQUAL
       "None"
)
  set(FREQUENCY 20)
  set(SUFFIX "_sanitizers")
else()
  set(FREQUENCY 100)
  set(SUFFIX "")
endif()

# 2. Generate the config files using a single loop
set(CONFIG_FILES "process_1" "process_2" "process_3")
foreach(FILE IN LISTS CONFIG_FILES)
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/${FILE}.yaml.in
    ${CMAKE_CURRENT_BINARY_DIR}/config/${FILE}${SUFFIX}.yaml @ONLY
  )
endforeach()

# 3. Define the integration test once using the dynamic suffix
set(_working_dir $<TARGET_FILE_DIR:sen::cli_run>)
find_package(Python REQUIRED COMPONENTS Interpreter)

add_sen_integration_test(
  stress_test_high_churn_subs
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../run.py
  ${_working_dir}
  ${CMAKE_CURRENT_BINARY_DIR}/config/process_1${SUFFIX}.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/config/process_2${SUFFIX}.yaml
  ${CMAKE_CURRENT_BINARY_DIR}/config/process_3${SUFFIX}.yaml
  REQ_DEPS interest_filtering py ether kernel FLAKY USE_TESTCONTAINERS
)

# ctest's global --timeout is 20s. Measured on the CI runner: 12.1s in Release, 19.4s in Debug,
# so Debug clears the limit by 3% and ordinary jitter reads as a hang. Two cores are what makes
# the difference -- on a fourteen-core machine both build types take 11.5s.
#
# The driver's own deadline stays under ctest's, so a run that really does hang is reported by the
# driver, which names the participants, rather than killed by ctest, which says only that time ran
# out.
set_tests_properties(stress_test_high_churn_subs PROPERTIES TIMEOUT 120)
append_test_env_modification(stress_test_high_churn_subs "SEN_INTEGRATION_TEST_TIMEOUT=set:90")
