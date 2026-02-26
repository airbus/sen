# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

find_program(PYTEST_EXEC pytest REQUIRED)

include(tps/spdlog)

# Generate the needed packages
add_sen_package(
  TARGET transport_1
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Transport instance example"
  SOURCES transport_1/src/transport.h transport_1/src/transport.cpp
  STL_FILES transport_1/stl/transport/transport.stl
  BASE_PATH transport_1
  EXPORT_NAME sen.test
  CODEGEN_SETTINGS transport_1/src/codegen_settings.json
  PRIVATE_DEPS spdlog::spdlog test_helpers
  NO_SCHEMA
)

set_target_properties(transport_1 PROPERTIES FOLDER "test")
set_target_properties(transport_1 PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_sen_package(
  TARGET transport_2
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Transport instance example"
  SOURCES transport_2/src/transport.h transport_2/src/transport.cpp
  STL_FILES transport_2/stl/transport/transport.stl
  BASE_PATH transport_2
  EXPORT_NAME sen.test
  CODEGEN_SETTINGS transport_2/src/codegen_settings.json
  PRIVATE_DEPS spdlog::spdlog test_helpers
  NO_SCHEMA
)

set_target_properties(transport_2 PROPERTIES FOLDER "test")
set_target_properties(transport_2 PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# transport tests
#
# Check transport between two processes
#
# Expected result: success

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_sen_integration_test(
  kernel_transport_test
  COMMAND
  ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/../runner.py
  ${CMAKE_CURRENT_SOURCE_DIR}/transport_1/config/transport_1.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/transport_2/config/transport_2.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/../tester.yaml
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  REQ_COMPONENTS ether py REQ_DEPS transport_1 transport_2
)

set_tests_properties(
  kernel_transport_test PROPERTIES ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/..;PYTHONUNBUFFERED=1"
)
set_tests_properties(kernel_transport_test PROPERTIES RUN_SERIAL TRUE)
