# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/cli11)

add_executable(cli_archive cli_archive_setup.cpp main.cpp)

target_link_libraries(
  cli_archive
  PUBLIC sen::core sen::db
  PRIVATE $<BUILD_INTERFACE:CLI11::CLI11>
)

add_executable(sen::cli_archive ALIAS cli_archive)
sen_internal_configure_app(cli_archive)
sen_internal_install(cli_archive)

target_include_directories(
  cli_archive
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/cli_archive
)

sen_enable_static_analysis(cli_archive)

if(SEN_BUILD_TESTS)
  add_library(cli_archive_for_testing STATIC cli_archive_setup.cpp)
  sen_configure_target(cli_archive_for_testing)
  target_link_libraries(
    cli_archive_for_testing
    PUBLIC sen::core sen::db
    PRIVATE $<BUILD_INTERFACE:CLI11::CLI11>
  )
  target_include_directories(
    cli_archive_for_testing
    PRIVATE CLI11::CLI11
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/cli_archive
  )
  add_subdirectory(test)
endif()
