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
  COMMAND
  ${PYTEST_EXEC}
  ${CMAKE_CURRENT_SOURCE_DIR}/crash_report_tester.py
  WORKING_DIRECTORY $<TARGET_FILE_DIR:sen::cli_run>
  REQ_DEPS crash_report
)

set_tests_properties(
  kernel_crash_report_stacktrace_test
  PROPERTIES ENVIRONMENT
             "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}:${CMAKE_CURRENT_SOURCE_DIR}/..;PYTHONUNBUFFERED=1"
)
set_tests_properties(kernel_crash_report_stacktrace_test PROPERTIES RUN_SERIAL TRUE)
