# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

find_package(Python REQUIRED COMPONENTS Interpreter)
set(_working_dir $<TARGET_FILE_DIR:sen::cli_run>)

# One test per behaviour, so a failure names what broke rather than "the session test".
set(_term_session_cases
    test_the_binary_starts_and_draws_a_prompt
    test_help_lists_the_commands
    test_ls_shows_the_objects_on_the_bus
    test_the_displayed_scope_path_can_be_typed_back
    test_an_unknown_command_reports_and_the_term_survives
    test_completion_keeps_the_rest_of_the_line
    test_ctrl_d_exits_only_from_an_empty_line
    test_a_huge_paste_leaves_the_term_responsive
    test_the_term_asks_the_terminal_for_bracketed_paste
    test_a_multi_line_paste_runs_nothing_and_becomes_one_line
    test_a_tab_inside_a_paste_is_text_not_completion
    test_a_paste_whose_end_marker_never_arrives_releases_itself
    test_a_torn_end_marker_ends_the_paste_and_leaves_no_stray_bytes
    test_escape_cancels_a_paste_in_progress
    test_the_line_completion_produces_gets_a_useful_answer
    test_output_fills_the_pane_from_the_top
    test_the_banner_is_drawn_at_the_top_of_the_screen
    test_status_reports_the_running_components
    test_open_and_close_report_both_ways
    test_query_create_list_and_remove
    test_listen_listeners_and_unlisten
    test_log_reports_and_sets_a_level
    test_inspect_shows_an_object_and_a_type
    test_types_and_units_take_a_filter
    test_theme_switches_and_reports
    test_clear_empties_the_output
    test_version_reports_the_build
    test_a_form_opens_takes_values_and_submits_the_call
    test_escape_closes_a_form_without_calling
    test_f1_runs_help
    test_page_up_scrolls_back_and_page_down_returns
    test_arrow_up_recalls_the_previous_command
    test_escape_twice_on_an_empty_line_shuts_the_kernel_down
    test_exit_shuts_the_kernel_down
)

# Not on Windows: the harness drives the term through a pty, and pty, fcntl, termios and SIGKILL do not
# exist in CPython there, so every one of these fails at import rather than telling anyone anything. The
# same guard and the same reasoning are on the kernel's Python-driven suites in libs/kernel/test.
if(NOT MSVC)
  foreach(_case IN LISTS _term_session_cases)
    add_sen_integration_test(
      term_session_${_case}
      COMMAND
      ${Python_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/test_session.py
      ${_working_dir}/cli_run
      ${CMAKE_CURRENT_SOURCE_DIR}/config/session.yaml
      TestTermSession.${_case}
      REQ_DEPS term_showcase py ether term kernel
    )
  endforeach()
endif()
