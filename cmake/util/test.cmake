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

# Not on a sanitizer build: those lanes exist to report what is wrong, and
# stopping early hides it.
set(CMAKE_COMMON_CTEST_ARGUMENTS)
if(SEN_USE_SANITIZER STREQUAL None)
  list(APPEND CMAKE_COMMON_CTEST_ARGUMENTS "--stop-on-failure")
endif()

list(
  APPEND
  CMAKE_COMMON_CTEST_ARGUMENTS
  "--output-on-failure"
  "--timeout"
  "20"
)
if(${SEN_CTEST_RANDOMIZE_TESTS})
  list(APPEND CMAKE_COMMON_CTEST_ARGUMENTS "--schedule-random")
endif()

if(CMAKE_MAJOR_VERSION GREATER_EQUAL 4)
  set(CTEST_PARALLEL_ARGS "--parallel")
else()
  set(CTEST_PARALLEL_ARGS "--parallel" 0)
endif()

# Where the container-based suites mount the repository. run.py reads it from
# the environment and both sides default to the same path, so the mount point
# is written down once.
set(SEN_INTEGRATION_TEST_MOUNT
    "/home/builder/sen"
    CACHE STRING "Path the container-based integration tests mount the repository at"
)

# An empty main selection is a mistake, not a pass: without --no-tests=error a
# dropped suite or a filter typo leaves ctest reporting success over zero tests.
set(CMAKE_CTEST_ARGUMENTS
    ${CTEST_PARALLEL_ARGS}
    "--output-junit"
    "ctestParallelReport.xml"
    "--no-tests=error"
    ${CMAKE_COMMON_CTEST_ARGUMENTS}
)

# The flaky pass may legitimately select nothing: a build configuration is
# allowed to carry no flaky tests at all.
set(CMAKE_FLAKY_CTEST_ARGUMENTS
    "--repeat"
    "until-pass:5"
    "--output-junit"
    "ctestFlakyReport.xml"
    "--no-tests=ignore"
    ${CMAKE_COMMON_CTEST_ARGUMENTS}
)

# Appended to every sanitizer's options below, so findings can be collected.
set(SEN_SANITIZER_LOG_OPTION "")
if(SEN_SANITIZER_LOG_DIR)
  file(MAKE_DIRECTORY "${SEN_SANITIZER_LOG_DIR}")
  set(SEN_SANITIZER_LOG_OPTION ":log_path=${SEN_SANITIZER_LOG_DIR}/report")
endif()

# Compiled into each test binary rather than passed as a test property; see
# sanitizer_default_options.cpp for why, and why it cannot come from a library.
add_library(sen_sanitizer_options INTERFACE)
target_sources(sen_sanitizer_options INTERFACE ${CMAKE_CURRENT_LIST_DIR}/sanitizer_default_options.cpp)

if(ASAN_SUPPRESSION_FILE)
  target_compile_definitions(
    sen_sanitizer_options
    INTERFACE
      SEN_ASAN_DEFAULT_OPTIONS="suppressions=${ASAN_SUPPRESSION_FILE}:fast_unwind_on_malloc=0:malloc_context_size=100${SEN_SANITIZER_LOG_OPTION}"
  )
endif()

if(LSAN_SUPPRESSION_FILE)
  target_compile_definitions(
    sen_sanitizer_options
    INTERFACE
      SEN_LSAN_DEFAULT_OPTIONS="suppressions=${LSAN_SUPPRESSION_FILE}:report_objects=1${SEN_SANITIZER_LOG_OPTION}"
  )
endif()

# Without print_stacktrace a finding is one line, often naming only a dependency.
if(UBSAN_SUPPRESSION_FILE)
  target_compile_definitions(
    sen_sanitizer_options
    INTERFACE
      SEN_UBSAN_DEFAULT_OPTIONS="suppressions=${UBSAN_SUPPRESSION_FILE}:print_stacktrace=1${SEN_SANITIZER_LOG_OPTION}"
  )
endif()

if(TSAN_SUPPRESSION_FILE)
  target_compile_definitions(
    sen_sanitizer_options
    INTERFACE SEN_TSAN_DEFAULT_OPTIONS="suppressions=${TSAN_SUPPRESSION_FILE}${SEN_SANITIZER_LOG_OPTION}"
  )
endif()

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
  COMMAND ${CMAKE_CTEST_COMMAND} ${CMAKE_FLAKY_CTEST_ARGUMENTS} -L "unit" -L "flaky"
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
#   [EXTRA_PROPERTIES <prop1> <val1> [<prop2> <val2> ...]]
# )
#
# EXTRA_PROPERTIES are forwarded to gtest_discover_tests' PROPERTIES list,
# applied to every discovered test in this suite. Use it for ctest properties
# like RESOURCE_LOCK, RUN_SERIAL, TIMEOUT.
function(add_sen_unit_test_suite test_name)
  set(_options FLAKY)
  set(_one_value_args)
  set(_multi_value_args LINK_DEPS EXTRA_PROPERTIES)

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

  # Tests must see the same char signedness as the code under test: the
  # production flags force -fsigned-char (sen_misc_utils.cmake). A test built
  # with the platform default poisons shared inline definitions on platforms
  # where char is unsigned by default (arm): in Debug nothing is inlined, one
  # copy is picked for the whole process, and values like
  # numeric_limits<char>::max() become wrong in the other translation units.
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${test_name} PRIVATE -fsigned-char)
  endif()

  target_link_libraries(${test_name} PRIVATE GTest::gmock_main ${_arg_LINK_DEPS})

  if(TARGET sen_coverage_flags)
    target_link_libraries(${test_name} PRIVATE sen_coverage_flags)
  endif()

  target_link_libraries(${test_name} PRIVATE sen_sanitizer_options)

  set(labels "unit")
  if(${_arg_FLAKY})
    list(APPEND labels "flaky")
  endif()

  set(environment "")
  if(LSAN_SUPPRESSION_FILE)
    list(APPEND environment "LSAN_OPTIONS=suppressions=${LSAN_SUPPRESSION_FILE}${SEN_SANITIZER_LOG_OPTION}")
  endif()

  if(ASAN_SUPPRESSION_FILE)
    list(APPEND environment "ASAN_OPTIONS=suppressions=${ASAN_SUPPRESSION_FILE}${SEN_SANITIZER_LOG_OPTION}")
  endif()

  if(TSAN_SUPPRESSION_FILE)
    list(APPEND environment "TSAN_OPTIONS=suppressions=${TSAN_SUPPRESSION_FILE}${SEN_SANITIZER_LOG_OPTION}")
  endif()

  # Build the PROPERTIES key/value list explicitly. Passing empty ENVIRONMENT inline would let
  # list expansion eat the next args (e.g. EXTRA_PROPERTIES would silently land as ENVIRONMENT's
  # value), so only append a property when it has a value.
  set(_test_props LABELS ${labels})
  if(environment)
    list(
      APPEND
      _test_props
      ENVIRONMENT
      ${environment}
    )
  endif()
  if(_arg_EXTRA_PROPERTIES)
    list(APPEND _test_props ${_arg_EXTRA_PROPERTIES})
  endif()

  gtest_discover_tests(${test_name} DISCOVERY_MODE PRE_TEST PROPERTIES ${_test_props})

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
  set(_options FLAKY USE_TESTCONTAINERS)
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
    list(APPEND labels "flaky")
  endif()
  set_tests_properties(${test_name} PROPERTIES LABELS "${labels}")

  # disable log buffering in integration tests that use python
  append_test_env_modification(${test_name} "PYTHONUNBUFFERED=set:1")

  set(_lsan_suppressions ${LSAN_SUPPRESSION_FILE})
  set(_asan_suppressions ${ASAN_SUPPRESSION_FILE})
  set(_ubsan_suppressions ${UBSAN_SUPPRESSION_FILE})
  set(_tsan_suppressions ${TSAN_SUPPRESSION_FILE})
  set(_sanitizer_log_option ${SEN_SANITIZER_LOG_OPTION})

  # the USE_TESTCONTAINERS argument indicates that the integration test will use python testcontainers
  if(${_arg_USE_TESTCONTAINERS})
    # Hand the runtime image and the mount point to the python test driver
    # (see run.py). The sanitizer options are read inside the container, so
    # the suppression files are named by their path there, not on the host.
    append_test_env_modification(
      ${test_name} "SEN_INTEGRATION_TEST_IMAGE=set:${SEN_INTEGRATION_TEST_IMAGE}"
      "SEN_INTEGRATION_TEST_MOUNT=set:${SEN_INTEGRATION_TEST_MOUNT}"
    )
    set(_lsan_suppressions ${SEN_INTEGRATION_TEST_MOUNT}/cmake/util/lsan_ignorelist.txt)
    set(_asan_suppressions ${SEN_INTEGRATION_TEST_MOUNT}/cmake/util/asan_ignorelist.txt)
    set(_ubsan_suppressions ${SEN_INTEGRATION_TEST_MOUNT}/cmake/util/ubsan_ignorelist.txt)
    set(_tsan_suppressions ${SEN_INTEGRATION_TEST_MOUNT}/cmake/util/tsan_ignorelist.txt)

    # And where a finding is written, for the same reason. Left on the host's path the
    # sanitizer creates it inside the container and the container is deleted with it, and a
    # log path silences stderr, so nothing survives. run.py mounts the repository read-write
    # here, so a path under the mount comes back out. Per test; run.py adds the container.
    if(SEN_SANITIZER_LOG_DIR)
      file(
        RELATIVE_PATH
        _log_dir_from_root
        ${PROJECT_SOURCE_DIR}
        ${SEN_SANITIZER_LOG_DIR}
      )
      # Per test, because every container's process is pid 1 and the runtime names its
      # file report.<pid>: without this the whole container suite lands on report.1 and
      # truncates itself. run.py adds a directory per container, which is the other half
      # -- a test that starts two of them collides with itself otherwise.
      set(_sanitizer_log_option
          ":log_path=${SEN_INTEGRATION_TEST_MOUNT}/${_log_dir_from_root}/${test_name}/report"
      )
    endif()
  endif()

  if(LSAN_SUPPRESSION_FILE)
    append_test_env_modification(
      ${test_name}
      "LSAN_OPTIONS=set:suppressions=${_lsan_suppressions}:report_objects=1${_sanitizer_log_option}"
    )
  endif()

  if(ASAN_SUPPRESSION_FILE)
    append_test_env_modification(
      ${test_name}
      "ASAN_OPTIONS=set:suppressions=${_asan_suppressions}:fast_unwind_on_malloc=0:malloc_context_size=100${_sanitizer_log_option}"
    )
  endif()

  if(UBSAN_SUPPRESSION_FILE)
    append_test_env_modification(
      ${test_name} "UBSAN_OPTIONS=set:suppressions=${_ubsan_suppressions}:print_stacktrace=1"
    )
  endif()

  if(TSAN_SUPPRESSION_FILE)
    append_test_env_modification(
      ${test_name} "TSAN_OPTIONS=set:suppressions=${_tsan_suppressions}${_sanitizer_log_option}"
    )
  endif()

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

  if(_arg_WORKING_DIRECTORY)
    set(_working_dir ${_arg_WORKING_DIRECTORY})
  elseif(DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(_working_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
  else()
    # Only a multi-config generator writes binaries under a per-configuration directory.
    # Keyed on the generator's name, every single-config build that is not Ninja was sent
    # to one that does not exist, and ctest will not start a test there.
    get_property(_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(_multi_config)
      set(_working_dir ${PROJECT_BINARY_DIR}/bin/$<CONFIG>)
    else()
      set(_working_dir ${PROJECT_BINARY_DIR}/bin)
    endif()
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
    list(APPEND labels "flaky")
  endif()
  set_tests_properties(${test_name} PROPERTIES LABELS "${labels}")

  if(_arg_WILL_FAIL)
    set_tests_properties(${test_name} PROPERTIES WILL_FAIL TRUE)
  endif()

  if(NOT WIN32)
    set_tests_properties(${test_name} PROPERTIES ENVIRONMENT "LD_LIBRARY_PATH=${_working_dir}")
    append_test_env_modification(
      ${test_name} "PATH=path_list_append:$<TARGET_FILE_DIR:sen::cli_sen>"
      "LD_LIBRARY_PATH=path_list_append:$<TARGET_FILE_DIR:sen::cli_sen>"
    )
  endif()

  if(LSAN_SUPPRESSION_FILE)
    append_test_env_modification(
      ${test_name} "LSAN_OPTIONS=set:suppressions=${LSAN_SUPPRESSION_FILE}${SEN_SANITIZER_LOG_OPTION}"
    )
  endif()

  if(ASAN_SUPPRESSION_FILE)
    append_test_env_modification(
      ${test_name} "ASAN_OPTIONS=set:suppressions=${ASAN_SUPPRESSION_FILE}${SEN_SANITIZER_LOG_OPTION}"
    )
  endif()

  if(TSAN_SUPPRESSION_FILE)
    append_test_env_modification(
      ${test_name} "TSAN_OPTIONS=set:suppressions=${TSAN_SUPPRESSION_FILE}${SEN_SANITIZER_LOG_OPTION}"
    )
  endif()

endfunction()

# add_sanitizer_options(<test_name>)
#
# Tells a test where its suppressions are and where to write a finding. Without it a
# test is instrumented and reports to nobody, which reads exactly like a clean run.
# The integration and run-smoke suites pass extra flags and still carry their own copies.
function(add_sanitizer_options test_name)
  if(LSAN_SUPPRESSION_FILE)
    append_test_env_modification(
      ${test_name} "LSAN_OPTIONS=set:suppressions=${LSAN_SUPPRESSION_FILE}${SEN_SANITIZER_LOG_OPTION}"
    )
  endif()

  if(ASAN_SUPPRESSION_FILE)
    append_test_env_modification(
      ${test_name} "ASAN_OPTIONS=set:suppressions=${ASAN_SUPPRESSION_FILE}${SEN_SANITIZER_LOG_OPTION}"
    )
  endif()

  if(UBSAN_SUPPRESSION_FILE)
    append_test_env_modification(
      ${test_name} "UBSAN_OPTIONS=set:suppressions=${UBSAN_SUPPRESSION_FILE}:print_stacktrace=1"
    )
  endif()

  if(TSAN_SUPPRESSION_FILE)
    append_test_env_modification(
      ${test_name} "TSAN_OPTIONS=set:suppressions=${TSAN_SUPPRESSION_FILE}${SEN_SANITIZER_LOG_OPTION}"
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
    list(APPEND labels "flaky")
  endif()
  set_tests_properties(${test_name} PROPERTIES LABELS "${labels}")

  add_sanitizer_options(${test_name})

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

# append_test_env_modification(
#   <test_target_name>
#   [list of modifications]
# )
function(append_test_env_modification test_target_name)
  set(new_modifications ${ARGN})

  if(NOT new_modifications)
    message(
      AUTHOR_WARNING
        "append_test_env_modification called for '${test_target_name}' but no modifications were provided!"
    )
    return()
  endif()

  get_test_property(${test_target_name} ENVIRONMENT_MODIFICATION current_modifications)

  if(current_modifications STREQUAL "NOTFOUND")
    set(current_modifications "")
  endif()

  list(APPEND current_modifications ${new_modifications})

  set_tests_properties(${test_target_name} PROPERTIES ENVIRONMENT_MODIFICATION "${current_modifications}")
endfunction()
