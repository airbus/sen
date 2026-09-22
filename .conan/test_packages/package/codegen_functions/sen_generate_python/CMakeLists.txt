# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")

# sen_generate_python configures and links a library target
# so it needs a compilable source
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/empty.cpp" "")
add_library(cmake_generated_python STATIC "${CMAKE_CURRENT_BINARY_DIR}/empty.cpp")
sen_generate_python(
  TARGET cmake_generated_python
  OUTPUT_DIR ${generated_dir}
  STL_FILES generated_python.stl
  GEN_HDR_FILES generated_files
)

list(LENGTH generated_files generated_file_count)
if(NOT
   generated_file_count
   EQUAL
   1
)
  message(FATAL_ERROR "sen_generate_python() did not return its generated file")
endif()

list(
  GET
  generated_files
  0
  generated_file
)
add_generated_file_test(
  cmake.sen_generate_python.module
  "${generated_file}"
  REQUIRED_TEXT
  "GeneratedConfig"
)

# package markers created during configuration
foreach(marker IN ITEMS __init__.py py.typed)
  if(NOT EXISTS "${generated_dir}/${marker}")
    message(FATAL_ERROR "sen_generate_python() did not create ${marker}")
  endif()
endforeach()
