# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# The level mapping is a pair of switches with one arm per level, and a component run only ever
# takes the arm its loggers happen to be on. Called directly, every arm is checked.
add_sen_unit_test_suite(
  logmaster_test
  logmaster_test.cpp
  LINK_DEPS
  sen::core
  sen::kernel
  logmaster_for_testing
  spdlog::spdlog
)
target_include_directories(logmaster_test PRIVATE ${CMAKE_SOURCE_DIR}/components/logmaster/src)

# The example configs cannot carry this one, because every config under examples/config loads
# the term.
add_sen_run_smoke_test(
  logmaster_smoke DESCRIPTION "Publishes a LogMaster, scans once a cycle for loggers, and stops."
  CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/config/logmaster.yaml
)

# Calling mute or setLevel needs something to call them. A terminal is one way in and a Python
# module is another, and the second needs no screen.
add_sen_integration_test(
  logmaster_control_test
  DESCRIPTION
  "Mutes and unmutes every logger at once, and sets one logger's level and pattern."
  COMMAND
  ./cli_run
  ${CMAKE_CURRENT_SOURCE_DIR}/config/logmaster_control.yaml
  WORKING_DIRECTORY $<TARGET_FILE_DIR:sen::cli_run>
  REQ_DEPS cli_run
)

append_test_env_modification(
  logmaster_control_test "PYTHONPATH=path_list_append:${CMAKE_CURRENT_SOURCE_DIR}/src"
)
