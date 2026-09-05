# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/cli11)
include(tps/json)

set(cli_gen_sources
    cli_input.cpp
    cli_input.h
    io.cpp
    io.h
    cpp_cli.cpp
    html_cli.cpp
    json_cli.cpp
    mkdocs_cli.cpp
    plantuml_cli.cpp
    python_cli.cpp
    typescript_cli.cpp
    main.cpp
)

add_executable(cli_gen ${cli_gen_sources})

sen_enable_static_analysis(cli_gen)

target_link_libraries(
  cli_gen
  PUBLIC sen::core
  PRIVATE sen::gen
  PRIVATE $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
  PRIVATE $<BUILD_INTERFACE:CLI11::CLI11>
)

add_executable(sen::cli_gen ALIAS cli_gen)
sen_internal_configure_app(cli_gen)
sen_internal_install(cli_gen)

target_include_directories(
  cli_gen
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/cli_gen
)
