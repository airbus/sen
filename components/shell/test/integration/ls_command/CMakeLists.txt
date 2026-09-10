# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET ls_command_test
  STL_FILES stl/ls_command_test.stl
  SOURCES src/publisher.cpp
  NO_SCHEMA
)

find_package(Python REQUIRED COMPONENTS Interpreter)
set(_working_dir $<TARGET_FILE_DIR:sen::cli_run>)

# Checks that the initial 'ls' command discovers unopened sessions
add_sen_integration_test(
  shell_ls_initial_discovery
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/test_ls.py
  ${_working_dir}/cli_run
  ${CMAKE_CURRENT_SOURCE_DIR}/config/publisher.yaml
  TestLsCommand.test_initial_discovery
  REQ_DEPS ls_command_test py ether shell kernel
)

# Checks that 'ls' shows the correct tags after opening a session
add_sen_integration_test(
  shell_ls_open_session
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/test_ls.py
  ${_working_dir}/cli_run
  ${CMAKE_CURRENT_SOURCE_DIR}/config/publisher.yaml
  TestLsCommand.test_open_session
  REQ_DEPS ls_command_test py ether shell kernel
)

# Checks that 'ls' shows the correct tags after opening a specific bus
add_sen_integration_test(
  shell_ls_open_bus
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/test_ls.py
  ${_working_dir}/cli_run
  ${CMAKE_CURRENT_SOURCE_DIR}/config/publisher.yaml
  TestLsCommand.test_open_bus
  REQ_DEPS ls_command_test py ether shell kernel
)

# Checks that 'ls' shows the correct tags when opening an empty session context
add_sen_integration_test(
  shell_ls_open_empty_session
  COMMAND
  ${Python_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/test_ls.py
  ${_working_dir}/cli_run
  ${CMAKE_CURRENT_SOURCE_DIR}/config/publisher.yaml
  TestLsCommand.test_open_empty_session
  REQ_DEPS ls_command_test py ether shell kernel
)
