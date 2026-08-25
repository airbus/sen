# === benchmark.cmake ==================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# Benchmark initialization code. Benchmarks come with the full mode, which sets
# SEN_BUILD_BENCHMARKS, and live in a benchmark/ directory next to a library's test/
# directory. Configure with -DSEN_BUILD_BENCHMARKS=OFF to leave them out entirely.

if(SEN_BUILD_BENCHMARKS)
  find_package(benchmark REQUIRED)
endif()

add_custom_target(run_benchmarks)

# add_sen_benchmark(
#   <name> <sources>...
#   [LINK_DEPS <deps>...]
# )
#
# Registers a google-benchmark executable and hooks it into the
# run_benchmarks target. Each run writes <name>.json next to the binary so
# results can be archived and compared across runs.
function(add_sen_benchmark benchmark_name)
  if(NOT SEN_BUILD_BENCHMARKS)
    return()
  endif()

  set(_options)
  set(_one_value_args)
  set(_multi_value_args LINK_DEPS)

  cmake_parse_arguments(
    _arg
    "${_options}"
    "${_one_value_args}"
    "${_multi_value_args}"
    ${ARGN}
  )

  # Left out of the default target: run_benchmarks is what builds them, so a full
  # build does not pay for benchmarks nobody is going to run.
  add_executable(${benchmark_name} EXCLUDE_FROM_ALL ${_arg_UNPARSED_ARGUMENTS})
  target_link_libraries(${benchmark_name} PRIVATE benchmark::benchmark_main ${_arg_LINK_DEPS})

  # The runner is a per-benchmark target: commands can only attach to targets
  # created in the same directory, and run_benchmarks lives at the top level.
  add_custom_target(
    run_${benchmark_name}
    COMMAND ${benchmark_name} "--benchmark_out=$<TARGET_FILE:${benchmark_name}>.json"
            "--benchmark_out_format=json"
    VERBATIM USES_TERMINAL
  )
  add_dependencies(run_${benchmark_name} ${benchmark_name})
  add_dependencies(run_benchmarks run_${benchmark_name})
endfunction()
