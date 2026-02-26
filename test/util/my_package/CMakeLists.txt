# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/spdlog)

add_sen_package(
  TARGET my_package
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Example package skeleton"
  SOURCES src/my_class.h src/my_class.cpp
  STL_FILES stl/my_package/my_class.stl
            stl/my_package/basic_types.stl
            stl/my_package/config.stl
            stl/my_package/test.stl
            stl/my_package/test_sub.stl
            stl/my_package/my_abstract_class.stl
  CODEGEN_SETTINGS src/codegen_settings.json
  SCHEMA_PATH ${CMAKE_CURRENT_BINARY_DIR}
  PRIVATE_DEPS spdlog::spdlog
  GEN_HDR_FILES _generated_headers
)

set_target_properties(my_package PROPERTIES FOLDER "examples/basic")
set_target_properties(my_package PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# smoke tests
add_sen_run_smoke_test(my_package_abstract_smoke CONFIG_FILE config/my_package_abstract.yaml WILL_FAIL)
add_sen_run_smoke_test(my_package_smoke CONFIG_FILE config/my_package.yaml)
add_sen_run_smoke_test(my_package_shell_smoke CONFIG_FILE config/my_package_with_shell.yaml)
add_sen_run_smoke_test(my_package_rshell_smoke CONFIG_FILE config/my_package_remote_shell.yaml)
add_sen_run_smoke_test(my_package_vtime_smoke CONFIG_FILE config/my_package_virtual_time.yaml)
add_sen_run_smoke_test(my_package_log_smoke CONFIG_FILE config/my_package_with_log.yaml)
add_sen_run_smoke_test(my_package_fast NO_START_STOP CONFIG_FILE config/my_package_fast.yaml)

# TODO: SEN-1298 investigate and fix the crash on Windows
if(NOT WIN32)
  add_sen_run_smoke_test(my_package_exception CONFIG_FILE config/my_package_exception.yaml WILL_FAIL)
endif()
