# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/concurrentqueue)
include(tps/json)
include(tps/uwebsockets)

# asio is the only consumer of `ws_client.{h,cpp}` and `ws_server_test.cpp`, the loopback
# WebSocket client and integration test that exercise the real uWS path. We look for it without
# REQUIRED so this component stays buildable in a future extracted repo that doesn't ship asio.
# The dispatcher and method tests don't need it. The production component does not link asio.
find_package(asio QUIET)

set(jsonrpc_test_sources
    dispatcher_fixture.cpp
    dispatcher_test.cpp
    lifecycle_test.cpp
    loaded_bundle_test.cpp
    multi_connection_test.cpp
    rpc_methods_test.cpp
    static_file_server_test.cpp
    subscribe_test.cpp
    util_test.cpp
)

set(jsonrpc_test_deps
    sen::core
    sen::kernel
    sen::gen
    concurrentqueue::concurrentqueue
    nlohmann_json::nlohmann_json
    uwebsockets::uwebsockets
    jsonrpc_for_testing
)

if(asio_FOUND)
  list(
    APPEND
    jsonrpc_test_sources
    static_file_routes_test.cpp
    ws_server_test.cpp
    ws_client.cpp
  )
  list(APPEND jsonrpc_test_deps asio::asio)
else()
  message(
    STATUS "jsonrpc: asio not found, skipping WebSocketServer loopback tests (ws_client + ws_server_test)"
  )
endif()

add_sen_unit_test_suite(
  jsonrpc_test
  ${jsonrpc_test_sources}
  LINK_DEPS
  ${jsonrpc_test_deps}
  # When built, ws_server_test (ephemeral ports) and lifecycle_test's loadsViaTestKernel
  # (port 9090) both bind loopback sockets. gtest_discover_tests turns each gtest into a separate
  # ctest case, so under ctest -j two of them could race on the same port. The lock serializes
  # the suite. Cheap to keep on even when the loopback tests are skipped.
  EXTRA_PROPERTIES
  RESOURCE_LOCK
  jsonrpc_network
)

# Generate the fixture_widget.stl class into the test target. DispatcherFixture publishes an
# instance on `local.fixture` so the integration tests have a real kernel object with a writable
# property (`counter`) and a fireable event (`chimed`) to exercise getProperty / setProperty /
# subscribeProperty / subscribeEvent end-to-end.
sen_generate_cpp(TARGET jsonrpc_test STL_FILES stl/fixture_widget.stl)

if(asio_FOUND)
  target_include_directories(jsonrpc_test SYSTEM PRIVATE asio::asio)
endif()
target_include_directories(jsonrpc_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

set_target_properties(jsonrpc_test PROPERTIES FOLDER "components/jsonrpc/test")
