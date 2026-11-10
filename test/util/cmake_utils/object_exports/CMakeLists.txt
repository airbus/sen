# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

cmake_minimum_required(VERSION 3.21)
project(object_exports LANGUAGES CXX)
enable_testing()

include("${SEN_SOURCE_DIR}/cmake/util/sen_misc_utils.cmake")
include("${SEN_SOURCE_DIR}/cmake/util/sen_package_utils.cmake")
include("${SEN_SOURCE_DIR}/cmake/util/sen_internal_utils.cmake")

# Exercise the actual package/installation graph without requiring a built Sen runtime.
# Only compiler setup, build provenance and the code generator are replaced.
function(sen_configure_target target)
  set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()
function(sen_add_build_info target name)

endfunction()
function(sen_generate_cpp)
  cmake_parse_arguments(
    GEN
    ""
    "TARGET;OUTPUT_DIR"
    ""
    ${ARGN}
  )
  add_custom_command(
    OUTPUT "${GEN_OUTPUT_DIR}/generated.h" "${GEN_OUTPUT_DIR}/generated.cpp"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GEN_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/generated.h" "${GEN_OUTPUT_DIR}/generated.h"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/generated.cpp"
            "${GEN_OUTPUT_DIR}/generated.cpp"
    DEPENDS generated.h generated.cpp
    VERBATIM
  )
  target_sources(${GEN_TARGET} PRIVATE "${GEN_OUTPUT_DIR}/generated.cpp")
  target_include_directories(${GEN_TARGET} PUBLIC "$<BUILD_INTERFACE:${GEN_OUTPUT_DIR}>")
endfunction()

add_library(sen::kernel INTERFACE IMPORTED)
add_library(private_dependency STATIC private.cpp)
set_target_properties(private_dependency PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_sen_package(
  TARGET ordinary
  SOURCES package.cpp
  STL_FILES fixture.stl
  PRIVATE_DEPS private_dependency
  NO_SCHEMA
)
add_sen_package(
  TARGET split
  TEST_TARGET split_test
  SOURCES package.cpp
  STL_FILES fixture.stl
  PRIVATE_DEPS private_dependency
  NO_SCHEMA
)

foreach(target ordinary_gen split_gen split_obj)
  get_target_property(type ${target} TYPE)
  if(NOT
     type
     STREQUAL
     "OBJECT_LIBRARY"
  )
    message(FATAL_ERROR "${target} must remain an independent OBJECT library")
  endif()
endforeach()

foreach(target ordinary split split_test)
  add_executable(check_${target} main.cpp)
  target_link_libraries(check_${target} PRIVATE ${target})
  add_test(NAME check_${target} COMMAND check_${target})
endforeach()

# Neither the private dependency nor the helper objects belong in this export set.
foreach(target ordinary split)
  set_target_properties(
    ${target}
    PROPERTIES SEN_EXPORTS_TYPES YES
               SEN_IMPORT_DIRS "-i;interfaces/${target};-i;interfaces/common"
               FIXTURE_METADATA "preserved"
               EXPORT_PROPERTIES FIXTURE_METADATA
  )
endforeach()
sen_internal_install(ordinary)
sen_internal_install(split)
install(
  EXPORT sen_targets
  NAMESPACE fixture::
  DESTINATION lib/cmake/fixture
)
export(
  EXPORT sen_targets
  NAMESPACE fixture::
  FILE "${CMAKE_CURRENT_BINARY_DIR}/fixture-targets.cmake"
)
