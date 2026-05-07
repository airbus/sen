# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_sen_package(
  TARGET type_clash_participant_1
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Type clash participant 1"
  SOURCES participant_1/src/participant_1.h participant_1/src/participant_1.cpp
  STL_FILES participant_1/stl/participant_1.stl
  BASE_PATH participant_1
  EXPORT_NAME sen.test.type_clash
  NO_SCHEMA
)
sen_enable_static_analysis(type_clash_participant_1)
set_target_properties(
  type_clash_participant_1 PROPERTIES FOLDER "test" LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_sen_package(
  TARGET type_clash_participant_2
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Type clash participant 2"
  SOURCES participant_2/src/participant_2.h participant_2/src/participant_2.cpp
  STL_FILES participant_2/stl/participant_2.stl
  BASE_PATH participant_2
  EXPORT_NAME sen.test.type_clash
  NO_SCHEMA
)
sen_enable_static_analysis(type_clash_participant_2)
set_target_properties(
  type_clash_participant_2 PROPERTIES FOLDER "test" LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_sen_package(
  TARGET type_clash_participant_3
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Type clash participant 3"
  SOURCES participant_3/src/participant_3.h participant_3/src/participant_3.cpp
  STL_FILES participant_3/stl/participant_3.stl
  BASE_PATH participant_3
  EXPORT_NAME sen.test.type_clash
  NO_SCHEMA
)
sen_enable_static_analysis(type_clash_participant_3)
set_target_properties(
  type_clash_participant_3 PROPERTIES FOLDER "test" LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_sen_integration_test(
  kernel_type_clash_test_1
  COMMAND
  ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/../runner.py
  ${CMAKE_CURRENT_SOURCE_DIR}/participant_1/config/participant_1.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/participant_2/config/participant_2.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/type_clash_tester.yaml
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  REQ_COMPONENTS ether py REQ_DEPS type_clash_participant_1 type_clash_participant_2
)

set_tests_properties(
  kernel_type_clash_test_1
  PROPERTIES ENVIRONMENT
             "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}:${CMAKE_CURRENT_SOURCE_DIR}/..;PYTHONUNBUFFERED=1"
)
set_tests_properties(kernel_type_clash_test_1 PROPERTIES RUN_SERIAL TRUE)
set_tests_properties(
  kernel_type_clash_test_1 PROPERTIES PASS_REGULAR_EXPRESSION "has an incompatible remote implementation"
)

add_sen_integration_test(
  kernel_type_clash_test_2
  COMMAND
  ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/../runner.py
  ${CMAKE_CURRENT_SOURCE_DIR}/participant_1/config/participant_1.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/participant_3/config/participant_3.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/type_clash_tester.yaml
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  REQ_COMPONENTS ether py REQ_DEPS type_clash_participant_1 type_clash_participant_3
)

set_tests_properties(
  kernel_type_clash_test_2
  PROPERTIES ENVIRONMENT
             "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}:${CMAKE_CURRENT_SOURCE_DIR}/..;PYTHONUNBUFFERED=1"
)
set_tests_properties(kernel_type_clash_test_2 PROPERTIES RUN_SERIAL TRUE)
set_tests_properties(
  kernel_type_clash_test_2 PROPERTIES PASS_REGULAR_EXPRESSION "has an incompatible remote implementation"
)
