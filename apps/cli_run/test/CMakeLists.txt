# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# cli_run is the binary every user starts, and it was the worst covered thing in the tree that is not
# deprecated. Like cli_archive, all of it is in an anonymous namespace inside the executable, so the only
# way in is to run it and read what it printed. Every expectation below is a string from main.cpp rather
# than a guess at one.
#
# --bus, --bus-file and --format all carry needs(networkFootprintOption), so each appears together with
# --print-network-footprint; on its own any of them is a usage error.

# browser_open.cpp is the one part of cli_run that is not in an anonymous namespace, so it can be
# compiled into a test and called rather than driven through the binary. It builds only with the
# web explorer, which is the only thing that opens a browser.
if(SEN_BUILD_WEBEXPLORER)
  add_sen_unit_test_suite(
    cli_run_browser_open_test browser_open_test.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../browser_open.cpp
  )
  target_include_directories(cli_run_browser_open_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
  if(WIN32)
    # This target compiles browser_open.cpp itself rather than linking cli_run, so it needs the
    # Winsock import library that cli_run links for the same source.
    target_link_libraries(cli_run_browser_open_test PRIVATE ws2_32)
  endif()
endif()

set(_config ${CMAKE_CURRENT_SOURCE_DIR}/config/with_ether.yaml)
set(_bare_config ${CMAKE_CURRENT_SOURCE_DIR}/config/no_components.yaml)
set(_bus_file ${CMAKE_CURRENT_SOURCE_DIR}/config/buses.txt)

add_sen_smoke_test(
  cli_run_help_lists_the_footprint_options DESCRIPTION "Lists the network-footprint options in the help text."
  COMMAND sen::cli_run --help
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_run
)
set_tests_properties(
  cli_run_help_lists_the_footprint_options PROPERTIES PASS_REGULAR_EXPRESSION "--print-network-footprint"
)

add_sen_smoke_test(
  cli_run_refuses_a_preset_it_does_not_have DESCRIPTION
  "Refuses an unknown preset and names the presets that exist."
  COMMAND sen::cli_run --preset nosuchpreset
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_run
)
set_tests_properties(
  # CLI11 checks IsMember on --preset, so makeBootloader's own "invalid preset" throw is unreachable
  # from the command line. What a user sees is this, which names the presets that do exist.
  cli_run_refuses_a_preset_it_does_not_have
  PROPERTIES PASS_REGULAR_EXPRESSION "not in [{]shell,term,replay,explorer,web-explorer[}]"
)

add_sen_smoke_test(
  cli_run_prints_the_network_footprint DESCRIPTION "Prints the ports and buses a configuration would use."
  COMMAND sen::cli_run ${_config} --print-network-footprint
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_run
)
set_tests_properties(
  cli_run_prints_the_network_footprint PROPERTIES PASS_REGULAR_EXPRESSION "For ephemeral ports"
)

# json and text cannot both be right: the json branch returns the serialised object alone, so the
# sentence the text branch appends must be absent. Asserting both ways is what tells them apart.
add_sen_smoke_test(
  cli_run_prints_the_footprint_as_json DESCRIPTION
  "Prints the footprint as JSON, with none of the prose form in it."
  COMMAND sen::cli_run ${_config} --print-network-footprint --format json
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_run
)
set_tests_properties(
  cli_run_prints_the_footprint_as_json
  PROPERTIES PASS_REGULAR_EXPRESSION
             "[{]"
             FAIL_REGULAR_EXPRESSION
             "For ephemeral ports"
)

add_sen_smoke_test(
  cli_run_reads_buses_from_a_file DESCRIPTION "Takes the buses to report from a file."
  COMMAND sen::cli_run ${_config} --print-network-footprint --bus-file ${_bus_file}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_run
)
# The report prints a bus as two fields rather than as one name, so the needle is the field.
set_tests_properties(cli_run_reads_buses_from_a_file PROPERTIES PASS_REGULAR_EXPRESSION "busName: +demo")

add_sen_smoke_test(
  cli_run_takes_a_bus_on_the_command_line DESCRIPTION
  "Takes a bus from the command line and reports it as supplied rather than configured."
  COMMAND sen::cli_run ${_config} --print-network-footprint --bus remote.demo
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_run
)
set_tests_properties(
  # "supplied" is the source a bus from --bus or --bus-file carries, as against a configured one.
  cli_run_takes_a_bus_on_the_command_line PROPERTIES PASS_REGULAR_EXPRESSION "source: +supplied"
)

# CLI11 checks ExistingFile on --bus-file, so a path that is not there is refused before readBusFile is
# reached. That leaves readBusFile's own "could not open bus file" unreachable from the command line.
add_sen_smoke_test(
  cli_run_refuses_a_bus_file_that_is_not_there DESCRIPTION "Refuses a bus file that does not exist."
  COMMAND sen::cli_run ${_config} --print-network-footprint --bus-file
          ${CMAKE_CURRENT_BINARY_DIR}/no_such_buses.txt
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_run
)
set_tests_properties(cli_run_refuses_a_bus_file_that_is_not_there PROPERTIES WILL_FAIL TRUE)

add_sen_smoke_test(
  cli_run_refuses_a_format_it_does_not_know DESCRIPTION "Refuses an output format it does not know."
  COMMAND sen::cli_run ${_config} --print-network-footprint --format yaml
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_run
)
set_tests_properties(cli_run_refuses_a_format_it_does_not_know PROPERTIES WILL_FAIL TRUE)

add_sen_smoke_test(
  cli_run_refuses_a_config_that_is_not_there DESCRIPTION "Refuses a configuration file that does not exist."
  COMMAND sen::cli_run ${CMAKE_CURRENT_BINARY_DIR}/no_such_config.yaml
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_run
)
set_tests_properties(cli_run_refuses_a_config_that_is_not_there PROPERTIES WILL_FAIL TRUE)

add_sen_smoke_test(
  cli_run_refuses_a_local_bus_in_the_footprint DESCRIPTION
  "Refuses to report a network footprint for a local bus, which uses none."
  COMMAND sen::cli_run ${_config} --print-network-footprint --bus local.demo
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_run
)
set_tests_properties(
  cli_run_refuses_a_local_bus_in_the_footprint PROPERTIES PASS_REGULAR_EXPRESSION
                                                          "local buses have no network footprint"
)

# Each preset is a configuration compiled into the binary, and nothing read any of them back. A
# preset that stopped parsing would ship broken and be found by a user. Printing the footprint
# loads the configuration and starts nothing, so this holds for the ones that want a terminal too.
foreach(
  _preset
  shell
  term
  replay
  explorer
  web-explorer
)
  add_sen_smoke_test(
    cli_run_loads_the_${_preset}_preset DESCRIPTION
    "Loads the ${_preset} preset compiled into the binary, and reports the ports it would use."
    COMMAND sen::cli_run --preset ${_preset} --print-network-footprint
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_run
  )
  set_tests_properties(
    cli_run_loads_the_${_preset}_preset PROPERTIES PASS_REGULAR_EXPRESSION "mode: +ephemeral"
  )
  if(WIN32)
    append_test_env_modification(
      cli_run_loads_the_${_preset}_preset "PATH=path_list_prepend:$<TARGET_FILE_DIR:sen::core>"
    )
  else()
    append_test_env_modification(
      cli_run_loads_the_${_preset}_preset "LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:sen::core>"
    )
  endif()
endforeach()

# A bus file the kernel wrote on Windows starts with a byte order mark, and the reader strips it.
# Without that the first bus name carries three invisible bytes and matches nothing.
string(
  ASCII 239
        187
        191
        _utf8_bom
)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/buses_with_bom.txt "${_utf8_bom}remote.demo\n")

add_sen_smoke_test(
  cli_run_reads_a_bus_file_with_a_byte_order_mark DESCRIPTION
  "Reads a bus file that starts with a byte order mark, which a Windows editor leaves there."
  COMMAND sen::cli_run ${_config} --print-network-footprint --bus-file
          ${CMAKE_CURRENT_BINARY_DIR}/buses_with_bom.txt
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_run
)
set_tests_properties(
  cli_run_reads_a_bus_file_with_a_byte_order_mark PROPERTIES PASS_REGULAR_EXPRESSION "busName: +demo"
)

# The ordinary path: a kernel that starts and stops. No expectation on the output, because the point is
# the exit status.
add_sen_smoke_test(
  cli_run_starts_and_stops_a_kernel_with_nothing_in_it DESCRIPTION
  "Starts and stops a kernel configured with no components."
  COMMAND sen::cli_run ${_bare_config} --start-stop
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_run
)

foreach(
  _test
  cli_run_help_lists_the_footprint_options
  cli_run_refuses_a_preset_it_does_not_have
  cli_run_prints_the_network_footprint
  cli_run_prints_the_footprint_as_json
  cli_run_reads_buses_from_a_file
  cli_run_takes_a_bus_on_the_command_line
  cli_run_refuses_a_bus_file_that_is_not_there
  cli_run_refuses_a_format_it_does_not_know
  cli_run_refuses_a_config_that_is_not_there
  cli_run_refuses_a_local_bus_in_the_footprint
  cli_run_starts_and_stops_a_kernel_with_nothing_in_it
  cli_run_reads_a_bus_file_with_a_byte_order_mark
)
  if(WIN32)
    append_test_env_modification(${_test} "PATH=path_list_prepend:$<TARGET_FILE_DIR:sen::core>")
  else()
    append_test_env_modification(${_test} "LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:sen::core>")
  endif()
endforeach()
