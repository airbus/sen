# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/cli11)
include(tps/inja)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/templates)
sen_internal_generate_template_headers(
  OUTDIR
  ${CMAKE_CURRENT_BINARY_DIR}/templates
  GEN_FILE_LIST
  template_dumps_test
  TEMPLATE_FILES
  templates/test_txt.j2
)

add_sen_unit_test_suite(
  cli_package_test
  ${template_dumps_test}
  util_test.cpp
  init_package_test.cpp
  init_component_test.cpp
  LINK_DEPS
  CLI11::CLI11
  pantor::inja
  cli_package_for_testing
)

target_include_directories(
  cli_package_test SYSTEM
  PRIVATE cli_package CLI11::CLI11 ${CMAKE_CURRENT_SOURCE_DIR}/..
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/templates
)

source_group("templates/test/headers" FILES ${template_dumps_test})

sen_internal_add_resources(
  TARGET
  cli_package_test
  GROUP
  "templates/test"
  RESOURCE_FILES
  templates/test_txt.j2
)

sen_enable_static_analysis(cli_package_test)
