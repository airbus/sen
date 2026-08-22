# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# Sen-to-MCP gateway. Node/TypeScript on @sen/client. Toggle: SEN_BUILD_MCP_GATEWAY.

if(NOT SEN_BUILD_JSONRPC)
  return()
endif()

option(SEN_BUILD_MCP_GATEWAY "Build the @sen/mcp-gateway Node app" ON)
if(NOT SEN_BUILD_MCP_GATEWAY)
  return()
endif()

# npm.cmd first, for the same reason as the @sen/client target: on Windows the plain name finds
# a shell script that Windows cannot run. Own cache variable, so this lookup is not silently
# satisfied by whichever sibling filled a shared one first.
find_program(MCP_GATEWAY_NPM_EXEC NAMES npm.cmd npm)
if(NOT MCP_GATEWAY_NPM_EXEC)
  message(STATUS "mcp_gateway: npm not found, skipping target registration")
  return()
endif()

# npm's shebang needs node on PATH; surface Conan's node dir at command level (CMake >= 3.25).
get_filename_component(_mcp_gateway_node_bin_dir ${MCP_GATEWAY_NPM_EXEC} DIRECTORY)
set(_mcp_gateway_path_env --modify "PATH=path_list_prepend:${_mcp_gateway_node_bin_dir}")

set(_mcp_gateway_node_modules_sentinel ${CMAKE_CURRENT_SOURCE_DIR}/node_modules/.package-lock.json)
add_custom_command(
  OUTPUT ${_mcp_gateway_node_modules_sentinel}
  # --loglevel=error, not --silent: --silent also hides the reason a failure happened,
  # which leaves the build log with nothing to act on.
  COMMAND ${CMAKE_COMMAND} -E env ${_mcp_gateway_path_env} ${MCP_GATEWAY_NPM_EXEC} ci --no-audit --no-fund
          --loglevel=error
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/package.json ${CMAKE_CURRENT_SOURCE_DIR}/package-lock.json
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "npm ci for @sen/mcp-gateway"
  VERBATIM
)
add_custom_target(mcp_gateway_npm_install DEPENDS ${_mcp_gateway_node_modules_sentinel})

file(
  GLOB_RECURSE
  _mcp_gateway_ts_sources
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.ts
)
file(
  GLOB
  _mcp_gateway_doc_users_guide
  CONFIGURE_DEPENDS
  ${PROJECT_SOURCE_DIR}/docs/users_guide/*.md
)
file(
  GLOB
  _mcp_gateway_doc_components
  CONFIGURE_DEPENDS
  ${PROJECT_SOURCE_DIR}/docs/components/*.md
)
file(
  GLOB
  _mcp_gateway_doc_howto
  CONFIGURE_DEPENDS
  ${PROJECT_SOURCE_DIR}/docs/howto_guides/*.md
)
# baked_*.ts are build artifacts: exclude them so we don't loop (writing them triggers a re-run).
list(
  FILTER
  _mcp_gateway_ts_sources
  EXCLUDE
  REGEX
  "/baked_(docs|package)\\.ts$"
)

# Bakes the docs corpus and wire identity into the gitignored src/baked_*.ts that tools.ts and
# index.ts import. Declared as an OUTPUT so the bundle, the unit suite and the typecheck depend
# on it by file rather than on whoever ran first.
set(_mcp_gateway_baked ${CMAKE_CURRENT_SOURCE_DIR}/src/baked_docs.ts
                       ${CMAKE_CURRENT_SOURCE_DIR}/src/baked_package.ts
)
add_custom_command(
  OUTPUT ${_mcp_gateway_baked}
  COMMAND ${CMAKE_COMMAND} -E env SEN_MCP_VERSION=${CMAKE_PROJECT_VERSION} ${_mcp_gateway_path_env}
          ${MCP_GATEWAY_NPM_EXEC} run bake --loglevel=error
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/bake_docs.mjs
          ${CMAKE_CURRENT_SOURCE_DIR}/package.json
          ${_mcp_gateway_doc_users_guide}
          ${_mcp_gateway_doc_components}
          ${_mcp_gateway_doc_howto}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "bake docs + identity for @sen/mcp-gateway"
  VERBATIM
)
add_custom_target(mcp_gateway_bake DEPENDS ${_mcp_gateway_baked})
set_target_properties(mcp_gateway_bake PROPERTIES FOLDER "apps")

# esbuild bundle: src/ + deps + baked docs -> dist/sen-mcp-gateway.cjs (+ .map). `npm run build`
# re-runs the bake through its `prebuild` hook, so it is handed the same SEN_MCP_VERSION.
set(_mcp_gateway_bundle ${CMAKE_CURRENT_SOURCE_DIR}/dist/sen-mcp-gateway.cjs)
set(_mcp_gateway_metafile ${CMAKE_CURRENT_SOURCE_DIR}/dist/meta.json)

# esbuild resolves @sen/client into its dist/, which exists only once that package is built.
# The dependency has to be FILE-level: a target-level add_dependencies() does not order an
# OUTPUT-producing command under Ninja, and a fresh checkout then fails inside esbuild.
if(NOT TARGET jsonrpc_ts_client_dist)
  message(
    FATAL_ERROR "mcp_gateway: the gateway bundles @sen/client and needs its dist/ build. "
                "Enable SEN_BUILD_JSONRPC_TS_CLIENT (with npm available) or disable SEN_BUILD_MCP_GATEWAY."
  )
endif()
set(_mcp_gateway_sen_client_dist ${PROJECT_SOURCE_DIR}/components/jsonrpc/clients/typescript/dist/index.js)

add_custom_command(
  OUTPUT ${_mcp_gateway_bundle} ${_mcp_gateway_metafile}
  COMMAND ${CMAKE_COMMAND} -E env SEN_MCP_VERSION=${CMAKE_PROJECT_VERSION} ${_mcp_gateway_path_env}
          ${MCP_GATEWAY_NPM_EXEC} run build --loglevel=error
  DEPENDS ${_mcp_gateway_node_modules_sentinel}
          ${_mcp_gateway_sen_client_dist}
          ${_mcp_gateway_baked}
          ${CMAKE_CURRENT_SOURCE_DIR}/package.json
          ${CMAKE_CURRENT_SOURCE_DIR}/tsconfig.json
          ${_mcp_gateway_ts_sources}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "bundle @sen/mcp-gateway"
  VERBATIM
)
add_custom_target(mcp_gateway_build ALL DEPENDS ${_mcp_gateway_bundle})
add_dependencies(
  mcp_gateway_build
  mcp_gateway_npm_install
  mcp_gateway_bake
  jsonrpc_ts_client_dist
)
set_target_properties(mcp_gateway_build PROPERTIES FOLDER "apps")

# Bake bundled-dep licenses; the metafile narrows the bake to what the .cjs actually ships.
sen_internal_bake_npm_licenses(
  TARGET
  "MCP Gateway (npm)"
  SOURCE_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}
  NPM_SENTINEL
  ${_mcp_gateway_node_modules_sentinel}
  NPM_EXEC
  ${MCP_GATEWAY_NPM_EXEC}
  ESBUILD_METAFILE
  ${_mcp_gateway_metafile}
)

# Both launchers ship everywhere: the MCP host spawns the one its platform can run, and an
# installed tree is routinely produced on one OS and consumed on another (containers, shares).
install(
  PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/bin/sen-mcp-gateway ${CMAKE_CURRENT_SOURCE_DIR}/bin/sen-mcp-gateway.cmd
  DESTINATION ${CMAKE_INSTALL_BINDIR}
  COMPONENT mcp_gateway
)
install(
  FILES ${_mcp_gateway_bundle} ${_mcp_gateway_bundle}.map
  DESTINATION ${CMAKE_INSTALL_BINDIR}
  COMPONENT mcp_gateway
)
install(
  DIRECTORY "${CMAKE_BINARY_DIR}/foss_licenses/MCP Gateway (npm)"
  DESTINATION foss_licenses
  COMPONENT mcp_gateway
  OPTIONAL
)

# Integration: globalSetup spawns Sen against inheritance.yaml (shared with @sen/client).
# Gated on SEN_BUILD_TESTS because REQ_DEPS references `my_package` and `inheritance` test
# fixture targets that only exist when tests are enabled.
if(SEN_BUILD_TESTS)
  add_sen_integration_test(
    mcp_gateway_integration
    COMMAND
    ${MCP_GATEWAY_NPM_EXEC}
    run
    test:integration
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    REQ_DEPS jsonrpc mcp_gateway_npm_install mcp_gateway_build my_package inheritance
  )
  # ctest's global --timeout is 20s, which every other test clears easily. Both vitest suites pay
  # for a node runtime before asserting anything, and this one also spawns a Sen kernel: ~18s of
  # that budget in a Debug build.
  set_tests_properties(
    mcp_gateway_integration
    PROPERTIES RUN_SERIAL
               TRUE
               TIMEOUT
               120
  )
  append_test_env_modification(
    mcp_gateway_integration
    "GATEWAY_ENTRYPOINT=set:${_mcp_gateway_bundle}"
    "SEN_BINARY=set:$<TARGET_FILE:sen::cli_sen>"
    "SEN_CONFIG=set:${CMAKE_CURRENT_SOURCE_DIR}/test/integration/configs/inheritance.yaml"
    "PATH=path_list_prepend:${_mcp_gateway_node_bin_dir}"
  )

  # Unit suite (no Sen subprocess). Needs the bake and the @sen/client dist: without either,
  # the imports fail to resolve and vitest reports a collection error, not a test failure.
  add_sen_integration_test(
    mcp_gateway_unit
    COMMAND
    ${MCP_GATEWAY_NPM_EXEC}
    run
    test
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    REQ_DEPS mcp_gateway_npm_install mcp_gateway_bake jsonrpc_ts_client_dist
  )
  append_test_env_modification(mcp_gateway_unit "PATH=path_list_prepend:${_mcp_gateway_node_bin_dir}")

  # Full-strictness `tsc --noEmit` over prod + test configs. esbuild strips types without
  # checking them, so without this gate a type error ships inside the bundle.
  # Serial: its `pretypecheck` bake rewrites the src/baked_*.ts that mcp_gateway_unit reads.
  add_sen_integration_test(
    mcp_gateway_typecheck
    COMMAND
    ${MCP_GATEWAY_NPM_EXEC}
    run
    typecheck
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    REQ_DEPS mcp_gateway_npm_install mcp_gateway_bake jsonrpc_ts_client_dist
  )
  # add_sen_integration_test labels everything "integration". This suite is unit tests, and the
  # label decides more than naming: nightly runs `ctest -L unit --repeat until-fail:5` to hunt
  # intermittent tests, and the timing-sensitive cases here are exactly what that loop is for.
  # TIMEOUT for the reason given on the integration suite above; nightly repeats this one five times.
  set_tests_properties(
    mcp_gateway_unit
    PROPERTIES LABELS
               "unit"
               TIMEOUT
               120
  )
  set_tests_properties(mcp_gateway_typecheck PROPERTIES LABELS "unit")
  set_tests_properties(mcp_gateway_typecheck PROPERTIES RUN_SERIAL TRUE)
  # Same SEN_MCP_VERSION as the build, so the re-bake leaves the same identity behind.
  append_test_env_modification(
    mcp_gateway_typecheck "PATH=path_list_prepend:${_mcp_gateway_node_bin_dir}"
    "SEN_MCP_VERSION=set:${CMAKE_PROJECT_VERSION}"
  )
endif()
