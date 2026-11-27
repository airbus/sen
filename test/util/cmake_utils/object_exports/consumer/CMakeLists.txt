# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

cmake_minimum_required(VERSION 3.21)
project(object_export_consumer LANGUAGES CXX)
enable_testing()
add_library(sen::kernel INTERFACE IMPORTED)
include("${FIXTURE_EXPORT}")
foreach(target ordinary split)
  get_target_property(existing_metadata fixture::${target} FIXTURE_METADATA)
  if(NOT
     existing_metadata
     STREQUAL
     "preserved"
  )
    message(FATAL_ERROR "${target}: installed Sen metadata or existing export properties were lost")
  endif()
  add_executable(check_${target} ../main.cpp)
  target_link_libraries(check_${target} PRIVATE fixture::${target})
  if(WIN32)
    add_custom_command(
      TARGET check_${target}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:fixture::${target}>"
              "$<TARGET_FILE_DIR:check_${target}>"
      VERBATIM
    )
  endif()
  add_test(NAME check_${target} COMMAND check_${target})
endforeach()
