# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

sen_generate_package(
  TARGET cmake_generated_package
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  DESCRIPTION "Package generated through sen_generate_package"
  VERSION "0.0.1"
  STL_FILES stl/generated_package.stl
  GEN_HDR_FILES generated_headers
  NO_SCHEMA
)

list(LENGTH generated_headers generated_header_count)
if(NOT
   generated_header_count
   EQUAL
   1
)
  message(FATAL_ERROR "sen_generate_package() did not return its generated header")
endif()

# sen_generate_package() is the public-symbols wrapper around add_sen_package().
list(
  GET
  generated_headers
  0
  generated_header
)
add_generated_file_test(
  cmake.sen_generate_package.header
  "${generated_header}"
  REQUIRED_TEXT
  "SEN_EXPORT"
)
