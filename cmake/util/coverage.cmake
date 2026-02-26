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
  # Coverage tracking not supported for MSVC
  return()
endif()

file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/coverage_reports/" SEN_COVERAGE_REPORT_DIR)
file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/coverage_data/" SEN_COVERAGE_DATA_DIR)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  find_program(GCOV_PATH gcovr REQUIRED)
  find_program(LCOV_PATH lcov REQUIRED)
  find_program(GENHTML_PATH genhtml REQUIRED)

  make_directory(${SEN_COVERAGE_DATA_DIR})

  add_custom_target(
    generate-coverage-data
    COMMAND ${GCOV_PATH} -r ${CMAKE_SOURCE_DIR} -j 8 --cobertura ${SEN_COVERAGE_DATA_DIR}coverage.xml
            --print-summary --gcov-ignore-parse-errors
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS run_tests
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
            '/usr/include/*' '*/*_generated/*' '*/test/*'
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
  find_program(
    LLVM_PROFDATA_PATH
    llvm-profdata
    ${COMPILER_DIRECTORY}
    NO_DEFAULT_PATH
  )
  find_program(
    LLVM_COV_PATH
    llvm-cov
    ${COMPILER_DIRECTORY}
    NO_DEFAULT_PATH
  )
  find_package(Python3 REQUIRED COMPONENTS Interpreter)

  if(NOT LLVM_PROFDATA_PATH OR NOT LLVM_COV_PATH)
    message(WARNING "Could not find llvm-profdata or llvm-cov, skipping coverage generation.")
    return()
  endif()

  set(SEN_COVERAGE_TARGETS
      ""
      CACHE STRING "Targets for which code coverage should be generated."
  )
  mark_as_advanced(SEN_COVERAGE_TARGETS)

  if(NOT SEN_COVERAGE_TARGETS)
    get_property(SEN_IMPLICIT_COVERAGE_TARGETS GLOBAL PROPERTY SEN_INTERNAL_TARGETS)
  endif()
  foreach(target ${SEN_COVERAGE_TARGETS} ${SEN_IMPLICIT_COVERAGE_TARGETS})
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
      ${SEN_COVERAGE_DATA_DIR} ${SEN_COVERAGE_REPORT_DIR} ${coverage_binaries}
      --ignore-filename-regex=".*generated.*"
    COMMAND echo "Generated coverage overview [see: ${SEN_COVERAGE_REPORT_DIR}index.html]"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS run_tests
  )
else()
  message(STATUS "Coverage setup not supported.")
endif()
