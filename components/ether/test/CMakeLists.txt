# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_unit_test_suite(
  ether_test
  network_exclusion_test.cpp
  LINK_DEPS
  sen::core
  asio::asio
  ether_for_testing
)

target_include_directories(ether_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

if(WIN32)
  target_link_libraries(ether_test PRIVATE ole32 oleaut32 wbemuuid)
endif()

set_target_properties(
  ether_test PROPERTIES FOLDER "test" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)
