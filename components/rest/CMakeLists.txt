# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/asio)
include(tps/cpptrace)
include(tps/spdlog)
include(tps/json)
include(tps/llhttp)

# avoid compiling this component if llhttp not found
if(NOT llhttp_VERSION)
  message("llhttp not found. Disabling the rest component.")
  return()
endif()

find_program(SWAGGER_CLI swagger-cli)
if(NOT SWAGGER_CLI)
  message(
    STATUS
      "validate-openapi target skipped: swagger-cli not found. Install it with 'npm install -g swagger-cli' if you need to validate the OpenAPI spec."
  )
else()
  add_custom_target(
    validate-openapi
    COMMAND ${SWAGGER_CLI} validate openapi.yaml
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/docs/components"
    COMMENT "Validating OpenAPI spec..."
    VERBATIM
  )
endif()

set(base_sources
    src/component.cpp
    src/http_server.cpp
    src/http_session.cpp
    src/http_response.cpp
    src/http_request.cpp
    src/http_header.cpp
    src/client_session.cpp
    src/base_router.cpp
    src/sen_router.cpp
    src/types.cpp
    src/object_invokes_manager.cpp
    src/object_interests_manager.cpp
    src/object_members_manager.cpp
    src/notifications.cpp
    src/utils.cpp
    src/response_adapter.cpp
    src/locators.cpp
    src/jwt.cpp
)

add_sen_component(
  TARGET rest
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "HTTP server providing a REST API to interact with Sen resources"
  SOURCES ${base_sources}
  STL_FILES stl/configuration.stl stl/options.stl stl/types.stl
  DEPS asio::asio
       llhttp::llhttp
       nlohmann_json::nlohmann_json
       spdlog::spdlog
  BASE_PATH ${CMAKE_CURRENT_SOURCE_DIR}
  TEST_TARGET rest_for_testing
)

set_target_properties(rest PROPERTIES FOLDER "components/rest")
sen_internal_install(rest)
sen_enable_static_analysis(rest)

if(SEN_BUILD_TESTS)
  add_subdirectory(test)
endif()
