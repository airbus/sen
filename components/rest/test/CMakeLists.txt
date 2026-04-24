# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/asio)
include(tps/cpptrace)
include(tps/spdlog)
include(tps/llhttp)

set(rest_test_deps
    sen::core
    sen::kernel
    asio::asio
    cpptrace::cpptrace
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    rest_for_testing
    llhttp::llhttp
)

add_sen_unit_test_suite(
  rest_test
  config_test.cpp
  router_test.cpp
  http_response_test.cpp
  notifications_test.cpp
  locators_test.cpp
  jwt_test.cpp
  server_test.cpp
  request.cpp
  LINK_DEPS
  ${rest_test_deps}
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12.0)
    target_compile_options(rest_test PRIVATE -Wno-attributes # TODO(SEN-1558): remove after code gen rework
    )
  endif()
endif()

target_include_directories(
  rest_test SYSTEM
  INTERFACE
  PRIVATE asio::asio nlohmann_json::nlohmann_json ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_sen_unit_test_suite(
  rest_flaky_test
  e2e_test.cpp
  request.cpp
  LINK_DEPS
  ${rest_test_deps}
  FLAKY
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12.0)
    target_compile_options(
      rest_flaky_test PRIVATE -Wno-attributes # TODO(SEN-1558): remove after code gen rework
    )
  endif()
endif()

target_include_directories(
  rest_flaky_test SYSTEM
  INTERFACE
  PRIVATE asio::asio nlohmann_json::nlohmann_json ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

sen_enable_static_analysis(rest_test)
