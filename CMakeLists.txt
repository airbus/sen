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
option(SEN_BUILD_EXAMPLES "Build examples" ON)
option(SEN_BUILD_TESTS "Build tests" ON)
option(SEN_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(SEN_BUILD_DOCS "Build documentation" ON)
# Negative polarity is a wart vs the rest of the codebase (Conan `with_*` and
# `SEN_<COMP>_ENABLE_CLANG_TIDY` are positive). Conan's `with_clang_tidy` already
# normalizes this for most users; flipping at the CMake layer would break direct
# `-DSEN_DISABLE_CLANG_TIDY=...` callers, so we live with it.
option(SEN_DISABLE_CLANG_TIDY "Disable clang-tidy" ON)
option(SEN_COVERAGE_ENABLE "Enable coverage generation" OFF)
option(SEN_OPTIMIZED_DEBUG "Enable smaller and better optimized debug build" OFF)
# Off by default because it costs every Release build the size and time of carrying debug
# information. The release workflow sets it; nothing else needs to.
option(SEN_RELEASE_SYMBOLS "Build Release with debug information and split it out on install" OFF)
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
# Marks this as Sen's own build. sen_configure_target() is public through the installed cmake,
# so anything unconditional there reaches every consumer target; options Sen applies only to
# itself are gated on this. This file is not installed, so a consumer never defines it.
set(SEN_INTERNAL_BUILD YES)

include(util/sen_utils)
include(util/git_info)

# obtain the version
set(SEN_VERSION)
get_version_from_git(SEN_VERSION)

# MSVC keeps debug information in one program database per target directory, and every parallel
# cl.exe contends for it. /FS is meant to serialise that through mspdbsrv and does not when the
# compile runs under a compiler-cache launcher: building the test suite for RelWithDebInfo stops
# with C1041. Embedded puts the information in the objects instead, which is what Release already
# asks for with /Z7 and for the second reason as well -- a cache stores nothing against a program
# database. Debug keeps one: nothing builds it on MSVC, and it is the cheaper format to step in.
#
# Before project(), which is where the per-configuration flags are decided, and guarded because
# the policy arrived in 3.25 and the minimum above is 3.21.
if(POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
      "$<$<CONFIG:Debug>:ProgramDatabase>$<$<CONFIG:RelWithDebInfo>:Embedded>"
  )
endif()

# project info
project(
  sen
  VERSION ${SEN_VERSION}
  LANGUAGES CXX C
  DESCRIPTION "A simple way for applications to talk to one another and create complex systems with ease."
)

# Windows headers change structure layouts and which APIs exist by this macro, so
# every translation unit has to agree on it. Project-wide rather than per target,
# since asio assumes 0x0601 wherever nothing set it.
if(MSVC)
  add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()

# The build id ties a crash report to the binary that produced it. Set here because the linkers
# emit one by default and a default can change. MSVC and Apple use identifiers of their own.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_link_options(-Wl,--build-id=sha1)
endif()

include(util/sen_internal_utils)

# Both here rather than in the installed cmake: the cache belongs to Sen's own build, and the
# warning reads MSVC and the release flags, which only exist once project() has run.
sen_internal_enable_compiler_cache()
sen_warn_if_the_compiler_cache_cannot_work()

include(util/sanitizers)
include(util/test)
include(util/benchmark)

# libs
add_subdirectory(libs/core)
add_subdirectory(libs/kernel)
add_subdirectory(libs/db)
add_subdirectory(libs/util)
add_subdirectory(libs/gen)

# components
add_subdirectory(components/shell)
add_subdirectory(components/term)
add_subdirectory(components/ether)
add_subdirectory(components/recorder)
add_subdirectory(components/replayer)
add_subdirectory(components/explorer)
add_subdirectory(components/py)
add_subdirectory(components/influx)
add_subdirectory(components/logmaster)
add_subdirectory(components/rest)
add_subdirectory(components/jsonrpc)
add_subdirectory(components/tracy)
add_subdirectory(components/webexplorer)

# Python bindings under libs/db. Not a Sen "component" (no conanfile.py COMPONENTS entry,
# no sen_internal_component_guard). Just a Python module that exposes sen::db; piggybacks
# on the existing SEN_BUILD_PY toggle rather than declaring its own option.
if(SEN_BUILD_PY)
  add_subdirectory(libs/db/bindings/python)
endif()

# apps
add_subdirectory(apps/cli_sen)
add_subdirectory(apps/cli_run)
add_subdirectory(apps/cli_gen)
add_subdirectory(apps/cli_archive)
add_subdirectory(apps/cli_package)
add_subdirectory(apps/cli_remote_shell)
add_subdirectory(apps/mcp_gateway)

# examples
if(SEN_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

# documentation
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/docs")
  if(SEN_BUILD_DOCS)
    add_subdirectory(docs)
    add_subdirectory(docs/doxygen)
  endif()
endif()

if(SEN_BUILD_TESTS)
  add_subdirectory(resources/installer/test)
  add_subdirectory(test/util/chaos_monkeys)
  add_subdirectory(test/util/cmake_packages)
  add_subdirectory(test/util/cmake_utils)
  add_subdirectory(test/util/dummy_entities)
  add_subdirectory(test/util/fom_extension)
  add_subdirectory(test/util/inheritance)
  add_subdirectory(test/util/my_package)
  add_subdirectory(test/util/stress_testing_utils)
  add_subdirectory(test/util/calls_on_removal)
  add_subdirectory(test/util/publish_types_manually)
  add_subdirectory(test/util/query_test)
  add_subdirectory(test/util/log_emitter)
  add_subdirectory(test/support)
endif()

# every declared component must have registered itself; see the function's comment
sen_internal_check_components_registered()

# coverage
include(util/coverage)

# installation
include(util/install)
