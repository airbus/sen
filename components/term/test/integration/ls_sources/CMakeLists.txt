# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

find_package(Python REQUIRED COMPONENTS Interpreter)
set(_working_dir $<TARGET_FILE_DIR:sen::cli_run>)

# One test per behaviour, so a failure names what broke rather than "the ls test".
set(_ls_sources_cases
    test_an_unopened_session_is_listed
    test_an_open_bus_with_no_objects_is_listed
    test_opening_a_bus_tags_it_open
    test_an_open_session_with_no_objects_is_listed
)

# Not on Windows, for the reason the session suite gives: the harness drives the term through a pty,
# and pty, fcntl, termios and SIGKILL are absent from CPython there.
if(NOT MSVC)
  foreach(_case IN LISTS _ls_sources_cases)
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
      term_ls_sources_${_case}
      DESCRIPTION
      "Checks that ${_case_words}."
      COMMAND
      ${Python_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/test_ls_sources.py
      ${_working_dir}/cli_run
      ${CMAKE_CURRENT_SOURCE_DIR}/config/ls_sources.yaml
      TestLsSources.${_case}
      REQ_DEPS term_showcase py ether term kernel
    )
  endforeach()
endif()
