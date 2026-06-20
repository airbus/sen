# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# Lets people keep jsonrpc C++ but skip the node toolchain.
option(SEN_BUILD_JSONRPC_TS_CLIENT "Build the @sen/client TypeScript package" ON)
if(NOT SEN_BUILD_JSONRPC_TS_CLIENT)
  return()
endif()

# Generate the TypeScript value-type modules consumed by the @sen/client npm package from the
# project's STL files (one .ts per STL plus an index.ts barrel, into src/generated/). The output
# is gitignored; this target produces it as part of the regular build, so the npm-side
# `tsc`/`vite`/`vitest` invocations see fresh types whenever an STL changes.
set(_ts_client_out_dir ${CMAKE_CURRENT_SOURCE_DIR}/src/generated)
set(_ts_client_stls ${PROJECT_SOURCE_DIR}/components/jsonrpc/stl/jsonrpc.stl)

# Hardcoded now until we have the full typescript generation pipeline.
set(_ts_client_outputs
    ${_ts_client_out_dir}/index.ts
    ${_ts_client_out_dir}/jsonrpc.ts
    ${_ts_client_out_dir}/type_specs.ts
    ${_ts_client_out_dir}/basic_types.ts
    ${_ts_client_out_dir}/log.ts
)

# The kernel STL include path is supplied explicitly; jsonrpc.stl `import`s into sen.kernel.*.
add_custom_command(
  OUTPUT ${_ts_client_outputs}
  COMMAND_EXPAND_LISTS VERBATIM
  COMMAND sen::cli_gen ts stl ${_ts_client_stls} -i ${PROJECT_SOURCE_DIR}/libs/kernel -d ${_ts_client_out_dir}
  DEPENDS ${_ts_client_stls} sen::cli_gen
  COMMENT "Generating @sen/client TypeScript types -> ${_ts_client_out_dir}"
)

add_custom_target(jsonrpc_ts_client_codegen ALL DEPENDS ${_ts_client_outputs})
add_dependencies(jsonrpc_ts_client_codegen sen::cli_gen)
set_target_properties(jsonrpc_ts_client_codegen PROPERTIES FOLDER "components/jsonrpc")

# Everything below needs the node toolchain; codegen above stays available without it.
find_program(NPM_EXEC npm)
if(NOT NPM_EXEC)
  message(STATUS "jsonrpc TS client: npm not found; skipping package install/build/tests")
  return()
endif()

set(_ts_client_node_modules_sentinel ${CMAKE_CURRENT_SOURCE_DIR}/node_modules/.package-lock.json)

# `npm` is a node script whose shebang needs node on PATH. The Conan toolchain places node in
# its own bin dir; we surface that on PATH at build-command level via `cmake -E env --modify
# PATH=path_list_prepend:...`, which is platform-aware (handles `:` vs `;` separator and
# quoting). Requires CMake >= 3.25; Sen pins 3.28.1 via Conan tool_requires.
get_filename_component(_node_bin_dir ${NPM_EXEC} DIRECTORY)

# `npm ci` (not `npm install`) so the lockfile is the source of truth: faster, deterministic,
# and refuses to silently rewrite package-lock.json when a `^`-range admits a newer subdep.
add_custom_command(
  OUTPUT ${_ts_client_node_modules_sentinel}
  COMMAND ${CMAKE_COMMAND} -E env --modify "PATH=path_list_prepend:${_node_bin_dir}" ${NPM_EXEC} ci
          --no-audit --no-fund --silent
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/package.json ${CMAKE_CURRENT_SOURCE_DIR}/package-lock.json
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "npm ci for @sen/client"
  VERBATIM
)
add_custom_target(jsonrpc_ts_client_npm_install DEPENDS ${_ts_client_node_modules_sentinel})

# Production bundle (dist/). The package's `exports` map resolves `@sen/client` into dist/,
# so anything that bundles against the package (web_explorer's frontend) must depend on
# this target — a fresh checkout cannot rely on a manually-built dist/ lying around.
file(
  GLOB_RECURSE
  _ts_client_sources
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.ts
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.tsx
)
set(_ts_client_dist_output ${CMAKE_CURRENT_SOURCE_DIR}/dist/index.js)
add_custom_command(
  OUTPUT ${_ts_client_dist_output}
  COMMAND ${CMAKE_COMMAND} -E env --modify "PATH=path_list_prepend:${_node_bin_dir}" ${NPM_EXEC} run build
          --silent
  DEPENDS ${_ts_client_node_modules_sentinel} ${_ts_client_outputs} ${_ts_client_sources}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Building @sen/client bundle -> dist/"
  VERBATIM
)
add_custom_target(jsonrpc_ts_client_dist DEPENDS ${_ts_client_dist_output})
add_dependencies(jsonrpc_ts_client_dist jsonrpc_ts_client_codegen jsonrpc_ts_client_npm_install)
set_target_properties(jsonrpc_ts_client_dist PROPERTIES FOLDER "components/jsonrpc")

# Tests (opt-in). Gated on SEN_BUILD_TESTS because REQ_DEPS includes the `inheritance` test
# fixture target, which only exists when tests are enabled.
if(SEN_BUILD_TESTS)
  # globalSetup reads SEN_BINARY and SEN_CONFIG from the environment (set below) to spawn the
  # Sen subprocess; both are absolute so the test is cwd-independent.
  add_sen_integration_test(
    jsonrpc_ts_client_integration
    COMMAND
    ${NPM_EXEC}
    run
    test:integration
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    REQ_DEPS jsonrpc jsonrpc_ts_client_codegen jsonrpc_ts_client_npm_install inheritance
  )
  set_tests_properties(jsonrpc_ts_client_integration PROPERTIES RUN_SERIAL TRUE)
  append_test_env_modification(
    jsonrpc_ts_client_integration
    "SEN_BINARY=set:$<TARGET_FILE:sen::cli_sen>"
    "SEN_CONFIG=set:${CMAKE_CURRENT_SOURCE_DIR}/test/integration/configs/inheritance.yaml"
    "PATH=path_list_prepend:${_node_bin_dir}"
  )

  # Full-strictness `tsc --noEmit` over prod + test configs. vite builds do not typecheck
  # (esbuild strips types), so without this gate a type error ships silently.
  add_sen_integration_test(
    jsonrpc_ts_client_typecheck
    COMMAND
    ${NPM_EXEC}
    run
    typecheck
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    REQ_DEPS jsonrpc_ts_client_codegen jsonrpc_ts_client_npm_install
  )
  append_test_env_modification(jsonrpc_ts_client_typecheck "PATH=path_list_prepend:${_node_bin_dir}")
else()
  message(STATUS "jsonrpc TS client: tests disabled, skipping test registration")
endif()
