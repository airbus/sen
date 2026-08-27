# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET py_test_package
  SOURCES src_cpp/test_object.cpp
  STL_FILES stl/py_test_package.stl
  NO_SCHEMA
)

add_sen_package(
  TARGET py_open_query
  STL_FILES stl/py_open_query.stl
  NO_SCHEMA
)

add_sen_integration_test(
  py_modify_objects_test
  COMMAND
  ./cli_run
  ${CMAKE_CURRENT_SOURCE_DIR}/config/modify_objects.yaml
  WORKING_DIRECTORY $<TARGET_FILE_DIR:sen::cli_run>
  REQ_DEPS cli_run
)

add_sen_integration_test(
  py_open_query_test
  COMMAND
  ./cli_run
  ${CMAKE_CURRENT_SOURCE_DIR}/config/open_query.yaml
  WORKING_DIRECTORY $<TARGET_FILE_DIR:sen::cli_run>
  REQ_DEPS cli_run
)

add_sen_integration_test(
  py_wait_until_test
  COMMAND
  ./cli_run
  ${CMAKE_CURRENT_SOURCE_DIR}/config/wait_until.yaml
  WORKING_DIRECTORY $<TARGET_FILE_DIR:sen::cli_run>
  REQ_DEPS cli_run
)

add_sen_integration_test(
  py_test_event_test
  COMMAND
  ./cli_run
  ${CMAKE_CURRENT_SOURCE_DIR}/config/event.yaml
  WORKING_DIRECTORY $<TARGET_FILE_DIR:sen::cli_run>
  REQ_DEPS cli_run
)

append_test_env_modification(
  py_modify_objects_test "PYTHONPATH=path_list_append:${CMAKE_CURRENT_SOURCE_DIR}/src"
)

append_test_env_modification(py_open_query_test "PYTHONPATH=path_list_append:${CMAKE_CURRENT_SOURCE_DIR}/src")

append_test_env_modification(py_wait_until_test "PYTHONPATH=path_list_append:${CMAKE_CURRENT_SOURCE_DIR}/src")

append_test_env_modification(py_test_event_test "PYTHONPATH=path_list_append:${CMAKE_CURRENT_SOURCE_DIR}/src")
