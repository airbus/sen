# === run.cmake ========================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

function(run)
  execute_process(
    COMMAND ${ARGV}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
  )
  if(NOT
     result
     EQUAL
     0
  )
    message(FATAL_ERROR "Command failed (${result}): ${ARGV}\n${output}\n${error}")
  endif()
endfunction()

set(configure_args
    -G
    "${TEST_GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${TEST_COMPILER}"
    "-DCMAKE_BUILD_TYPE=${TEST_CONFIG}"
)
if(TEST_PLATFORM)
  list(
    APPEND
    configure_args
    -A
    "${TEST_PLATFORM}"
  )
endif()
if(TEST_TOOLSET)
  list(
    APPEND
    configure_args
    -T
    "${TEST_TOOLSET}"
  )
endif()
set(producer "${TEST_BINARY_DIR}/producer")
set(prefix "${TEST_BINARY_DIR}/install")
# A repeat run must also exercise generated-header ordering, rather than finding headers
# left by the previous build. Restrict removal to the four outputs owned by this fixture.
file(
  REMOVE
  "${producer}/ordinary_generated/generated.h"
  "${producer}/ordinary_generated/generated.cpp"
  "${producer}/split_generated/generated.h"
  "${producer}/split_generated/generated.cpp"
)
run("${CMAKE_COMMAND}"
    -S
    "${TEST_SOURCE_DIR}"
    -B
    "${producer}"
    ${configure_args}
    "-DSEN_SOURCE_DIR=${SEN_SOURCE_DIR}"
    "-DCMAKE_INSTALL_PREFIX=${prefix}"
)
run("${CMAKE_COMMAND}"
    --build
    "${producer}"
    --config
    "${TEST_CONFIG}"
    --parallel
    4
)
run("${CMAKE_CTEST_COMMAND}"
    --test-dir
    "${producer}"
    -C
    "${TEST_CONFIG}"
    --output-on-failure
)
run("${CMAKE_COMMAND}"
    --install
    "${producer}"
    --config
    "${TEST_CONFIG}"
)

foreach(mode build install)
  if(mode STREQUAL build)
    set(export_file "${producer}/fixture-targets.cmake")
  else()
    set(export_file "${prefix}/lib/cmake/fixture/sen_targets.cmake")
  endif()
  file(READ "${export_file}" exported)
  if(exported MATCHES "(ordinary_gen|split_gen|split_obj)([\",;> ]|$)|private_dependency")
    message(FATAL_ERROR "${mode} export exposes an internal target:\n${exported}")
  endif()
  set(consumer "${TEST_BINARY_DIR}/consumer-${mode}")
  run("${CMAKE_COMMAND}"
      -S
      "${TEST_SOURCE_DIR}/consumer"
      -B
      "${consumer}"
      ${configure_args}
      "-DFIXTURE_EXPORT=${export_file}"
  )
  run("${CMAKE_COMMAND}"
      --build
      "${consumer}"
      --config
      "${TEST_CONFIG}"
      --parallel
      4
  )
  run("${CMAKE_CTEST_COMMAND}"
      --test-dir
      "${consumer}"
      -C
      "${TEST_CONFIG}"
      --output-on-failure
  )
endforeach()
