# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# Frontend: drives the npm build of the React + TypeScript SPA into a single self-contained
# `dist/index.html` (all JS / CSS inlined). The sibling backend/ subdir picks that file up
# and bakes it into a C++ Sen component that registers it with the jsonrpc StaticFileServer
# at runtime.

# npm.cmd first, for the same reason as the @sen/client target: on Windows the plain name
# finds a shell script that Windows cannot run.
find_program(NPM_EXEC NAMES npm.cmd npm)
if(NOT NPM_EXEC)
  message(STATUS "webexplorer: npm not found, skipping build "
                 "(enable -o with_webexplorer=True or install node+npm on the host)"
  )
  return()
endif()

get_filename_component(_node_bin_dir ${NPM_EXEC} DIRECTORY)

set(_web_node_modules_sentinel ${CMAKE_CURRENT_SOURCE_DIR}/node_modules/.package-lock.json)
set(_web_dist_output ${CMAKE_CURRENT_SOURCE_DIR}/dist/index.html)

# `npm ci` so the lockfile is the source of truth (matches the @sen/client target).
add_custom_command(
  OUTPUT ${_web_node_modules_sentinel}
  COMMAND ${CMAKE_COMMAND} -E env --modify "PATH=path_list_prepend:${_node_bin_dir}" ${NPM_EXEC} ci --no-audit
          --no-fund --loglevel=error
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/package.json ${CMAKE_CURRENT_SOURCE_DIR}/package-lock.json
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "npm ci for webexplorer"
  VERBATIM
)
add_custom_target(webexplorer_npm_install DEPENDS ${_web_node_modules_sentinel})

# `npm run build` produces the singlefile dist/index.html. We list it as the OUTPUT so the
# downstream embed step (added in a later phase) can DEPENDS on this command and rebuild only
# when sources change. `index.html` is listed as its own (non-recursive) source - under
# GLOB_RECURSE the bare basename matches the dist/ output too, which produces a self-loop.
file(
  GLOB_RECURSE
  _web_sources
  CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.ts
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.tsx
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.css
)
list(APPEND _web_sources ${CMAKE_CURRENT_SOURCE_DIR}/index.html)

# vite resolves `@sen/client` through the package's exports map into its dist/ - which only
# exists once the client package is built. The dependency must be FILE-level (on the dist
# output in the vite command's DEPENDS): a target-level add_dependencies() on the consuming
# custom target does not order the OUTPUT-producing command under the Ninja generator, and a
# fresh checkout then fails deep in rollup ("failed to resolve import") instead of building
# the client first.
if(NOT TARGET jsonrpc_ts_client_dist)
  message(
    FATAL_ERROR "webexplorer: the frontend bundles @sen/client and needs its dist/ build. "
                "Enable SEN_BUILD_JSONRPC_TS_CLIENT (with npm available) or disable SEN_BUILD_WEBEXPLORER."
  )
endif()
set(_sen_client_dist ${PROJECT_SOURCE_DIR}/components/jsonrpc/clients/typescript/dist/index.js)

set(_web_metafile ${CMAKE_CURRENT_SOURCE_DIR}/dist/meta.json)
add_custom_command(
  OUTPUT ${_web_dist_output} ${_web_metafile}
  COMMAND ${CMAKE_COMMAND} -E env --modify "PATH=path_list_prepend:${_node_bin_dir}" ${NPM_EXEC} run build
          --loglevel=error
  DEPENDS ${_web_node_modules_sentinel} ${_web_sources} ${_sen_client_dist}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Building webexplorer single-file bundle -> ${_web_dist_output}"
  VERBATIM
)
add_custom_target(webexplorer_bundle ALL DEPENDS ${_web_dist_output})
add_dependencies(webexplorer_bundle webexplorer_npm_install jsonrpc_ts_client_dist)
set_target_properties(webexplorer_bundle PROPERTIES FOLDER "components/webexplorer/frontend")

# Full-strictness `tsc --noEmit`. vite builds do not typecheck (esbuild strips types), so
# without this gate a type error ships silently inside the baked bundle.
if(SEN_BUILD_TESTS)
  add_sen_integration_test(
    webexplorer_typecheck
    COMMAND
    ${NPM_EXEC}
    run
    typecheck
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    REQ_DEPS webexplorer_npm_install jsonrpc_ts_client_dist
  )
  append_test_env_modification(webexplorer_typecheck "PATH=path_list_prepend:${_node_bin_dir}")
endif()

sen_internal_bake_npm_licenses(
  TARGET
  "Web Explorer (npm)"
  SOURCE_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}
  NPM_SENTINEL
  ${_web_node_modules_sentinel}
  NPM_EXEC
  ${NPM_EXEC}
  ESBUILD_METAFILE
  ${_web_metafile}
)
