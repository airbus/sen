# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_unit_test_suite(
  ether_test
  bus_handler_test.cpp
  ether_transport_test.cpp
  network_exclusion_test.cpp
  network_footprint_test.cpp
  beam_framing_test.cpp
  port_binding_test.cpp
  LINK_DEPS
  sen::core
  asio::asio
  concurrentqueue::concurrentqueue
  spdlog::spdlog
  ether_for_testing
)

target_include_directories(ether_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

if(WIN32)
  target_link_libraries(ether_test PRIVATE ole32 oleaut32 wbemuuid)
endif()

set_target_properties(
  ether_test PROPERTIES FOLDER "test" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

# Two environment variables override what the configuration says, and each logs a warning so that a run
# which behaves unexpectedly says why in its own output. Nothing set them, so neither the parsing nor
# the warnings had ever run: isEqualsIgnoreCase, which exists only for these, read as uncovered.
#
# The accepted spellings are true/yes/a non-zero number against false/no/zero, so each test uses a
# different spelling from its pair.
set(_ether_config ${CMAKE_CURRENT_SOURCE_DIR}/config/ether.yaml)

add_sen_run_smoke_test(
  ether_env_disables_bus_multicast DESCRIPTION
  "Turns off multicast bus communication from the environment, and warns that it did."
  CONFIG_FILE ${_ether_config}
)
append_test_env_modification(ether_env_disables_bus_multicast "SEN_ETHER_DISABLE_BUS_MULTICAST=set:true")
set_tests_properties(
  ether_env_disables_bus_multicast
  PROPERTIES PASS_REGULAR_EXPRESSION "disallowing multicast bus communication via environment variable"
)

add_sen_run_smoke_test(
  ether_env_allows_bus_multicast DESCRIPTION
  "Leaves multicast bus communication on from the environment, and warns that it did."
  CONFIG_FILE ${_ether_config}
)
append_test_env_modification(ether_env_allows_bus_multicast "SEN_ETHER_DISABLE_BUS_MULTICAST=set:no")
set_tests_properties(
  ether_env_allows_bus_multicast PROPERTIES PASS_REGULAR_EXPRESSION
                                            "allowing multicast bus communication via environment variable"
)

add_sen_run_smoke_test(
  ether_env_allows_virtual_interfaces DESCRIPTION
  "Allows virtual interfaces from the environment, and warns that it did." CONFIG_FILE ${_ether_config}
)
append_test_env_modification(ether_env_allows_virtual_interfaces "SEN_ETHER_ALLOW_VIRTUAL_INTERFACES=set:1")
set_tests_properties(
  ether_env_allows_virtual_interfaces PROPERTIES PASS_REGULAR_EXPRESSION
                                                 "allowing virtual interfaces via environment variable"
)

add_sen_run_smoke_test(
  ether_env_disallows_virtual_interfaces DESCRIPTION
  "Disallows virtual interfaces from the environment, and warns that it did." CONFIG_FILE ${_ether_config}
)
append_test_env_modification(
  ether_env_disallows_virtual_interfaces "SEN_ETHER_ALLOW_VIRTUAL_INTERFACES=set:false"
)
set_tests_properties(
  ether_env_disallows_virtual_interfaces PROPERTIES PASS_REGULAR_EXPRESSION
                                                    "disallowing virtual interfaces via environment variable"
)
