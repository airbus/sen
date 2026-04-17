# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# cmake config
cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

# where to find our cmake files
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# build options
option(SEN_BUILD_EXAMPLES "Build examples" OFF)
option(SEN_BUILD_TESTS "Build tests" OFF)
option(SEN_BUILD_DOCS "Build documentation" ON)
option(SEN_DISABLE_CLANG_TIDY "Disable clang-tidy" ON)
option(SEN_DISABLE_EXTRA_COMPONENTS "Disable non-critical components" OFF)
option(SEN_COVERAGE_ENABLE "Enable coverage generation" OFF)
option(SEN_ENABLE_CMAKE_TARGET_GRAPH "Enable the generation of a graphviz with the targets" ON)
set(SEN_USE_SANITIZER
    "None"
    CACHE STRING "With sanitizer setup to enable"
)
set_property(
  CACHE SEN_USE_SANITIZER
  PROPERTY STRINGS
           None
           ASanUBSan
           Thread
)

# get access to our utility functions
include(util/sen_utils)
include(util/git_info)

# obtain the version
set(SEN_VERSION)
get_version_from_git(SEN_VERSION)

# project info
project(
  sen
  VERSION ${SEN_VERSION}
  LANGUAGES CXX C
  DESCRIPTION "A simple way for applications to talk to one another and create complex systems with ease."
)

include(util/sen_internal_utils)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.2.1)
  message(FATAL_ERROR "GCC ${CMAKE_CXX_COMPILER_VERSION} is not supported.")
endif()

if(MSVC)
  add_compile_options(/bigobj)
endif()

# configure coverage
if(${SEN_COVERAGE_ENABLE})
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-g -O0 -fno-inline)
    add_compile_options(--coverage)
    add_link_options(--coverage)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-g -O0 -fno-inline)
    file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/coverage_data/coverage-%m.profraw" SEN_COVERAGE_DATA_DIR)
    add_compile_options(-fprofile-instr-generate=${SEN_COVERAGE_DATA_DIR} -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate=${SEN_COVERAGE_DATA_DIR} -fcoverage-mapping)
  endif()
endif()

include(util/sanitizers)
include(util/test)

# libs
add_subdirectory(libs/core)
add_subdirectory(libs/kernel)
add_subdirectory(libs/db)
add_subdirectory(libs/db/bindings/python)
add_subdirectory(libs/util)

# apps
add_subdirectory(apps/cli_sen)
add_subdirectory(apps/cli_run)
add_subdirectory(apps/cli_gen)
add_subdirectory(apps/cli_archive)
add_subdirectory(apps/cli_package)
add_subdirectory(apps/cli_remote_shell)

# critical components
add_subdirectory(components/shell)
add_subdirectory(components/ether)

# non-critical components
if(NOT SEN_DISABLE_EXTRA_COMPONENTS)
  add_subdirectory(components/recorder)
  add_subdirectory(components/replayer)
  add_subdirectory(components/explorer)
  add_subdirectory(components/py)
  add_subdirectory(components/influx)
  add_subdirectory(components/logmaster)
  add_subdirectory(components/rest)
  add_subdirectory(components/tracy)
endif()

# examples
if(SEN_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# documentation
# Docs are not present when building with only the conan export_sources
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/docs")
  if(SEN_BUILD_DOCS)
    add_subdirectory(docs)
    add_subdirectory(docs/doxygen)
  endif()
endif()

if(SEN_BUILD_TESTS)
  add_subdirectory(test/util/chaos_monkeys)
  add_subdirectory(test/util/cmake_packages)
  add_subdirectory(test/util/dummy_entities)
  add_subdirectory(test/util/inheritance)
  add_subdirectory(test/util/my_package)
  add_subdirectory(test/util/stress_testing_utils)
  add_subdirectory(test/util/calls_on_removal)
  add_subdirectory(test/util/publish_types_manually)
endif()

# coverage
include(util/coverage)

# installation
include(util/install)
