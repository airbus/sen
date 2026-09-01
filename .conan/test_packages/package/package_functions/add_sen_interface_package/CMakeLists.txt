# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

set(interface_base_path "${CMAKE_CURRENT_SOURCE_DIR}/interface")

add_sen_interface_package(
  TARGET
  cmake_interface_types
  MAINTAINER
  "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  DESCRIPTION
  "Types exported by an interface package"
  VERSION
  "0.0.1"
  BASE_PATH
  ${interface_base_path}
  STL_FILES
  ${interface_base_path}/stl/interface_types.stl
)

get_target_property(interface_target_type cmake_interface_types TYPE)
if(NOT
   "${interface_target_type}"
   STREQUAL
   "INTERFACE_LIBRARY"
)
  message(FATAL_ERROR "add_sen_interface_package() did not create an interface library")
endif()

# Generate the C++ implementation for this consumer
get_target_property(interface_stl_files cmake_interface_types STL_FILES)
add_library(cmake_interface_cpp STATIC)
sen_generate_cpp(
  TARGET cmake_interface_cpp
  BASE_PATH ${interface_base_path}
  STL_FILES ${interface_stl_files}
)
target_link_libraries(cmake_interface_types INTERFACE cmake_interface_cpp)

# Generate a consumer that imports the interface types
set(consumer_base_path "${CMAKE_CURRENT_SOURCE_DIR}/consumer")
add_library(cmake_interface_consumer SHARED)
sen_generate_cpp(
  TARGET cmake_interface_consumer
  BASE_PATH ${consumer_base_path}
  STL_FILES ${consumer_base_path}/stl/consumer_types.stl
  GEN_HDR_FILES generated_headers
)
sen_add_dependencies(cmake_interface_consumer PRIVATE cmake_interface_types)

list(LENGTH generated_headers generated_header_count)
if(NOT
   generated_header_count
   EQUAL
   1
)
  message(FATAL_ERROR "The interface package consumer did not return its generated header")
endif()

list(
  GET
  generated_headers
  0
  generated_header
)
add_generated_file_test(
  cmake.add_sen_interface_package.consumer
  "${generated_header}"
  REQUIRED_TEXT
  "ConsumerValue"
)
