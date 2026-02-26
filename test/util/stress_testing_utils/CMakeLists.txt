# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

find_package(spdlog REQUIRED)

add_sen_package(
  TARGET stress_testing_utils
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Utilities for stress tests"
  SOURCES src/confirmed_producer.cpp
  STL_FILES stl/stress_testing_utils/confirmed_producer.stl
)

target_link_libraries(stress_testing_utils PRIVATE $<BUILD_INTERFACE:spdlog::spdlog>)
set_target_properties(stress_testing_utils PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
