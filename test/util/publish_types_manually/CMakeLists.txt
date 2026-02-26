# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_component(
  TARGET publish_types_manually
  MAINTAINER "Sen Core Team (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Checks if types are published with all its dependencies"
  SOURCES src/component.cpp
  STL_FILES stl/struct_inheritance.stl
  SCHEMA_PATH ${CMAKE_CURRENT_BINARY_DIR}
)
sen_enable_static_analysis(publish_types_manually)

set_target_properties(publish_types_manually PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_sen_run_smoke_test(publish_types_manually CONFIG_FILE config/component.yaml)
