# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

find_package(spdlog REQUIRED)

add_sen_package(
  TARGET my_package
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Example package skeleton"
  SOURCES src/my_class.h src/my_class.cpp
  STL_FILES stl/my_package/my_class.stl
            stl/my_package/basic_types.stl
            stl/my_package/config.stl
            stl/my_package/my_abstract_class.stl
  CODEGEN_SETTINGS src/codegen_settings.json
  SCHEMA_PATH ${CMAKE_CURRENT_BINARY_DIR}
)

target_sources(my_package PRIVATE ${_combined_schema})
target_link_libraries(my_package PRIVATE $<BUILD_INTERFACE:spdlog::spdlog>)
set_target_properties(my_package PROPERTIES FOLDER "examples/basic")
set_target_properties(my_package PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# TODO(SEN-1185): enable tests when possible
configure_file(config/my_package.yaml ${CMAKE_BINARY_DIR}/test_configs/my_package.yaml COPYONLY)
