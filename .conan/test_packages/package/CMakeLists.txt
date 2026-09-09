# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

cmake_minimum_required(VERSION 3.12)
project(test_package LANGUAGES CXX C)

enable_testing()

find_package(sen REQUIRED CONFIG)

set(SEN_GENERATED_FILE_TEST_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/test_generated_file.cmake")

function(add_generated_file_test test_name generated_file)
  cmake_parse_arguments(
    _arg
    ""
    "REQUIRED_TEXT;FORBIDDEN_TEXT"
    ""
    ${ARGN}
  )

  add_test(
    NAME ${test_name}
    COMMAND ${CMAKE_COMMAND} "-DGENERATED_FILE=${generated_file}" "-DREQUIRED_TEXT=${_arg_REQUIRED_TEXT}"
            "-DFORBIDDEN_TEXT=${_arg_FORBIDDEN_TEXT}" -P ${SEN_GENERATED_FILE_TEST_SCRIPT}
  )
endfunction()

# package tests
add_subdirectory(package_functions/add_sen_package package)
add_subdirectory(package_functions/add_sen_component interface)
add_subdirectory(package_functions/add_sen_interface_package i_package)
add_subdirectory(package_functions/sen_generate_package component)

# codegen tests
add_subdirectory(codegen_functions/sen_generate_cpp gen_cpp)
add_subdirectory(codegen_functions/sen_generate_python gen_python)
add_subdirectory(codegen_functions/sen_generate_uml gen_uml)
add_subdirectory(codegen_functions/sen_generate_yaml gen_yaml)

# misc tests
add_subdirectory(misc_functions/sen_configure_target misc_configure)
add_subdirectory(misc_functions/sen_enable_static_analysis misc_analysis)
add_subdirectory(misc_functions/get_external_interfaces misc_interfaces)
add_subdirectory(misc_functions/configure_exportable_packages misc_exports)
