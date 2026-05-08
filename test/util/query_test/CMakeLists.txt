# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET query_test
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Checks query subscription against enums and optionals"
  SOURCES src/component.cpp
  STL_FILES stl/query_test.stl
  SCHEMA_PATH ${CMAKE_CURRENT_BINARY_DIR}
  IS_COMPONENT
)

sen_enable_static_analysis(query_test)

set_target_properties(query_test PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_sen_run_smoke_test(query_test CONFIG_FILE config/component.yaml)
