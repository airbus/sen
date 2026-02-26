# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/cli11)

add_executable(cli_remote_shell main.cpp)

target_link_libraries(
  cli_remote_shell
  PUBLIC sen::core sen::kernel
  PRIVATE $<BUILD_INTERFACE:CLI11::CLI11>
  PRIVATE terminal_lib
)

sen_internal_configure_app(cli_remote_shell)
sen_internal_install(cli_remote_shell)
