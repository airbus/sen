# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

set(_webexplorer_stl_files stl/configuration.stl)

# Bake the frontend production bundle (single self-contained HTML produced by Vite +
# vite-plugin-singlefile in components/webexplorer/frontend/) into a compressed C
# byte array that the component decompresses at runtime via sen::decompressSymbolToString.
# The frontend's own CMakeLists owns the npm build that produces dist/index.html; we add
# an explicit target-level dependency below so the bake step runs after that.
set(_frontend_bundle ${CMAKE_CURRENT_SOURCE_DIR}/../frontend/dist/index.html)
set(_baked_bundle_header ${CMAKE_CURRENT_BINARY_DIR}/generated/explorer_html.h)
sen_file_to_cpp(
  IN
  ${_frontend_bundle}
  OUT
  ${_baked_bundle_header}
  VARNAME
  explorer_html
)

add_sen_package(
  TARGET webexplorer
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "C++ component that serves the @sen/web-explorer frontend bundle over jsonrpc's StaticFileServer"
  SOURCES src/component.cpp
  BASE_PATH ${CMAKE_CURRENT_SOURCE_DIR}
  STL_FILES ${_webexplorer_stl_files}
  PRIVATE_DEPS sen::kernel jsonrpc
  IS_COMPONENT
)

# Make the baked header reachable from component.cpp via `#include "generated/...".
target_include_directories(webexplorer PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Force the bake step (sen_file_to_cpp custom command) to run before component.cpp
# compiles. The intermediate target exists only to attach the dependency without listing
# the not-yet-existing header in webexplorer's SOURCES (add_sen_package scans them at
# configure time for SEN_EXPORT_CLASS, which would error on a missing file).
add_custom_target(webexplorer_baked_bundle DEPENDS ${_baked_bundle_header})
add_dependencies(webexplorer_baked_bundle webexplorer_bundle)
add_dependencies(webexplorer webexplorer_baked_bundle)

sen_enable_static_analysis(webexplorer)

set_target_properties(webexplorer PROPERTIES FOLDER "components/webexplorer/backend")

sen_internal_install(webexplorer)
