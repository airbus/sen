# === run.cmake ========================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================
#
# Installs the fixture package and configures a consumer against it, once per way a consumer can be
# pointed at the prefix. Sen exports its own configs without TARGET_NAME_CMAKEDIRS, so nothing else
# in the repository exercises that path.

set(_work "${WORK_DIR}")
file(REMOVE_RECURSE "${_work}")
file(MAKE_DIRECTORY "${_work}/consumer")

function(_run_or_fail what)
  execute_process(
    COMMAND ${ARGN}
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE _err
  )
  if(NOT
     _rc
     EQUAL
     0
  )
    message(FATAL_ERROR "${what} failed (${_rc}):\n${_out}\n${_err}")
  endif()
endfunction()

_run_or_fail(
  "configuring the fixture"
  ${CMAKE_COMMAND}
  -S
  "${FIXTURE_DIR}"
  -B
  "${_work}/build"
  -DSEN_CMAKE_UTILS_DIR=${SEN_CMAKE_UTILS_DIR}
)
_run_or_fail(
  "installing the fixture"
  ${CMAKE_COMMAND}
  --install
  "${_work}/build"
  --prefix
  "${_work}/prefix"
)

# Both locations need a config and a version file: find_package reads the version file from the
# directory it found the config in.
foreach(_dir "lib/cmake/radar" "cmake/radar")
  foreach(_file "radar-config.cmake" "radar-config-version.cmake")
    if(NOT EXISTS "${_work}/prefix/${_dir}/${_file}")
      message(FATAL_ERROR "the install is missing ${_dir}/${_file}")
    endif()
  endforeach()
endforeach()

file(
  WRITE "${_work}/consumer/CMakeLists.txt"
  "cmake_minimum_required(VERSION 3.21)\n"
  "project(consumer LANGUAGES NONE)\n"
  "find_package(radar \${WANT_VERSION} REQUIRED)\n"
  "if(NOT radar_INSTALL_DIR STREQUAL \"\${EXPECTED_PREFIX}\")\n"
  "  message(FATAL_ERROR \"radar_INSTALL_DIR is '\${radar_INSTALL_DIR}', expected '\${EXPECTED_PREFIX}'\")\n"
  "endif()\n"
)

# The three spellings a consumer is told to use, unversioned and versioned. The third reaches the
# forwarder at the location the previous layout used.
foreach(_entry "${_work}/prefix" "${_work}/prefix/lib/cmake" "${_work}/prefix/cmake")
  foreach(_version "" "1.0")
    file(REMOVE_RECURSE "${_work}/consumer/build")
    _run_or_fail(
      "find_package(radar ${_version}) via ${_entry}"
      ${CMAKE_COMMAND}
      -S
      "${_work}/consumer"
      -B
      "${_work}/consumer/build"
      -DCMAKE_PREFIX_PATH=${_entry}
      -DWANT_VERSION=${_version}
      -DEXPECTED_PREFIX=${_work}/prefix
    )
  endforeach()
endforeach()

# The other half: a version the package cannot satisfy must be refused, or the checks above would
# pass against a version file that says nothing.
file(REMOVE_RECURSE "${_work}/consumer/build")
execute_process(
  COMMAND ${CMAKE_COMMAND} -S "${_work}/consumer" -B "${_work}/consumer/build"
          -DCMAKE_PREFIX_PATH=${_work}/prefix/cmake -DWANT_VERSION=99.0 -DEXPECTED_PREFIX=${_work}/prefix
  RESULT_VARIABLE _rc
  OUTPUT_QUIET ERROR_QUIET
)
if(_rc EQUAL 0)
  message(
    FATAL_ERROR "find_package(radar 99.0) succeeded through the forwarder; the version file is not being read"
  )
endif()

file(REMOVE_RECURSE "${_work}")
message(STATUS "exportable package resolves from the prefix, the cmake directory and the forwarder")
