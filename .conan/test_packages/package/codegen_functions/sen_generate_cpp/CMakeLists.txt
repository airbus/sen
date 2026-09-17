# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

set(generated_schema "${CMAKE_CURRENT_BINARY_DIR}/generated_cpp.json")

add_library(cmake_generated_cpp SHARED)
sen_generate_cpp(
  TARGET cmake_generated_cpp
  OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated
  STL_FILES stl/generated_cpp.stl
  GEN_HDR_FILES generated_headers
  SCHEMA_FILE ${generated_schema} VISIBLE_CLASSES YES
)

list(LENGTH generated_headers generated_header_count)
if(NOT
   generated_header_count
   EQUAL
   1
)
  message(FATAL_ERROR "sen_generate_cpp() did not return its generated header")
endif()

list(
  GET
  generated_headers
  0
  generated_header
)
add_generated_file_test(
  cmake.sen_generate_cpp.header
  "${generated_header}"
  REQUIRED_TEXT
  "SEN_EXPORT"
)
add_generated_file_test(
  cmake.sen_generate_cpp.schema
  "${generated_schema}"
  REQUIRED_TEXT
  "cmake_generate_cpp.GeneratedClass"
)
