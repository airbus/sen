# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

find_program(PYTEST_EXEC pytest REQUIRED)

add_sen_package(
  TARGET crash_report
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Verify that crash reports contain a stack trace"
  SOURCES src/crash_report.h src/crash_report.cpp
  STL_FILES stl/crash_report.stl
  EXPORT_NAME sen.test.crash_report
  NO_SCHEMA
)

set_target_properties(crash_report PROPERTIES FOLDER "test")

add_sen_integration_test(
  kernel_crash_report_stacktrace_test
  DESCRIPTION
  "Checks that the report written after a crash carries a stack trace."
  COMMAND
  ${PYTEST_EXEC}
  ${CMAKE_CURRENT_SOURCE_DIR}/crash_report_tester.py
  WORKING_DIRECTORY $<TARGET_FILE_DIR:sen::cli_run>
  REQ_DEPS crash_report
)

append_test_env_modification(
  kernel_crash_report_stacktrace_test "PYTHONPATH=path_list_append:${CMAKE_CURRENT_SOURCE_DIR}"
  "PYTHONPATH=path_list_append:${CMAKE_CURRENT_SOURCE_DIR}/.."
)
set_tests_properties(kernel_crash_report_stacktrace_test PROPERTIES RUN_SERIAL TRUE)

append_test_env_modification(
  kernel_crash_report_stacktrace_test
  "TSAN_OPTIONS=string_prepend:report_signal_unsafe=0:handle_sigfpe=0:handle_abort=0 "
  "ASAN_OPTIONS=string_append: handle_sigfpe=0:handle_abort=0:detect_leaks=0"
  "ASAN_OPTIONS=string_append: fast_unwind_on_malloc=1:malloc_context_size=5"
  "UBSAN_OPTIONS=string_append: halt_on_error=0:print_stacktrace=0"
)

# A process that arms crash reporting itself, which `sen run` never does with the key set.
add_executable(self_armed_crash src/self_armed_crash.cpp)
sen_configure_target(self_armed_crash)
target_link_libraries(self_armed_crash PRIVATE sen::core sen::kernel)
sen_enable_static_analysis(self_armed_crash)

set_target_properties(
  self_armed_crash PROPERTIES FOLDER "test" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

add_sen_integration_test(
  kernel_crash_report_self_armed_test
  DESCRIPTION
  "Checks that crashReportDisabled suppresses the report of a process that armed itself."
  COMMAND
  ${PYTEST_EXEC}
  ${CMAKE_CURRENT_SOURCE_DIR}/self_armed_tester.py
  WORKING_DIRECTORY $<TARGET_FILE_DIR:self_armed_crash>
  REQ_DEPS self_armed_crash
)

append_test_env_modification(
  kernel_crash_report_self_armed_test
  "PYTHONPATH=path_list_append:${CMAKE_CURRENT_SOURCE_DIR}"
  "PYTHONPATH=path_list_append:${CMAKE_CURRENT_SOURCE_DIR}/.."
  "SEN_SELF_ARMED_CRASH=set:$<TARGET_FILE:self_armed_crash>"
)
set_tests_properties(kernel_crash_report_self_armed_test PROPERTIES RUN_SERIAL TRUE)

append_test_env_modification(
  kernel_crash_report_self_armed_test
  "TSAN_OPTIONS=string_prepend:report_signal_unsafe=0:handle_sigfpe=0:handle_abort=0 "
  "ASAN_OPTIONS=string_append: handle_sigfpe=0:handle_abort=0:detect_leaks=0"
  "ASAN_OPTIONS=string_append: fast_unwind_on_malloc=1:malloc_context_size=5"
  "UBSAN_OPTIONS=string_append: halt_on_error=0:print_stacktrace=0"
)
