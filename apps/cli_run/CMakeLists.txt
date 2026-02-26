# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/cli11)
include(tps/spdlog)

set(builtin_configs builtin_configs/shell.yaml builtin_configs/explorer.yaml builtin_configs/replay.yaml)

set(builtin_configs_headers)
foreach(_config_file ${builtin_configs})
  get_filename_component(_config_name ${_config_file} NAME_WE)

  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cli_run/builtin_configs)
  set(_outfile ${CMAKE_CURRENT_BINARY_DIR}/cli_run/builtin_configs/${_config_name}.h)

  sen_file_to_cpp(
    IN
    ${_config_file}
    OUT
    ${_outfile}
    VARNAME
    ${_config_name}
  )

  list(APPEND builtin_configs_headers ${_outfile})

endforeach()

add_executable(cli_run ${builtin_configs_headers} main.cpp)

target_link_libraries(
  cli_run
  PUBLIC sen::core sen::kernel
  PRIVATE $<BUILD_INTERFACE:CLI11::CLI11> $<BUILD_INTERFACE:spdlog::spdlog>
)

add_executable(sen::cli_run ALIAS cli_run)
sen_internal_configure_app(cli_run)

target_include_directories(cli_run PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/cli_run)

sen_internal_add_resources(
  TARGET
  cli_run
  GROUP
  "builtin_configs"
  RESOURCE_FILES
  ${builtin_configs}
)

sen_internal_install(cli_run)
