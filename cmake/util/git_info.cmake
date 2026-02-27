# === git_info.cmake ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# Returns a version string from Git
#
# These functions force a re-configure on each git commit so that you can trust the values of the variables in your
# build system.
#
# get_git_head_revision(<refspecvar> <hashvar> [ALLOW_LOOKING_ABOVE_CMAKE_SOURCE_DIR])
#
# Returns the refspec and sha hash of the current head revision
#
# git_local_changes(<var>)
#
# Returns either "clean" or "dirty" with respect to uncommitted changes. Uses the return code of "git diff-index
# --quiet HEAD --". Does not regard untracked files.

include_guard()

# We must run the following at "include" time, not at function call time, to find the path to this module rather than
# the path to a calling list file
get_filename_component(_gitdescmoddir ${CMAKE_CURRENT_LIST_FILE} PATH)

# Function _git_find_closest_git_dir finds the next closest .git directory that is part of any directory in the path
# defined by _start_dir. The result is returned in the parent scope variable whose name is passed as variable
# _git_dir_var. If no .git directory can be found, the function returns an empty string via _git_dir_var.
#
# Example: Given a path C:/bla/foo/bar and assuming C:/bla/.git exists and neither foo nor bar contain a
# file/directory .git. This wil return C:/bla/.git
#
function(_git_find_closest_git_dir _start_dir _git_dir_var)
  set(cur_dir "${_start_dir}")
  set(git_dir "${_start_dir}/.git")

  while(NOT EXISTS "${git_dir}")
    # .git dir not found, search parent directories
    set(git_previous_parent "${cur_dir}")
    get_filename_component(cur_dir "${cur_dir}" DIRECTORY)
    if(cur_dir STREQUAL git_previous_parent)
      # We have reached the root directory, we are not in git
      set(${_git_dir_var}
          ""
          PARENT_SCOPE
      )
      return()
    endif()
    set(git_dir "${cur_dir}/.git")
  endwhile()

  set(${_git_dir_var}
      "${git_dir}"
      PARENT_SCOPE
  )
endfunction()

function(get_git_head_revision _refspecvar _hashvar)
  _git_find_closest_git_dir("${CMAKE_CURRENT_SOURCE_DIR}" GIT_DIR)

  if("${ARGN}" STREQUAL "ALLOW_LOOKING_ABOVE_CMAKE_SOURCE_DIR")
    set(ALLOW_LOOKING_ABOVE_CMAKE_SOURCE_DIR TRUE)
  else()
    set(ALLOW_LOOKING_ABOVE_CMAKE_SOURCE_DIR FALSE)
  endif()

  if(NOT
     "${GIT_DIR}"
     STREQUAL
     ""
  )
    file(
      RELATIVE_PATH
      _relative_to_source_dir
      "${CMAKE_CURRENT_SOURCE_DIR}"
      "${GIT_DIR}"
    )
    if("${_relative_to_source_dir}" MATCHES "[.][.]" AND NOT ALLOW_LOOKING_ABOVE_CMAKE_SOURCE_DIR)
      # We've gone above the CMake root dir.
      set(GIT_DIR "")
    endif()
  endif()

  if("${GIT_DIR}" STREQUAL "")
    set(${_refspecvar}
        ""
        PARENT_SCOPE
    )
    set(${_hashvar}
        ""
        PARENT_SCOPE
    )
    return()
  endif()

  # Check if the current source dir is a git submodule or a worktree. In both cases .git is a file instead of a
  # directory.
  if(NOT IS_DIRECTORY ${GIT_DIR})

    # The following git command will return a non empty string that points to the super project working tree if the
    # current source dir is inside a git submodule. Otherwise the command will return an empty string.
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" rev-parse --show-superproject-working-tree
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT_VARIABLE out
      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(NOT
       "${out}"
       STREQUAL
       ""
    )
      # If out is empty, GIT_DIR/CMAKE_CURRENT_SOURCE_DIR is in a submodule
      file(READ ${GIT_DIR} submodule)
      string(
        REGEX
        REPLACE "gitdir: (.*)$"
                "\\1"
                GIT_DIR_RELATIVE
                ${submodule}
      )
      string(STRIP ${GIT_DIR_RELATIVE} GIT_DIR_RELATIVE)
      get_filename_component(SUBMODULE_DIR ${GIT_DIR} PATH)
      get_filename_component(GIT_DIR ${SUBMODULE_DIR}/${GIT_DIR_RELATIVE} ABSOLUTE)
      set(HEAD_SOURCE_FILE "${GIT_DIR}/HEAD")
    else()
      # GIT_DIR/CMAKE_CURRENT_SOURCE_DIR is in a worktree
      file(READ ${GIT_DIR} worktree_ref)
      # The .git directory contains a path to the worktree information directory inside the parent git repo of the
      # worktree.
      string(
        REGEX
        REPLACE "gitdir: (.*)$"
                "\\1"
                git_worktree_dir
                ${worktree_ref}
      )
      string(STRIP ${git_worktree_dir} git_worktree_dir)
      _git_find_closest_git_dir("${git_worktree_dir}" GIT_DIR)
      set(HEAD_SOURCE_FILE "${git_worktree_dir}/HEAD")
    endif()
  else()
    set(HEAD_SOURCE_FILE "${GIT_DIR}/HEAD")
  endif()
  set(GIT_DATA "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/git-data")
  if(NOT EXISTS "${GIT_DATA}")
    file(MAKE_DIRECTORY "${GIT_DATA}")
  endif()

  if(NOT EXISTS "${HEAD_SOURCE_FILE}")
    return()
  endif()
  set(HEAD_FILE "${GIT_DATA}/HEAD")
  configure_file("${HEAD_SOURCE_FILE}" "${HEAD_FILE}" COPYONLY)

  configure_file("${_gitdescmoddir}/git_info.cmake.in" "${GIT_DATA}/grab_ref.cmake" @ONLY)
  include("${GIT_DATA}/grab_ref.cmake")

  set(${_refspecvar}
      "${HEAD_REF}"
      PARENT_SCOPE
  )
  set(${_hashvar}
      "${HEAD_HASH}"
      PARENT_SCOPE
  )
endfunction()

function(git_local_changes _var)
  find_package(Git QUIET)

  if(Git_FOUND)
    get_git_head_revision(refspec hash)
    if(NOT GIT_FOUND)
      set(${_var}
          ""
          PARENT_SCOPE
      )
      return()
    endif()
    if(NOT hash)
      set(${_var}
          ""
          PARENT_SCOPE
      )
      return()
    endif()

    execute_process(
      COMMAND "${GIT_EXECUTABLE}" diff-index --quiet HEAD --
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      RESULT_VARIABLE res
      OUTPUT_VARIABLE out
      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(res EQUAL 0)
      set(${_var}
          "clean"
          PARENT_SCOPE
      )
    else()
      set(${_var}
          "dirty"
          PARENT_SCOPE
      )
    endif()
  endif()
endfunction()

function(get_version_from_git version_output_variable)
  find_package(Git QUIET)

  set(fallback_version "0.0.0")
  if(Git_FOUND)
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" describe --tags --always
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      RESULT_VARIABLE res
      OUTPUT_VARIABLE out
      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # returns (major.minor.patch-diff-ghash) 1.2.3-5-gabcd version string with hash
    #                                        ^ ^ ^ ^  ^^^^
    #                                        | | | |   └--  hash
    #                                        | | | └------  diff
    #                                        | | └--------  patch
    #                                        | └----------  minor
    #                                        └------------  major

    if(res EQUAL 0)

      string(FIND ${out} "." dot_found)
      if(dot_found EQUAL -1)
        set(${version_output_variable}
            ${fallback_version}
            PARENT_SCOPE
        )
      else()
        # Split of major.minor.patch
        string(
          REPLACE "-"
                  ";"
                  out
                  ${out}
        )

        list(
          GET
          out
          0
          out
        )
        set(${version_output_variable}
            ${out}
            PARENT_SCOPE
        )
      endif()

    else()
      set(${version_output_variable}
          ${fallback_version}
          PARENT_SCOPE
      )
    endif()
  else()
    set(${version_output_variable}
        ${fallback_version}
        PARENT_SCOPE
    )
  endif()
endfunction()

function(get_version_from_git_raw version_output_variable)
  find_package(Git QUIET)

  set(fallback_version "0.0.0")
  if(Git_FOUND)
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" describe --tags --always
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      RESULT_VARIABLE res
      OUTPUT_VARIABLE out
      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # Returns the version either to the current tag or the
    # last tag with with the diff-commit addition.
    # For example:
    #     current tag → 0.4.2
    #     otherwise   → 0.4.2-5-gc6625265
    #                   ▲ ▲ ▲ ▲  └──┬───┘
    #                   │ │ │ │     └─ hash
    #                   │ │ │ └──────  diff
    #                   │ │ └────────  patch
    #                   │ └──────────  minor
    #                   └────────────  major
    #
    #     5-gc6625265 indicates we are at commit c6625265
    #     and 5 commits have been made since tag 0.4.2

    if(res EQUAL 0)
      set(${version_output_variable}
          ${out}
          PARENT_SCOPE
      )
    else()
      set(${version_output_variable}
          ${fallback_version}
          PARENT_SCOPE
      )
    endif()
  else()
    set(${version_output_variable}
        ${fallback_version}
        PARENT_SCOPE
    )
  endif()
endfunction()

function(get_git_tags tag_list_output_variable)
  find_package(Git QUIET)
  if(NOT GIT_FOUND)
    message(WARNING "Git not found. Could not correctly determine if the current version is tagged.")
  endif()

  execute_process(
    COMMAND ${GIT_EXECUTABLE} tag --points-at
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    RESULT_VARIABLE res
    OUTPUT_VARIABLE out
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(res EQUAL 0)
    if(NOT
       ${out}
       EQUAL
       ""
    )
      string(
        REPLACE "\n"
                ";"
                out
                ${out}
      )
    endif()

    set(${tag_list_output_variable}
        ${out}
        PARENT_SCOPE
    )
  else()
    message(
      WARNING "Git returned error ${res}. Could not correctly determine if the current version is tagged."
    )
  endif()
endfunction()
