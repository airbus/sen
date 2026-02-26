# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
set(chaos_monkeys ${CMAKE_SOURCE_DIR}/test/util/chaos_monkeys)

include(tps/spdlog)

add_sen_package(
  TARGET creator_monkeys
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Creator Monkey package to generate stress tests in Sen"
  SOURCES ${CMAKE_SOURCE_DIR}/test/util/chaos_monkeys/src/creator_monkey.cpp
          ${CMAKE_SOURCE_DIR}/test/util/chaos_monkeys/src/creator_monkey.h
  STL_FILES ${CMAKE_SOURCE_DIR}/test/util/chaos_monkeys/stl/creator_monkeys/chaos_monkeys.stl
  BASE_PATH ${CMAKE_SOURCE_DIR}/test/util/chaos_monkeys
  EXPORT_NAME creator_monkeys
  NO_SCHEMA
  PRIVATE_DEPS spdlog::spdlog
)

set_target_properties(creator_monkeys PROPERTIES FOLDER "test/util/chaos_monkeys")
set_target_properties(creator_monkeys PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# TODO(SEN-1165): investigate flaky results for these tests
# # execute creator monkeys test with one type of object with a frequency of 10hz during 10 seconds
# add_sen_run_smoke_test(
#   chaos_monkey1 NO_START_STOP CONFIG_FILE ${CMAKE_SOURCE_DIR}/test/util/chaos_monkeys/test/monkey_1.yaml
# )
#
# # execute creator monkeys test with 2 types of object with a frequency of 30hz during 20 seconds
# add_sen_run_smoke_test(
#   chaos_monkey2 NO_START_STOP CONFIG_FILE ${CMAKE_SOURCE_DIR}/test/util/chaos_monkeys/test/monkey_2.yaml
# )
#
# # execute creator monkeys test with 3 type of object with a frequency of 100hz during 30 seconds
# add_sen_run_smoke_test(
#   chaos_monkey3 NO_START_STOP CONFIG_FILE ${CMAKE_SOURCE_DIR}/test/util/chaos_monkeys/test/monkey_3.yaml
# )
#
# # execute creator monkeys test with 3 type of object with a frequency of 60hz during 30 seconds
# add_sen_run_smoke_test(
#   chaos_monkey4 NO_START_STOP CONFIG_FILE ${CMAKE_SOURCE_DIR}/test/util/chaos_monkeys/test/monkey_4.yaml
# )
