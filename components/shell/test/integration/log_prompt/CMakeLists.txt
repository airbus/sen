# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET log_prompt_test
  STL_FILES stl/log_prompt_test.stl
  SOURCES src/publisher.cpp
  NO_SCHEMA
)

find_package(Python REQUIRED COMPONENTS Interpreter)
set(_working_dir $<TARGET_FILE_DIR:sen::cli_run>)

# Checks that the shell prompt is correctly shown after the initial logs
add_sen_integration_test(
  shell_log_prompt_initial
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/test_log_prompt.py
  ${_working_dir}/cli_run
  ${CMAKE_CURRENT_SOURCE_DIR}/config/publisher.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/config/listener.yaml
  TestLogPrompt.test_initial_startup_prompt
  REQ_DEPS log_prompt_test py ether shell kernel
)

# Checks that the shell prompt is correctly restored after asynchronous logs
add_sen_integration_test(
  shell_log_prompt_async
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/test_log_prompt.py
  ${_working_dir}/cli_run
  ${CMAKE_CURRENT_SOURCE_DIR}/config/publisher.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/config/listener.yaml
  TestLogPrompt.test_prompt_after_async_log
  REQ_DEPS log_prompt_test py ether shell kernel
)
