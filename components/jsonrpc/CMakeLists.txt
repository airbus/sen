# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

sen_internal_component_guard(NAME jsonrpc)

include(tps/concurrentqueue)
include(tps/json)
include(tps/spdlog)
include(tps/uwebsockets)

set(base_sources
    src/component.cpp
    src/dispatcher.cpp
    src/loaded_bundle.cpp
    src/server.cpp
    src/server_registry.cpp
    src/static_file_server.cpp
    src/subscriptions.cpp
    src/topology_service.cpp
    src/util.cpp
    src/ws_server.cpp
)

add_sen_package(
  TARGET jsonrpc
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "JSON-RPC 2.0 over WebSocket external API for sen"
  SOURCES ${base_sources}
  STL_FILES stl/configuration.stl stl/jsonrpc.stl stl/static_file_server.stl
  PRIVATE_DEPS
    sen::kernel
    sen::gen
    concurrentqueue::concurrentqueue
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    uwebsockets::uwebsockets
  BASE_PATH ${CMAKE_CURRENT_SOURCE_DIR}
  TEST_TARGET jsonrpc_for_testing
  IS_COMPONENT PUBLIC_SYMBOLS
)

sen_enable_static_analysis(jsonrpc)

# Run clang-tidy only on this component's own headers, not on transitively-included
# upstream Sen headers. Independent of the project-wide SEN_DISABLE_CLANG_TIDY.
option(SEN_JSONRPC_ENABLE_CLANG_TIDY "Enable clang-tidy on the jsonrpc component" OFF)
if(SEN_JSONRPC_ENABLE_CLANG_TIDY)
  find_program(_jsonrpc_clang_tidy NAMES clang-tidy)
  if(_jsonrpc_clang_tidy)
    set_property(
      TARGET jsonrpc
      PROPERTY CXX_CLANG_TIDY
               "${_jsonrpc_clang_tidy};-extra-arg=-std=c++17;-header-filter=.*/components/jsonrpc/.*"
    )
  else()
    message(WARNING "SEN_JSONRPC_ENABLE_CLANG_TIDY=ON but clang-tidy not found; skipping")
  endif()
endif()

set_target_properties(jsonrpc PROPERTIES FOLDER "components/jsonrpc")

sen_internal_install(jsonrpc)

install(FILES ${PROJECT_SOURCE_DIR}/schemas/jsonrpc.json DESTINATION schemas)

add_subdirectory(clients/typescript)

if(SEN_BUILD_TESTS)
  add_subdirectory(test)
endif()
