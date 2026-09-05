# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# `sen generate` hands over to the cli_gen binary sitting beside it, and the shortcuts do the same
# for theirs. Running a copy from a directory where that sibling is absent is the only way to reach
# the failure path, since the lookup goes through the running executable's own location.
#
# The copy is a fixture rather than part of the command, because a test runs one command.
set(isolated_sen_dir ${CMAKE_CURRENT_BINARY_DIR}/isolated_sen)

add_test(NAME sen_missing_generator_setup COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:sen::cli_sen>
                                                  ${isolated_sen_dir}/$<TARGET_FILE_NAME:sen::cli_sen>
)
set_tests_properties(
  sen_missing_generator_setup
  PROPERTIES FIXTURES_SETUP
             isolated_sen
             LABELS
             smoke
)

# Registered with a bare add_test, so it misses what the helpers give every other test. Without
# this it carries no log_path and check_sanitizer_lane.py fails the lane, correctly: a finding
# here would reach nobody.
add_sanitizer_options(sen_missing_generator_setup)

# Matching the diagnostic rather than gating on the exit code alone: the defect this covers
# exited zero AND printed nothing, so an empty output must not be able to pass.
add_sen_smoke_test(
  sen_missing_generator_reports
  COMMAND ${isolated_sen_dir}/$<TARGET_FILE_NAME:sen::cli_sen> generate mkdocs fom -d
          ${CMAKE_CURRENT_BINARY_DIR} -t ${CMAKE_CURRENT_BINARY_DIR} -o ${CMAKE_CURRENT_BINARY_DIR}/unused.md
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_sen
)
set_tests_properties(
  sen_missing_generator_reports
  PROPERTIES FIXTURES_REQUIRED
             isolated_sen
             PASS_REGULAR_EXPRESSION
             "cannot run.*cli_gen"
)

# The copy is moved away from its libraries, so the loader needs pointing back at them. Windows
# searches the executable's own directory and PATH and has no rpath, which is the same reason
# add_sen_unit_test_suite appends bin/ there.
if(WIN32)
  append_test_env_modification(
    sen_missing_generator_reports "PATH=path_list_prepend:$<TARGET_FILE_DIR:sen::cli_sen>"
  )
else()
  append_test_env_modification(
    sen_missing_generator_reports "LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:sen::cli_sen>"
  )
endif()
