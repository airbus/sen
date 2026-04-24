# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET py_test_package
  STL_FILES stl/py_test_package.stl
  NO_SCHEMA
)

add_sen_integration_test(
  py_modify_objects_test
  COMMAND
  ./cli_run
  ${CMAKE_CURRENT_SOURCE_DIR}/config/modify_objects.yaml
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  REQ_DEPS cli_run
)

append_test_env_modification(
  py_modify_objects_test "PYTHONUNBUFFERED=set:1;PYTHONPATH=path_list_append:${CMAKE_CURRENT_SOURCE_DIR}/src"
)
