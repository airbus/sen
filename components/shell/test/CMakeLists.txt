# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

set(shell_service_tests shell_service/printer_test.cpp shell_service/shell_test.cpp)

set(shell_sources
    ${CMAKE_SOURCE_DIR}/components/shell/src/shell_service/printer.cpp
    ${CMAKE_SOURCE_DIR}/components/shell/src/shell_service/shell.cpp
    ${CMAKE_SOURCE_DIR}/components/shell/src/shell_service/completer.cpp
    ${CMAKE_SOURCE_DIR}/components/shell/src/shell_service/line_edit.cpp
    ${CMAKE_SOURCE_DIR}/components/shell/src/shell_service/terminal_tree.cpp
    ${CMAKE_SOURCE_DIR}/components/shell/src/shell_service/history.cpp
    ${CMAKE_SOURCE_DIR}/libs/util/test/utils/kernel_fixture.cpp
)

add_library(shell_test_generated OBJECT)
sen_generate_cpp(TARGET shell_test_generated STL_FILES ${CMAKE_SOURCE_DIR}/components/shell/stl/shell.stl)

add_sen_unit_test_suite(
  shell_test
  ${shell_service_tests}
  ${shell_sources}
  LINK_DEPS
  terminal_lib
  sen::core
  sen::kernel
  PRIVATE
  shell_test_generated
)

target_include_directories(
  shell_test
  PRIVATE ${CMAKE_SOURCE_DIR}/components/shell/src
          ${CMAKE_SOURCE_DIR}/components/shell/src/shell_service
          ${CMAKE_SOURCE_DIR}/components/shell/src/terminal_lib
          ${CMAKE_SOURCE_DIR}/libs/util/test/utils
          $<TARGET_PROPERTY:shell_test_generated,BINARY_DIR>
)

set_target_properties(
  shell_test PROPERTIES FOLDER "test" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

sen_enable_static_analysis(shell_test)
