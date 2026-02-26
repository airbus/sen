# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

find_program(PYTEST_EXEC pytest REQUIRED)

# Generate the needed packages
add_sen_package(
  TARGET runtime_1
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Runtime instance example"
  SOURCES runtime_1/src/runtime.h runtime_1/src/runtime.cpp
  STL_FILES runtime_1/stl/runtime/runtime.stl
  BASE_PATH runtime_1
  EXPORT_NAME sen.test.runtime
  CODEGEN_SETTINGS runtime_1/src/codegen_settings.json
  PRIVATE_DEPS test_helpers
  NO_SCHEMA
)
sen_enable_static_analysis(runtime_1)

set_target_properties(runtime_1 PROPERTIES FOLDER "test")
set_target_properties(runtime_1 PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_sen_package(
  TARGET runtime_2
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Runtime instance example"
  SOURCES runtime_2/src/runtime.h runtime_2/src/runtime.cpp
  STL_FILES runtime_2/stl/runtime/runtime.stl
  BASE_PATH runtime_2
  EXPORT_NAME sen.test.runtime
  CODEGEN_SETTINGS runtime_2/src/codegen_settings.json
  PRIVATE_DEPS test_helpers
  NO_SCHEMA
)
sen_enable_static_analysis(runtime_2)

set_target_properties(runtime_2 PROPERTIES FOLDER "test")
set_target_properties(runtime_2 PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Generate the needed packages
add_sen_package(
  TARGET runtime_3
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Runtime instance example"
  SOURCES runtime_3/src/runtime.h runtime_3/src/runtime.cpp
  STL_FILES runtime_3/stl/runtime/runtime.stl
  BASE_PATH runtime_3
  EXPORT_NAME sen.test.runtime
  CODEGEN_SETTINGS runtime_3/src/codegen_settings.json
  PRIVATE_DEPS test_helpers
  NO_SCHEMA
)
sen_enable_static_analysis(runtime_3)

set_target_properties(runtime_3 PROPERTIES FOLDER "test")
set_target_properties(runtime_3 PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_sen_package(
  TARGET runtime_4
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Runtime instance example"
  SOURCES runtime_4/src/runtime.h runtime_4/src/runtime.cpp
  STL_FILES runtime_4/stl/runtime/runtime.stl
  BASE_PATH runtime_4
  EXPORT_NAME sen.test.runtime
  CODEGEN_SETTINGS runtime_4/src/codegen_settings.json
  PRIVATE_DEPS test_helpers
  NO_SCHEMA
)
sen_enable_static_analysis(runtime_4)

set_target_properties(runtime_4 PROPERTIES FOLDER "test")
set_target_properties(runtime_4 PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_sen_package(
  TARGET runtime_5
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Runtime instance example"
  SOURCES runtime_5/src/runtime.h runtime_5/src/runtime.cpp
  STL_FILES runtime_5/stl/runtime/runtime.stl
  BASE_PATH runtime_5
  EXPORT_NAME sen.test.runtime
  CODEGEN_SETTINGS runtime_5/src/codegen_settings.json
  PRIVATE_DEPS test_helpers
  NO_SCHEMA
)
sen_enable_static_analysis(runtime_5)

set_target_properties(runtime_5 PROPERTIES FOLDER "test")
set_target_properties(runtime_5 PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_sen_package(
  TARGET runtime_6
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Runtime instance example"
  SOURCES runtime_6/src/runtime.h runtime_6/src/runtime.cpp
  STL_FILES runtime_6/stl/runtime/runtime.stl
  BASE_PATH runtime_6
  EXPORT_NAME sen.test.runtime
  CODEGEN_SETTINGS runtime_6/src/codegen_settings.json
  PRIVATE_DEPS test_helpers
  NO_SCHEMA
)
sen_enable_static_analysis(runtime_6)

set_target_properties(runtime_6 PROPERTIES FOLDER "test")
set_target_properties(runtime_6 PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# runtime compatibility tests
#
# Check runtime compatibility between two processes
#
# Expected result: success

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_sen_integration_test(
  kernel_runtime_compat_test_1
  COMMAND
  ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/../runner.py
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime_1/config/runtime_1.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime_2/config/runtime_2.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/../tester.yaml
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  REQ_COMPONENTS ether py REQ_DEPS runtime_1 runtime_2
)

set_tests_properties(
  kernel_runtime_compat_test_1 PROPERTIES ENVIRONMENT
                                          "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/..;PYTHONUNBUFFERED=1"
)
set_tests_properties(kernel_runtime_compat_test_1 PROPERTIES RUN_SERIAL TRUE)

add_sen_integration_test(
  kernel_runtime_compat_test_2
  COMMAND
  ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/../runner.py
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime_3/config/runtime_3.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime_4/config/runtime_4.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/../tester.yaml
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  REQ_COMPONENTS ether py REQ_DEPS runtime_3 runtime_4
)

set_tests_properties(
  kernel_runtime_compat_test_2 PROPERTIES ENVIRONMENT
                                          "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/..;PYTHONUNBUFFERED=1"
)
set_tests_properties(kernel_runtime_compat_test_2 PROPERTIES RUN_SERIAL TRUE)

add_sen_integration_test(
  kernel_runtime_compat_test_3
  COMMAND
  ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/../runner.py
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime_5/config/runtime_5.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/runtime_6/config/runtime_6.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/../tester.yaml
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  REQ_COMPONENTS ether py REQ_DEPS runtime_5 runtime_6
)

set_tests_properties(
  kernel_runtime_compat_test_3 PROPERTIES ENVIRONMENT
                                          "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/..;PYTHONUNBUFFERED=1"
)
set_tests_properties(kernel_runtime_compat_test_3 PROPERTIES RUN_SERIAL TRUE)
