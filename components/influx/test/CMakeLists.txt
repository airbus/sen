# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

set(influx_tests influx_test.cpp)

add_sen_unit_test_suite(
  influx_test
  ${influx_tests}
  LINK_DEPS
  sen::core
  influx_for_testing
)

# The component's headers are private to it, and the line protocol is only reachable through them.
target_include_directories(influx_test PRIVATE ${CMAKE_SOURCE_DIR}/components/influx/src)

set_target_properties(
  influx_test PROPERTIES FOLDER "test" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

# The unit test above reaches the line protocol through the database. Nothing ran the component
# itself, its recorder, or the transports. A UDP target needs no server, so a configuration that
# points at a loopback port exercises all three, and an object carrying every value shape makes
# the conversion visit each of them.
add_sen_integration_test(
  influx_records_to_udp_test
  DESCRIPTION
  "Records an object of every value shape to a UDP target, through the component's own recorder."
  COMMAND
  ./cli_run
  ${CMAKE_CURRENT_SOURCE_DIR}/config/record_to_udp.yaml
  WORKING_DIRECTORY $<TARGET_FILE_DIR:sen::cli_run>
  REQ_DEPS cli_run
)

append_test_env_modification(
  influx_records_to_udp_test "PYTHONPATH=path_list_append:${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# Driven from a test rather than from a configuration: the component connects while it loads, so
# the server has to exist before the kernel starts.
#
# Not on Windows, for the reason libs/kernel/test/CMakeLists.txt gives about its own integration
# suites: objects never reach the bus there, so anything waiting for a recorded one times out.
# pytest is looked for here rather than inherited. The directories that look for it sit inside that
# same guard, so on Windows PYTEST_EXEC did not exist, this registered with the bare script as its
# command, and ctest refused to start it at all.
if(NOT MSVC)
  find_program(PYTEST_EXEC pytest REQUIRED)

  add_sen_integration_test(
    influx_records_to_tcp_test
    DESCRIPTION
    "Records to a TCP server the test provides, batching the points before sending them."
    COMMAND
    ${PYTEST_EXEC}
    ${CMAKE_CURRENT_SOURCE_DIR}/influx_tcp_tester.py
    WORKING_DIRECTORY $<TARGET_FILE_DIR:sen::cli_run>
    REQ_DEPS cli_run
  )

  append_test_env_modification(
    influx_records_to_tcp_test "PYTHONPATH=path_list_append:${CMAKE_CURRENT_SOURCE_DIR}/src"
  )
  set_tests_properties(influx_records_to_tcp_test PROPERTIES RUN_SERIAL TRUE)
endif()

# A selection the component cannot read used to reach std::terminate, so a typo in a configuration
# file produced a crash report. The component names it now and hands the error back. The kernel
# still ends the process abruptly while tearing down after that, which is why this asserts the
# message rather than the exit status.
add_test(
  NAME influx_names_a_selection_it_cannot_read
  COMMAND
    ${CMAKE_COMMAND} -DEXECUTABLE=$<TARGET_FILE:sen::cli_run>
    -DCONFIG=${CMAKE_CURRENT_SOURCE_DIR}/config/bad_selection.yaml -DEXPECT=Expected.FROM.keyword -P
    ${CMAKE_CURRENT_SOURCE_DIR}/expect_output.cmake
)
record_test_description(
  influx_names_a_selection_it_cannot_read
  "Names the selection it cannot read instead of ending the run without saying why."
)
set_tests_properties(influx_names_a_selection_it_cannot_read PROPERTIES LABELS smoke)
# A raw add_test does not arm the sanitizer options the add_sen_* helpers do, and this one drives
# the instrumented binary, so without them a finding here would reach nobody.
add_sanitizer_options(influx_names_a_selection_it_cannot_read)

# Two configurations a user writes before a complete one: the first leaves the sampling period out,
# the second has nothing to record yet. Both have to start and stop rather than fail.
add_sen_run_smoke_test(
  influx_takes_its_default_sampling_period DESCRIPTION
  "Starts with no sampling period configured and uses its own default."
  CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/config/default_period.yaml
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_sen_run_smoke_test(
  influx_warns_when_nothing_is_selected DESCRIPTION
  "Says that no data will be sent when the configuration selects nothing."
  CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/config/no_selections.yaml
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
set_tests_properties(
  influx_warns_when_nothing_is_selected PROPERTIES PASS_REGULAR_EXPRESSION "no data will be sent to Telegraf"
)
