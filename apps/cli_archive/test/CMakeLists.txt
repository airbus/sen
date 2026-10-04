# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# Every line of cli_archive is in an anonymous namespace inside the executable, so nothing can link to
# it: the CLI is checked by running it and reading what it printed. `sen::db::Output` writes the
# archive to <folder>/<name>, which is why the path below can be named here.

set(_recording_dir ${CMAKE_CURRENT_BINARY_DIR}/cli_archive_recording)

# The build directory outlives a test run, and the recorder will not write over a recording that is
# already there, so the fixture starts by removing it.
add_test(NAME cli_archive_clear_recording COMMAND ${CMAKE_COMMAND} -E rm -rf ${_recording_dir})
record_test_description(
  cli_archive_clear_recording
  "Removes the recording an earlier run left, which the recorder will not write over."
)
set_tests_properties(
  cli_archive_clear_recording
  PROPERTIES FIXTURES_SETUP
             cli_archive_recording
             LABELS
             smoke
)

# NO_START_STOP: the run ends when record_control.py asks it to, once the objects the recording
# selects are on the bus. --start-stop returns as soon as every component is running, which is
# before the recorder has sampled anything.
add_sen_run_smoke_test(
  cli_archive_record
  DESCRIPTION
  "Records the archive that the checks below read."
  NO_START_STOP
  CONFIG_FILE ${CMAKE_CURRENT_SOURCE_DIR}/config/record.yaml
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
append_test_env_modification(cli_archive_record "PYTHONPATH=path_list_append:${CMAKE_CURRENT_SOURCE_DIR}/src")
set_tests_properties(
  cli_archive_record
  PROPERTIES FIXTURES_SETUP
             cli_archive_recording
             DEPENDS
             cli_archive_clear_recording
)
add_dependencies(
  run_tests
  cli_sen
  recorder
  py
)

add_sen_smoke_test(
  cli_archive_info_reports_a_summary DESCRIPTION
  "Reports a summary of an archive naming a non-zero object count."
  COMMAND sen::cli_archive info ${_recording_dir}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_archive
)
set_tests_properties(
  cli_archive_info_reports_a_summary
  PROPERTIES FIXTURES_REQUIRED
             cli_archive_recording
             # A count rather than the label: every label prints whether or not anything was recorded.
             PASS_REGULAR_EXPRESSION
             "objects: +[1-9]"
)

add_sen_smoke_test(
  cli_archive_indexed_lists_the_objects DESCRIPTION "Lists the objects an indexed archive holds."
  COMMAND sen::cli_archive indexed ${_recording_dir}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_archive
)
set_tests_properties(
  cli_archive_indexed_lists_the_objects
  PROPERTIES FIXTURES_REQUIRED
             cli_archive_recording
             # A row, not the header: the header is printed even when the index is empty. The type and
             # the bus rather than the object name, so renaming the kernel's own object is not a
             # failure here.
             PASS_REGULAR_EXPRESSION
             "sen[.]kernel[.]KernelApi +local[.]kernel"
)

# No fixture: the point is the refusal, and CLI11 makes it before anything opens an archive.
add_sen_smoke_test(
  cli_archive_refuses_a_path_that_is_not_there DESCRIPTION "Refuses an archive path that does not exist."
  COMMAND sen::cli_archive info ${CMAKE_CURRENT_BINARY_DIR}/no_such_recording
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_archive
)
set_tests_properties(cli_archive_refuses_a_path_that_is_not_there PROPERTIES WILL_FAIL TRUE)

# merge is the half of the CLI nothing ran. Merging the recording with a copy of itself gives every
# object a duplicate, which is the only way to reach the resolution the terminal asks about.
set(_recording_copy ${CMAKE_CURRENT_BINARY_DIR}/cli_archive_recording_copy)
set(_merged ${CMAKE_CURRENT_BINARY_DIR}/cli_archive_merged)
set(_renamed ${CMAKE_CURRENT_BINARY_DIR}/cli_archive_renamed)

add_test(NAME cli_archive_copy_recording COMMAND ${CMAKE_COMMAND} -E copy_directory ${_recording_dir}
                                                 ${_recording_copy}
)
record_test_description(
  cli_archive_copy_recording "Copies the recording so a merge has two archives to combine."
)
set_tests_properties(
  cli_archive_copy_recording
  PROPERTIES FIXTURES_SETUP
             cli_archive_merge_inputs
             FIXTURES_REQUIRED
             cli_archive_recording
             LABELS
             smoke
)

# The first answer is not a number, so the "enter a number" branch runs before the rest are read.
# Every answer after it is 1: each duplicate asks how to resolve it and then which object to keep,
# and 1 is a valid answer to both, so one file serves however many objects the recording holds.
set(_answers ${CMAKE_CURRENT_BINARY_DIR}/merge_answers.txt)
string(
  REPEAT
  "1\n"
  400
  _ones
)
file(WRITE ${_answers} "nine\n${_ones}")

add_test(
  NAME cli_archive_merge_resolves_duplicates
  COMMAND
    ${CMAKE_COMMAND} -DEXECUTABLE=$<TARGET_FILE:sen::cli_archive> -DARG0=merge -DARG1=${_recording_dir}
    -DARG2=${_recording_copy} -DARG3=-o -DARG4=${_merged} -DARG5=--force -DINPUT_FILE=${_answers}
    -DEXPECT=output -P ${CMAKE_CURRENT_SOURCE_DIR}/run_with_input.cmake
)
record_test_description(
  cli_archive_merge_resolves_duplicates
  "Merges two archives whose objects all collide, answering at the terminal which object to keep."
)
set_tests_properties(
  cli_archive_merge_resolves_duplicates
  PROPERTIES FIXTURES_REQUIRED
             cli_archive_merge_inputs
             LABELS
             smoke
)

# The other half of the menu. Both objects are given the same new name first, which is the branch
# that refuses a rename leaving the collision in place, and the names after it are unique.
set(_rename_answers ${CMAKE_CURRENT_BINARY_DIR}/merge_rename_answers.txt)
file(WRITE ${_rename_answers} "2\nsame\nsame\nkept_from_first\nkept_from_second\n")

add_test(
  NAME cli_archive_merge_renames_duplicates
  COMMAND
    ${CMAKE_COMMAND} -DEXECUTABLE=$<TARGET_FILE:sen::cli_archive> -DARG0=merge -DARG1=${_recording_dir}
    -DARG2=${_recording_copy} -DARG3=-o -DARG4=${_renamed} -DARG5=--force -DINPUT_FILE=${_rename_answers}
    -DEXPECT=output -P ${CMAKE_CURRENT_SOURCE_DIR}/run_with_input.cmake
)
record_test_description(
  cli_archive_merge_renames_duplicates
  "Keeps both of a pair of colliding objects by renaming them, and refuses a rename that leaves them colliding."
)
set_tests_properties(
  cli_archive_merge_renames_duplicates
  PROPERTIES FIXTURES_REQUIRED
             cli_archive_merge_inputs
             LABELS
             smoke
)

# The other two alignments. Each merges the same pair, so each answers the duplicate the same way.
set(_zero_merged ${CMAKE_CURRENT_BINARY_DIR}/cli_archive_zero_merged)
set(_offset_merged ${CMAKE_CURRENT_BINARY_DIR}/cli_archive_offset_merged)

add_test(
  NAME cli_archive_merge_aligns_to_zero
  COMMAND
    ${CMAKE_COMMAND} -DEXECUTABLE=$<TARGET_FILE:sen::cli_archive> -DARG0=merge -DARG1=${_recording_dir}
    -DARG2=${_recording_copy} -DARG3=-o -DARG4=${_zero_merged} -DARG5=--mode -DARG6=zero -DARG7=--force
    -DINPUT_FILE=${_answers} -DEXPECT=output -P ${CMAKE_CURRENT_SOURCE_DIR}/run_with_input.cmake
)
record_test_description(
  cli_archive_merge_aligns_to_zero "Merges two archives with both start times moved to zero."
)
set_tests_properties(
  cli_archive_merge_aligns_to_zero
  PROPERTIES FIXTURES_REQUIRED
             cli_archive_merge_inputs
             LABELS
             smoke
)

add_test(
  NAME cli_archive_merge_aligns_to_an_offset
  COMMAND
    ${CMAKE_COMMAND} -DEXECUTABLE=$<TARGET_FILE:sen::cli_archive> -DARG0=merge -DARG1=${_recording_dir}
    -DARG2=${_recording_copy} -DARG3=-o -DARG4=${_offset_merged} -DARG5=--mode -DARG6=offset -DARG7=--offset
    -DARG8=0 -DARG9=--offset -DARG10=2 -DARG11=--force -DINPUT_FILE=${_answers} -DEXPECT=output -P
    ${CMAKE_CURRENT_SOURCE_DIR}/run_with_input.cmake
)
record_test_description(
  cli_archive_merge_aligns_to_an_offset "Merges two archives, each shifted by the offset given for it."
)
set_tests_properties(
  cli_archive_merge_aligns_to_an_offset
  PROPERTIES FIXTURES_REQUIRED
             cli_archive_merge_inputs
             LABELS
             smoke
)

# A merge driven by a script rather than a person runs out of answers. It has to say so and stop,
# not read whatever happens to be on the terminal next.
set(_short_answers ${CMAKE_CURRENT_BINARY_DIR}/merge_short_answers.txt)
file(WRITE ${_short_answers} "99999999999999999999\n")

add_test(
  NAME cli_archive_merge_refuses_to_run_out_of_answers
  COMMAND
    ${CMAKE_COMMAND} -DEXECUTABLE=$<TARGET_FILE:sen::cli_archive> -DARG0=merge -DARG1=${_recording_dir}
    -DARG2=${_recording_copy} -DARG3=-o -DARG4=${_merged} -DARG5=--force -DINPUT_FILE=${_short_answers} -P
    ${CMAKE_CURRENT_SOURCE_DIR}/run_with_input.cmake
)
record_test_description(
  cli_archive_merge_refuses_to_run_out_of_answers
  "Stops when the answers to a duplicate run out, after refusing one that is not a number it can use."
)
set_tests_properties(
  cli_archive_merge_refuses_to_run_out_of_answers
  PROPERTIES FIXTURES_REQUIRED
             cli_archive_merge_inputs
             WILL_FAIL
             TRUE
             LABELS
             smoke
)

add_sen_smoke_test(
  cli_archive_merge_refuses_an_offset_without_the_mode DESCRIPTION
  "Refuses --offset unless the merge mode is the one that uses it."
  COMMAND sen::cli_archive merge ${_recording_dir} ${_recording_copy} --offset 1.0
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_archive
)
set_tests_properties(
  cli_archive_merge_refuses_an_offset_without_the_mode
  PROPERTIES FIXTURES_REQUIRED
             cli_archive_merge_inputs
             WILL_FAIL
             TRUE
)

add_sen_smoke_test(
  cli_archive_merge_refuses_offset_mode_without_offsets DESCRIPTION
  "Refuses offset mode unless every input archive carries an offset."
  COMMAND sen::cli_archive merge ${_recording_dir} ${_recording_copy} --mode offset --offset 1.0
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_archive
)
set_tests_properties(
  cli_archive_merge_refuses_offset_mode_without_offsets
  PROPERTIES FIXTURES_REQUIRED
             cli_archive_merge_inputs
             WILL_FAIL
             TRUE
)

# CLI11 checks IsMember on --mode, so parseMergeMode's own "unknown merge mode" throw is
# unreachable from the command line. What a user sees is CLI11's refusal, which is what this holds.
add_sen_smoke_test(
  cli_archive_merge_refuses_a_mode_it_does_not_know DESCRIPTION "Refuses a merge mode it does not know."
  COMMAND sen::cli_archive merge ${_recording_dir} ${_recording_copy} --mode sideways
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_archive
)
set_tests_properties(
  cli_archive_merge_refuses_a_mode_it_does_not_know
  PROPERTIES FIXTURES_REQUIRED
             cli_archive_merge_inputs
             WILL_FAIL
             TRUE
)

# A directory that exists but holds no archive gets past CLI11's ExistingDirectory check and fails
# when the reader opens it. Both commands have to say so and exit, rather than carry on over
# whatever they did manage to read.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/not_an_archive)

add_sen_smoke_test(
  cli_archive_info_refuses_a_directory_that_is_not_an_archive DESCRIPTION
  "Refuses a directory that exists and holds no archive."
  COMMAND sen::cli_archive info ${CMAKE_CURRENT_BINARY_DIR}/not_an_archive
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_archive
)
set_tests_properties(cli_archive_info_refuses_a_directory_that_is_not_an_archive PROPERTIES WILL_FAIL TRUE)

add_sen_smoke_test(
  cli_archive_indexed_refuses_a_directory_that_is_not_an_archive DESCRIPTION
  "Refuses to list the objects of a directory that holds no archive."
  COMMAND sen::cli_archive indexed ${CMAKE_CURRENT_BINARY_DIR}/not_an_archive
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} REQ_DEPS cli_archive
)
set_tests_properties(cli_archive_indexed_refuses_a_directory_that_is_not_an_archive PROPERTIES WILL_FAIL TRUE)

# The archive is read through sen::core and sen::db, which are shared libraries in another directory.
foreach(
  _test
  cli_archive_info_reports_a_summary
  cli_archive_indexed_lists_the_objects
  cli_archive_refuses_a_path_that_is_not_there
  cli_archive_merge_refuses_an_offset_without_the_mode
  cli_archive_merge_refuses_offset_mode_without_offsets
  cli_archive_merge_refuses_a_mode_it_does_not_know
  cli_archive_info_refuses_a_directory_that_is_not_an_archive
  cli_archive_indexed_refuses_a_directory_that_is_not_an_archive
)
  if(WIN32)
    append_test_env_modification(${_test} "PATH=path_list_prepend:$<TARGET_FILE_DIR:sen::core>")
  else()
    append_test_env_modification(${_test} "LD_LIBRARY_PATH=path_list_prepend:$<TARGET_FILE_DIR:sen::core>")
  endif()
endforeach()

# The add_sen_* helpers arm the sanitizer options themselves. A raw add_test does not, and a test
# that carries no log_path is one a sanitizer finding cannot reach anybody from, which is what
# check_sanitizer_lane.py fails the lane over. Each of these drives the instrumented binary through
# a cmake script, so a finding under it is reachable and has to be captured.
foreach(
  _test
  cli_archive_clear_recording
  cli_archive_copy_recording
  cli_archive_merge_resolves_duplicates
  cli_archive_merge_renames_duplicates
  cli_archive_merge_aligns_to_zero
  cli_archive_merge_aligns_to_an_offset
  cli_archive_merge_refuses_to_run_out_of_answers
)
  add_sanitizer_options(${_test})
endforeach()
