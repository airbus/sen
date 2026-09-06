# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET fibonacci
  TEST_TARGET fibonacci_test_lib # static library with all symbols, used by unit tests
  MAINTAINER "Sen Core Team (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Illustrates the usage of deferred functions"
  SOURCES src/fibonacci.cpp
  STL_FILES stl/fibonacci.stl
  CODEGEN_SETTINGS src/codegen_settings.json
  SCHEMA_PATH ${CMAKE_CURRENT_BINARY_DIR}
)

add_sen_unit_test_suite(
  fibonacci_test
  test/fibonacci_algorithm_test.cpp
  LINK_DEPS
  fibonacci_test_lib
)
