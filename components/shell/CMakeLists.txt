# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/asio)
include(tps/cli11)

# -------------------------------------------------------------------------------------------------------------
# terminal_lib
# -------------------------------------------------------------------------------------------------------------

set(sources
    src/terminal_lib/local_terminal.h
    src/terminal_lib/remote_terminal_client.cpp
    src/terminal_lib/remote_terminal_client.h
    src/terminal_lib/remote_terminal_server.cpp
    src/terminal_lib/remote_terminal_server.h
    src/terminal_lib/terminal.h
    src/terminal_lib/util.h
    src/terminal_lib/vterm.h
)

if(WIN32)
  set(os_sources src/terminal_lib/win32_terminal.cpp)
else()
  set(os_sources src/terminal_lib/posix_terminal.cpp)
endif()

add_library(terminal_lib STATIC ${sources} ${os_sources})

sen_generate_cpp(TARGET terminal_lib STL_FILES stl/remote_shell.stl)

target_link_libraries(terminal_lib PUBLIC sen::core $<BUILD_INTERFACE:asio::asio>)

target_include_directories(
  terminal_lib PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/components/shell/src/terminal_lib>"
)

sen_internal_configure_lib(terminal_lib)
sen_enable_static_analysis(terminal_lib)

set_target_properties(terminal_lib PROPERTIES FOLDER "components/shell")

sen_internal_install(terminal_lib)

# -------------------------------------------------------------------------------------------------------------
# shell component
# -------------------------------------------------------------------------------------------------------------

add_sen_component(
  TARGET shell
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "A shell for the sen kernel"
  SOURCES src/shell_service/shell.h
          src/shell_service/shell.cpp
          src/shell_service/component.cpp
          src/shell_service/completer.h
          src/shell_service/completer.cpp
          src/shell_service/printer.h
          src/shell_service/printer.cpp
          src/shell_service/line_edit.h
          src/shell_service/line_edit.cpp
          src/shell_service/terminal_tree.h
          src/shell_service/terminal_tree.cpp
          src/shell_service/box_chars.h
          src/shell_service/history.h
          src/shell_service/history.cpp
          src/shell_service/styles.h
  STL_FILES stl/shell.stl
  DEPS terminal_lib
)

set_target_properties(shell PROPERTIES FOLDER "components/shell")
sen_enable_static_analysis(shell)

sen_internal_install(shell)

install(FILES ${PROJECT_SOURCE_DIR}/schemas/shell.json DESTINATION schemas)
