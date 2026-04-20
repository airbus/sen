# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/ftxui)
include(tps/spdlog)

set(term_test_deps
    sen::core
    sen::kernel
    ftxui::component
    spdlog::spdlog
    term_for_testing
)

add_sen_unit_test_suite(
  term_test
  unit/completer_test.cpp
  unit/scope_test.cpp
  unit/parse_utils_test.cpp
  unit/invocation_test.cpp
  unit/value_formatter_test.cpp
  unit/signature_renderer_test.cpp
  unit/output_pane_test.cpp
  unit/log_router_test.cpp
  unit/object_store_test.cpp
  unit/suggester_test.cpp
  unit/completer_incremental_test.cpp
  unit/input_pane_test.cpp
  unit/arg_form_test.cpp
  unit/status_bar_test.cpp
  unit/banner_test.cpp
  unit/text_table_test.cpp
  unit/tree_view_test.cpp
  unit/clipboard_test.cpp
  unit/command_engine_test.cpp
  LINK_DEPS
  ${term_test_deps}
)

add_subdirectory(integration/packages/term_showcase)

sen_generate_cpp(
  TARGET term_test
  STL_FILES unit/stl/test_object.stl
  BASE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/unit
  GEN_HDR_FILES public_generated_files
)

target_include_directories(term_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

sen_enable_static_analysis(term_test)
