# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET test_helpers
  SOURCES include/test_helpers/helpers.h src/helpers.cpp
  STL_FILES stl/test_helpers/test_helpers.stl
  BASE_PATH stl
  CODEGEN_SETTINGS codegen_settings.json
  PUBLIC_SYMBOLS
  EXPORT_NAME sen.test
)

target_include_directories(test_helpers PUBLIC include)

# setting up the environment for the test
find_package(Python REQUIRED COMPONENTS Interpreter)
set(_working_dir $<TARGET_FILE_DIR:sen::cli_run>)
set(_script_file ${CMAKE_CURRENT_SOURCE_DIR}/../run.py)

# Base test that checks the communication between integration test Listener and Publisher objects without an implementation. This objects are then implemented in the integration tests.
add_sen_integration_test(
  base_kernel_integration_test
  COMMAND
  ${Python_EXECUTABLE}
  ${_script_file}
  ${_working_dir}
  ${CMAKE_CURRENT_SOURCE_DIR}/config/base_test_proc_1.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/config/base_test_proc_2.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/config/base_test_proc_3.yaml
  REQ_DEPS interest_filtering py ether kernel FLAKY USE_TESTCONTAINERS
)
