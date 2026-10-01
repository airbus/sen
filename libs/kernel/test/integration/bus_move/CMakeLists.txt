# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

find_package(Python3 REQUIRED COMPONENTS Interpreter)

include(tps/spdlog)

add_sen_package(
  TARGET bus_move
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Moving an object between two buses, seen from another process"
  SOURCES src/bus_move.h src/bus_move.cpp
  STL_FILES stl/bus_move.stl
  EXPORT_NAME sen.test.bus_move
  PRIVATE_DEPS spdlog::spdlog
  NO_SCHEMA
)
sen_enable_static_analysis(bus_move)
set_target_properties(bus_move PROPERTIES FOLDER "test")

# An object moved between two buses in one cycle, watched from another process through one mux.
# The watcher says what it heard; the verdict is that line, because the thing under test is what
# an observer is told rather than whether anything exited non-zero.
add_sen_integration_test(
  kernel_bus_move_test
  COMMAND
  ${Python3_EXECUTABLE}
  ${CMAKE_CURRENT_SOURCE_DIR}/../runner.py
  ${CMAKE_CURRENT_SOURCE_DIR}/config/mover.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/config/watcher.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/bus_move_tester.yaml
  WORKING_DIRECTORY $<TARGET_FILE_DIR:sen::cli_run>
  REQ_COMPONENTS ether py REQ_DEPS bus_move
)

append_test_env_modification(
  kernel_bus_move_test "PYTHONPATH=path_list_append:${CMAKE_CURRENT_SOURCE_DIR}"
  "PYTHONPATH=path_list_append:${CMAKE_CURRENT_SOURCE_DIR}/.."
)
set_tests_properties(kernel_bus_move_test PROPERTIES RUN_SERIAL TRUE)

# Both halves matter. Without the pass expression a run where the watcher never speaks is green,
# and without the fail expression a run that speaks and says the wrong thing is too.
set_tests_properties(kernel_bus_move_test PROPERTIES PASS_REGULAR_EXPRESSION "BUS MOVE OK")
set_tests_properties(kernel_bus_move_test PROPERTIES FAIL_REGULAR_EXPRESSION "BUS MOVE FAIL")
