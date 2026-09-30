# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_component(
  TARGET cmake_component
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  DESCRIPTION "Component generated through add_sen_component"
  VERSION "0.0.1"
  STL_FILES stl/component_types.stl
  GEN_HDR_FILES generated_headers
  SCHEMA_PATH ${CMAKE_CURRENT_BINARY_DIR}
)

list(LENGTH generated_headers generated_header_count)
if(NOT
   generated_header_count
   EQUAL
   1
)
  message(FATAL_ERROR "add_sen_component() did not return its generated header")
endif()

get_target_property(component_schema cmake_component SCHEMA)
list(
  GET
  generated_headers
  0
  generated_header
)
add_generated_file_test(cmake.add_sen_component.header "${generated_header}")
add_generated_file_test(
  cmake.add_sen_component.schema
  "${component_schema}"
  REQUIRED_TEXT
  "\"component\": \"cmake_component\""
)

# No EXPORT_INTERFACES, and this package installs none of its .stl files, so the paths must stay
# out of the export set: get_external_interfaces() would rewrite them into a directory that has
# nothing in it and stop the consumer's configure.
get_target_property(cmake_component_exported cmake_component EXPORT_PROPERTIES)
foreach(gated_property IN ITEMS BASE_PATH STL_FILES)
  if("${gated_property}" IN_LIST cmake_component_exported)
    message(FATAL_ERROR "add_sen_component() exported ${gated_property} without EXPORT_INTERFACES: "
                        "${cmake_component_exported}"
    )
  endif()
endforeach()
