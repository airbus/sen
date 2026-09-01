# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_sen_integration_test(
  kernel_termination_test
  COMMAND
  ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/terminate_test.py
  ${CMAKE_CURRENT_SOURCE_DIR}/bare_kernel.yaml
  WORKING_DIRECTORY $<TARGET_FILE_DIR:sen::cli_run>
  REQ_DEPS cli_run
)

set_tests_properties(kernel_termination_test PROPERTIES ENVIRONMENT "PYTHONUNBUFFERED=1")
