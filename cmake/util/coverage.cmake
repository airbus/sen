# === coverage.cmake ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

if(NOT SEN_COVERAGE_ENABLE)
  message(STATUS "Coverage tracking disabled")
  return()
endif()

if(MSVC)
  # Nothing to configure: OpenCppCoverage reads the .pdb while the suite runs, so it needs no
  # instrumentation and no target here. The workflow wraps ctest with it. A Release leg still
  # needs SEN_RELEASE_SYMBOLS for the program database to exist at all.
  return()
endif()

file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/coverage_reports/" SEN_COVERAGE_REPORT_DIR)
file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/coverage_data/" SEN_COVERAGE_DATA_DIR)

# What no figure counts, shared with the MSVC legs through a file because they reach it from the
# workflow rather than from here. The file holds wildcards, which is what OpenCppCoverage takes;
# gcovr and llvm-cov want regular expressions, so each pattern is translated here. Dots are
# escaped before the star is expanded, or the escape would be expanded in turn.
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/coverage-exclusions.txt" SEN_COVERAGE_IGNORE_WILDCARDS)
list(
  FILTER
  SEN_COVERAGE_IGNORE_WILDCARDS
  EXCLUDE
  REGEX
  "^(#|$)"
)
foreach(wildcard IN LISTS SEN_COVERAGE_IGNORE_WILDCARDS)
  string(
    REPLACE "."
            "[.]"
            _pattern
            "${wildcard}"
  )
  string(
    REPLACE "*"
            ".*"
            _pattern
            "${_pattern}"
  )
  list(APPEND SEN_COVERAGE_IGNORE_PATTERNS "${_pattern}")
endforeach()
# Appended here because only this file knows where the build tree is.
list(APPEND SEN_COVERAGE_IGNORE_PATTERNS "${CMAKE_BINARY_DIR}/.*")
list(
  JOIN
  SEN_COVERAGE_IGNORE_PATTERNS
  "|"
  SEN_COVERAGE_IGNORE_REGEX
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  find_program(GCOV_PATH gcovr REQUIRED)
  find_program(LCOV_PATH lcov REQUIRED)
  find_program(GENHTML_PATH genhtml REQUIRED)

  make_directory(${SEN_COVERAGE_DATA_DIR})

  add_custom_target(
    generate-coverage-data
    COMMAND ${GCOV_PATH} -r ${CMAKE_SOURCE_DIR} -j 8 --cobertura ${SEN_COVERAGE_DATA_DIR}coverage.xml
            --print-summary --gcov-ignore-parse-errors --exclude "${SEN_COVERAGE_IGNORE_REGEX}"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS run_tests
  )

  # The same report over a suite that already ran, which is how the workflow uses it: ctest is
  # invoked there rather than through run_tests, because on Windows cmake --build does not
  # return once ctest finishes and every leg is kept on one path.
  add_custom_target(
    collect-coverage
    COMMAND ${GCOV_PATH} -r ${CMAKE_SOURCE_DIR} -j 8 --cobertura ${SEN_COVERAGE_DATA_DIR}coverage.xml
            --print-summary --gcov-ignore-parse-errors --exclude "${SEN_COVERAGE_IGNORE_REGEX}"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    VERBATIM
  )

  add_custom_target(
    clean-generate-coverage-data
    COMMENT "Remove old coverage/lcov files to prevent miss alignment."
    COMMAND find ${CMAKE_BINARY_DIR} -type f -name '*.gcda' -exec rm {} +
    COMMAND ${LCOV_PATH} -d . --zerocounters
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )

  add_custom_target(
    generate-coverage-report
    COMMAND ${LCOV_PATH} -d . --capture --no-external --rc lcov_branch_coverage=1 -b ${CMAKE_SOURCE_DIR} -o
            coverage.info
    COMMAND ${LCOV_PATH} -r coverage.info --rc lcov_branch_coverage=1 -o filtered_coverage.info
            '/usr/include/*' '*/*_generated/*' '*/test/*' '${CMAKE_BINARY_DIR}/*'
    COMMAND ${GENHTML_PATH} -o ${SEN_COVERAGE_REPORT_DIR} filtered_coverage.info --legend --rc
            lcov_branch_coverage=1
    COMMAND rm -rf coverage.info filtered_coverage.info
    COMMAND cmake --build ${CMAKE_BINARY_DIR} --target clean-generate-coverage-data
    COMMAND echo "Generated coverage overview [see: ${SEN_COVERAGE_REPORT_DIR}index.html]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS generate-coverage-data
  )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  get_filename_component(COMPILER_DIRECTORY ${CMAKE_CXX_COMPILER} DIRECTORY)
  string(
    REGEX MATCH
          "[0-9]+"
          _llvm_major
          "${CMAKE_CXX_COMPILER_VERSION}"
  )
  find_program(
    LLVM_PROFDATA_PATH
    NAMES llvm-profdata "llvm-profdata-${_llvm_major}"
    HINTS ${COMPILER_DIRECTORY} "/usr/lib/llvm-${_llvm_major}/bin"
  )
  find_program(
    LLVM_COV_PATH
    NAMES llvm-cov "llvm-cov-${_llvm_major}"
    HINTS ${COMPILER_DIRECTORY} "/usr/lib/llvm-${_llvm_major}/bin"
  )
  find_package(Python3 REQUIRED COMPONENTS Interpreter)

  if(NOT LLVM_PROFDATA_PATH OR NOT LLVM_COV_PATH)
    message(FATAL_ERROR "Coverage was enabled but llvm-profdata or llvm-cov could not be found. "
                        "Install llvm-${_llvm_major}, or configure without SEN_COVERAGE_ENABLE."
    )
  endif()

  set(SEN_COVERAGE_TARGETS
      ""
      CACHE STRING "Targets for which code coverage should be generated."
  )
  mark_as_advanced(SEN_COVERAGE_TARGETS)

  if(NOT SEN_COVERAGE_TARGETS)
    get_property(SEN_IMPLICIT_COVERAGE_TARGETS GLOBAL PROPERTY SEN_INTERNAL_TARGETS)
    get_property(SEN_IMPLICIT_COMPONENTS_TARGETS GLOBAL PROPERTY SEN_INTERNAL_COMPONENT_TARGETS)
  endif()
  foreach(target ${SEN_COVERAGE_TARGETS} ${SEN_IMPLICIT_COVERAGE_TARGETS} ${SEN_IMPLICIT_COMPONENTS_TARGETS})
    get_target_property(target_type ${target} TYPE)
    if("${target_type}" STREQUAL "SHARED_LIBRARY" OR "${target_type}" STREQUAL "EXECUTABLE")
      list(APPEND coverage_binaries $<TARGET_FILE:${target}>)
    endif()
  endforeach()

  file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/cmake/util/generate_coverage_report.py"
       GENERATE_COVERAGE_REPORT_SCRIPT
  )

  add_custom_target(
    clean-generate-coverage-data COMMAND ${CMAKE_COMMAND} -E remove_directory ${SEN_COVERAGE_DATA_DIR}
  )

  add_custom_target(
    generate-coverage-report
    COMMAND
      ${Python3_EXECUTABLE} ${GENERATE_COVERAGE_REPORT_SCRIPT} ${LLVM_PROFDATA_PATH} ${LLVM_COV_PATH}
      ${SEN_COVERAGE_DATA_DIR} ${SEN_COVERAGE_REPORT_DIR} ${coverage_binaries} --ignore-filename-regex
      "${SEN_COVERAGE_IGNORE_REGEX}"
    COMMAND echo "Generated coverage overview [see: ${SEN_COVERAGE_REPORT_DIR}index.html]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS run_tests
    VERBATIM
  )

  # Same report, over a suite that already ran. See the GNU branch for why the workflow needs it.
  add_custom_target(
    collect-coverage
    COMMAND
      ${Python3_EXECUTABLE} ${GENERATE_COVERAGE_REPORT_SCRIPT} ${LLVM_PROFDATA_PATH} ${LLVM_COV_PATH}
      ${SEN_COVERAGE_DATA_DIR} ${SEN_COVERAGE_REPORT_DIR} ${coverage_binaries} --ignore-filename-regex
      "${SEN_COVERAGE_IGNORE_REGEX}"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    VERBATIM
  )
else()
  message(STATUS "Coverage setup not supported.")
endif()
