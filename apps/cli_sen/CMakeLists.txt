# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/cli11)

add_executable(cli_sen main.cpp)

target_link_libraries(
  cli_sen
  PRIVATE $<BUILD_INTERFACE:CLI11::CLI11>
  PRIVATE core
)

add_executable(sen::sen ALIAS cli_sen)
add_executable(sen::cli_sen ALIAS cli_sen)

set_target_properties(cli_sen PROPERTIES RUNTIME_OUTPUT_NAME sen)

sen_internal_configure_app(cli_sen)
sen_enable_static_analysis(cli_sen)
sen_internal_install(cli_sen)
