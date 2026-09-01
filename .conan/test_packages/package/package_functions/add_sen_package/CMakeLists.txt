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
  GEN_HDR_FILES my_package_generated_headers
  SCHEMA_PATH ${CMAKE_CURRENT_BINARY_DIR}
)

get_target_property(my_package_type my_package TYPE)
if(NOT
   "${my_package_type}"
   STREQUAL
   "SHARED_LIBRARY"
)
  message(FATAL_ERROR "add_sen_package() created ${my_package_type}; expected SHARED_LIBRARY")
endif()

get_target_property(my_package_schema my_package SCHEMA)
set(expected_my_package_schema "${CMAKE_CURRENT_BINARY_DIR}/my_package.json")
if(NOT
   "${my_package_schema}"
   STREQUAL
   "${expected_my_package_schema}"
)
  message(FATAL_ERROR "add_sen_package() returned an unexpected schema path: ${my_package_schema}")
endif()

list(LENGTH my_package_generated_headers generated_header_count)
if(NOT
   generated_header_count
   EQUAL
   4
)
  message(FATAL_ERROR "add_sen_package() returned ${generated_header_count} headers; expected 4")
endif()

target_link_libraries(my_package PRIVATE $<BUILD_INTERFACE:spdlog::spdlog>)
set_target_properties(my_package PROPERTIES FOLDER "examples/basic")

foreach(generated_header IN LISTS my_package_generated_headers)
  get_filename_component(header_name "${generated_header}" NAME)
  add_generated_file_test(cmake.add_sen_package.${header_name} "${generated_header}")
endforeach()
add_generated_file_test(
  cmake.add_sen_package.schema
  "${my_package_schema}"
  REQUIRED_TEXT
  "my_package.MyClass"
)

# TODO(SEN-1185): enable tests when possible
configure_file(config/my_package.yaml ${CMAKE_BINARY_DIR}/test_configs/my_package.yaml COPYONLY)
