# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/asio)
include(tps/ftxui)
include(tps/spdlog)

# -------------------------------------------------------------------------------------------------------------
# term component
# -------------------------------------------------------------------------------------------------------------

add_sen_package(
  TARGET term
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Modern TUI-based interactive terminal for the sen kernel"
  SOURCES src/component.cpp
          src/app.h
          src/app.cpp
          src/app_renderers.h
          src/app_renderers.cpp
          src/banner.h
          src/banner.cpp
          src/output_pane.h
          src/output_pane.cpp
          src/input_pane.h
          src/input_pane.cpp
          src/status_bar.h
          src/status_bar.cpp
          src/styles.h
          src/theme.h
          src/theme.cpp
          src/parse_utils.h
          src/scope.h
          src/scope.cpp
          src/signature_renderer.h
          src/signature_renderer.cpp
          src/arg_form.h
          src/arg_form.cpp
          src/suggester.h
          src/suggester.cpp
          src/value_formatter.h
          src/value_formatter.cpp
          src/object_store.h
          src/object_store.cpp
          src/command_engine.h
          src/command_engine.cpp
          src/command_engine_watch.cpp
          src/command_engine_inspect.cpp
          src/completer.h
          src/completer.cpp
          src/tree_view.h
          src/tree_view.cpp
          src/output_capture.h
          src/output_capture.cpp
          src/log_sink.h
          src/log_sink.cpp
          src/log_router.h
          src/log_router.cpp
          src/clipboard.h
          src/clipboard.cpp
          src/text_table.h
          src/text_table.cpp
          src/byte_format.h
  STL_FILES stl/term.stl
  PRIVATE_DEPS ftxui::component asio::asio spdlog::spdlog
  TEST_TARGET term_for_testing
  IS_COMPONENT
)

set_target_properties(term PROPERTIES FOLDER "components/term")
sen_enable_static_analysis(term)

sen_internal_install(term)

if(SEN_BUILD_TESTS)
  add_subdirectory(test)
endif()

install(FILES ${PROJECT_SOURCE_DIR}/schemas/term.json DESTINATION schemas)
