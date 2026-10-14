# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

find_package(Python REQUIRED COMPONENTS Interpreter)
set(_working_dir $<TARGET_FILE_DIR:sen::cli_run>)

# One test per route, because the routes are captured by different mechanisms and a single test
# over all of them would report "output missing" without saying which one term lost.
set(_output_capture_cases
    test_a_kernel_logger_line_is_visible
    test_a_stderr_line_is_visible
    test_a_cout_line_is_visible
    test_a_printf_line_is_visible
    test_a_plain_spdlog_line_is_visible
)

# Not on Windows, for the reason the session suite gives: the harness drives the term through a pty,
# and pty, fcntl, termios and SIGKILL are absent from CPython there.
if(NOT MSVC)
  foreach(_case IN LISTS _output_capture_cases)
    string(
      REGEX
      REPLACE "^test_"
              ""
              _case_words
              "${_case}"
    )
    string(
      REPLACE "_"
              " "
              _case_words
              "${_case_words}"
    )
    add_sen_integration_test(
      term_output_capture_${_case}
      DESCRIPTION
      "Checks that ${_case_words}."
      COMMAND
      ${Python_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/test_output_capture.py
      ${_working_dir}/cli_run
      ${CMAKE_CURRENT_SOURCE_DIR}/../config/term_capture.yaml
      TestOutputCapture.${_case}
      REQ_DEPS log_emitter term kernel
    )
  endforeach()
endif()
