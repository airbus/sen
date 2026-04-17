# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

set(dr_tests
    coordinates_test.cpp
    dead_reckoner_test.cpp
    mat3_test.cpp
    quat_test.cpp
    utils/kernel_fixture.cpp
    utils/kernel_fixture.h
    utils_test.cpp
)

add_sen_unit_test_suite(
  util_test
  ${dr_tests}
  LINK_DEPS
  sen::core
  sen::kernel
  sen::util
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12.0)
    target_compile_options(util_test PRIVATE -Wno-attributes # TODO(SEN-1558): remove after code gen rework
    )
  endif()
endif()

target_include_directories(
  util_test SYSTEM
  INTERFACE
  PRIVATE ${CMAKE_SOURCE_DIR}/libs/util/include ${CMAKE_SOURCE_DIR}/libs/util/src/dr
)

set_target_properties(
  util_test PROPERTIES FOLDER "test" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)
