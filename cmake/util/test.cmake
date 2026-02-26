# === test.cmake =======================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# Meta test initialization code

# By default, we run tests in random order to prevent the introduction of implicit test dependencies.
option(SEN_CTEST_RANDOMIZE_TESTS "Randomize test execution" On)

if(NOT SEN_BUILD_TESTS)
  set(BUILD_TESTING OFF) # Signal to ctest that no tests should be build
endif()

set(CMAKE_COMMON_CTEST_ARGUMENTS
    "--stop-on-failure"
    "--output-on-failure"
    "--timeout"
    "20"
    ${CMAKE_COMMON_CTEST_ARGUMENTS}
)
if(${CTEST_RANDOMIZE_TESTS})
  list(APPEND CMAKE_COMMON_CTEST_ARGUMENTS "--schedule-random")
endif()

set(CMAKE_CTEST_ARGUMENTS
    "--parallel"
    "0"
    "--output-junit"
    "ctestParallelReport.xml"
    ${CMAKE_COMMON_CTEST_ARGUMENTS}
)

set(CMAKE_FLAKY_CTEST_ARGUMENTS
    "--repeat"
    "until-pass:5"
    "--output-junit"
    "ctestFlakyReport.xml"
    ${CMAKE_COMMON_CTEST_ARGUMENTS}
)

enable_testing()

find_package(GTest QUIET)
option(INSTALL_GTEST "" OFF)
include(GoogleTest)

add_custom_target(run_tests)
add_custom_command(
  POST_BUILD TARGET run_tests
  COMMAND ${CMAKE_CTEST_COMMAND} ${CMAKE_CTEST_ARGUMENTS} -LE "flaky"
  VERBATIM
  COMMAND ${CMAKE_CTEST_COMMAND} ${CMAKE_FLAKY_CTEST_ARGUMENTS} -L "flaky"
  VERBATIM
  COMMAND junitparser merge "ctestParallelReport.xml" "ctestFlakyReport.xml" "ctestReport.xml"
  USES_TERMINAL
)

add_custom_target(run_unit_tests)
add_custom_command(
  POST_BUILD TARGET run_unit_tests
  COMMAND ${CMAKE_CTEST_COMMAND} ${CMAKE_CTEST_ARGUMENTS} -L "unit" -LE "flaky"
  VERBATIM
  COMMAND ${CMAKE_CTEST_COMMAND} ${CMAKE_FLAKY_CTEST_ARGUMENTS} -L "unit" "flaky"
  VERBATIM USES_TERMINAL
)

add_custom_target(run_integration_tests)
add_custom_command(
  POST_BUILD TARGET run_integration_tests
  COMMAND ${CMAKE_CTEST_COMMAND} ${CMAKE_CTEST_ARGUMENTS} -L "integration" -LE "flaky"
  VERBATIM USES_TERMINAL
)

add_custom_target(run_smoke_tests)
add_custom_command(
  POST_BUILD TARGET run_smoke_tests
  COMMAND ${CMAKE_CTEST_COMMAND} ${CMAKE_CTEST_ARGUMENTS} -L "smoke" -LE "flaky"
  VERBATIM USES_TERMINAL
)

add_custom_target(run_flaky_tests)
add_custom_command(
  POST_BUILD TARGET run_flaky_tests
  COMMAND ${CMAKE_CTEST_COMMAND} ${CMAKE_FLAKY_CTEST_ARGUMENTS} -L "flaky"
  VERBATIM USES_TERMINAL
)

# add_sen_unit_test_suite(
#   ... test_args ...
#   [LINK_DEPS <deps>]
# )
function(add_sen_unit_test_suite test_name)
  set(_options FLAKY)
  set(_one_value_args)
  set(_multi_value_args LINK_DEPS)

  cmake_parse_arguments(
    _arg
    "${_options}"
    "${_one_value_args}"
    "${_multi_value_args}"
    ${ARGN}
  )

  if(NOT TARGET GTest::gmock_main)
    message(FATAL_ERROR "add_sen_unit_test_suite() requires package GTest and it is not found.")
  endif()

  add_executable(${test_name} ${_arg_UNPARSED_ARGUMENTS})

  target_link_libraries(${test_name} PRIVATE GTest::gmock_main ${_arg_LINK_DEPS})

  set(labels "unit")
  if(${_arg_FLAKY})
    list(APPEND labels "LABELS;flaky")
  endif()

  gtest_discover_tests(${test_name} DISCOVERY_MODE PRE_TEST PROPERTIES LABELS ${labels})

  add_dependencies(run_unit_tests ${test_name})
  add_dependencies(run_tests ${test_name})

  sen_enable_static_analysis(${test_name})
endfunction()

# add_sen_integration_test(
#   ... test_args ...
#   [REQ_COMPONENTS <req_comp>]
#   [REQ_DEPS <deps>]
# )
function(add_sen_integration_test test_name)
  set(_options)
  set(_one_value_args)
  set(_multi_value_args REQ_COMPONENTS REQ_DEPS)

  cmake_parse_arguments(
    _arg
    "${_options}"
    "${_one_value_args}"
    "${_multi_value_args}"
    ${ARGN}
  )

  add_test(NAME ${test_name} ${_arg_UNPARSED_ARGUMENTS})

  set(labels "integration")
  if(${_arg_FLAKY})
    list(APPEND labels "LABELS;flaky")
  endif()
  set_tests_properties(${test_name} PROPERTIES LABELS ${labels})

  add_dependencies(run_integration_tests ${_arg_REQ_COMPONENTS} ${_arg_REQ_DEPS})
  add_dependencies(run_tests ${_arg_REQ_COMPONENTS} ${_arg_REQ_DEPS})
endfunction()

# add_sen_run_smoke_test(
#   <name>
#   CONFIG_FILE <file>
#   [WORKING_DIRECTORY <target>]
#   [NO_START_STOP]
#   [WILL_FAIL]
# )
function(add_sen_run_smoke_test test_name)
  set(_options NO_START_STOP WILL_FAIL FLAKY)
  set(_one_value_args WORKING_DIRECTORY CONFIG_FILE)
  set(_multi_value_args)

  cmake_parse_arguments(
    _arg
    "${_options}"
    "${_one_value_args}"
    "${_multi_value_args}"
    ${ARGN}
  )

  if(NOT _arg_CONFIG_FILE)
    message(FATAL_ERROR "add_sen_run_smoke_test: no CONFIG_FILE set")
  endif()

  if(NOT
     CMAKE_GENERATOR
     STREQUAL
     "Ninja"
  )
    set(_working_dir ${PROJECT_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE})
  else()
    set(_working_dir ${PROJECT_BINARY_DIR}/bin)
  endif()

  if(_arg_WORKING_DIRECTORY)
    set(_working_dir ${_arg_WORKING_DIRECTORY})
  endif()

  get_filename_component(_abs_config ${_arg_CONFIG_FILE} ABSOLUTE)

  if(_arg_NO_START_STOP)
    set(_start_stop_flag)
  else()
    set(_start_stop_flag --start-stop)
  endif()

  add_test(
    NAME ${test_name}
    COMMAND sen::cli_sen run ${_abs_config} ${_start_stop_flag}
    WORKING_DIRECTORY ${_working_dir} COMMAND_EXPAND_LISTS
  )

  set(labels "smoke")
  if(${_arg_FLAKY})
    list(APPEND labels "LABELS;flaky")
  endif()
  set_tests_properties(${test_name} PROPERTIES LABELS ${labels})

  if(_arg_WILL_FAIL)
    set_tests_properties(${test_name} PROPERTIES WILL_FAIL TRUE)
  endif()

  if(NOT WIN32)
    set_tests_properties(${test_name} PROPERTIES ENVIRONMENT "LD_LIBRARY_PATH=${_working_dir}")
    set_tests_properties(
      ${test_name}
      PROPERTIES
        ENVIRONMENT_MODIFICATION
        "PATH=path_list_append:$<TARGET_FILE_DIR:sen::cli_sen>;LD_LIBRARY_PATH=path_list_append:$<TARGET_FILE_DIR:sen::cli_sen>"
    )
  endif()

endfunction()

# add_sen_smoke_test(
#   <name>
#   COMMAND <cmd>
#   [WORKING_DIRECTORY <target>]
#   [REQ_DEPS <deps>]
# )
function(add_sen_smoke_test test_name)
  set(_options FLAKY)
  set(_one_value_args WORKING_DIRECTORY)
  set(_multi_value_args COMMAND REQ_DEPS)

  cmake_parse_arguments(
    _arg
    "${_options}"
    "${_one_value_args}"
    "${_multi_value_args}"
    ${ARGN}
  )

  add_test(
    NAME ${test_name}
    COMMAND ${_arg_COMMAND}
    WORKING_DIRECTORY ${_arg_WORKING_DIRECTORY}
  )

  set(labels "smoke")
  if(${_arg_FLAKY})
    list(APPEND labels "LABELS;flaky")
  endif()
  set_tests_properties(${_arg_NAME} PROPERTIES LABELS ${labels})

  add_dependencies(run_smoke_tests ${_arg_REQ_DEPS})
  add_dependencies(run_tests ${_arg_REQ_DEPS})
endfunction()

# add_sen_cli_gen_smoke_test(
#   <name>
#   COMMAND <cmd>
#   [WORKING_DIRECTORY <target>]
#   [REQ_DEPS <deps>]
# )
function(add_sen_cli_gen_smoke_test test_name)
  set(_options)
  set(_one_value_args)
  set(_multi_value_args REQ_DEPS)

  cmake_parse_arguments(
    _arg
    "${_options}"
    "${_one_value_args}"
    "${_multi_value_args}"
    ${ARGN}
  )

  add_sen_smoke_test(
    ${test_name}
    ${_arg_UNPARSED_ARGUMENTS}
    REQ_DEPS
    cli_gen
    ${_arg_REQ_DEPS}
  )
endfunction()
