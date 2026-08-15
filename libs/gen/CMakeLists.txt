# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/json)
include(tps/inja)

# Shared substrate for all generators in libs/gen.
set(common_sources
    src/common/type_storage.cpp
    src/common/type_storage.h
    src/common/util.cpp
    src/common/util.h
)

# -------------------------------------------------------------------------------------------------------------
# JSON Generator
# -------------------------------------------------------------------------------------------------------------

file(
  GLOB
  json_templates
  CONFIGURE_DEPENDS
  "src/json/templates/*.j2"
)

sen_internal_generate_template_headers(
  OUTDIR
  ${CMAKE_CURRENT_BINARY_DIR}/gen/json_templates
  GEN_FILE_LIST
  json_template_dumps
  TEMPLATE_FILES
  ${json_templates}
)

# Kernel STL sources baked into the .so so getKernelSchema can resolve
# `sen.kernel.*` types from process memory. Lives under `kernel_stls/` rather than
# `json_templates/` so the symbol path matches the content.
set(kernel_baked_stls ${PROJECT_SOURCE_DIR}/libs/kernel/stl/sen/kernel/log.stl
                      ${PROJECT_SOURCE_DIR}/libs/kernel/stl/sen/kernel/basic_types.stl
)
sen_internal_generate_template_headers(
  OUTDIR
  ${CMAKE_CURRENT_BINARY_DIR}/gen/kernel_stls
  GEN_FILE_LIST
  kernel_stl_dumps
  TEMPLATE_FILES
  ${kernel_baked_stls}
)

set(json_public_headers include/sen/gen/json.h)
set(json_sources src/json/json_generator.cpp src/json/json_templates.cpp src/json/json_templates.h)

# -------------------------------------------------------------------------------------------------------------
# Python Generator
# -------------------------------------------------------------------------------------------------------------

file(
  GLOB
  python_templates
  CONFIGURE_DEPENDS
  "src/python/templates/*.j2"
)

sen_internal_generate_template_headers(
  OUTDIR
  ${CMAKE_CURRENT_BINARY_DIR}/gen/python_templates
  GEN_FILE_LIST
  python_template_dumps
  TEMPLATE_FILES
  ${python_templates}
)

set(python_public_headers include/sen/gen/python.h)
set(python_sources src/python/python_generator.cpp src/python/python_templates.cpp
                   src/python/python_templates.h
)

# -------------------------------------------------------------------------------------------------------------
# PlantUML Generator
# -------------------------------------------------------------------------------------------------------------

file(
  GLOB
  plantuml_templates
  CONFIGURE_DEPENDS
  "src/plantuml/templates/*.j2"
)

sen_internal_generate_template_headers(
  OUTDIR
  ${CMAKE_CURRENT_BINARY_DIR}/gen/plantuml_templates
  GEN_FILE_LIST
  plantuml_template_dumps
  TEMPLATE_FILES
  ${plantuml_templates}
)

set(plantuml_public_headers include/sen/gen/plantuml.h)
set(plantuml_sources src/plantuml/plantuml_generator.cpp src/plantuml/plantuml_templates.cpp
                     src/plantuml/plantuml_templates.h
)

# -------------------------------------------------------------------------------------------------------------
# MKDocs Generator
# -------------------------------------------------------------------------------------------------------------

file(
  GLOB
  mkdocs_templates
  CONFIGURE_DEPENDS
  "src/mkdocs/templates/*.j2"
)

sen_internal_generate_template_headers(
  OUTDIR
  ${CMAKE_CURRENT_BINARY_DIR}/gen/mkdocs_templates
  GEN_FILE_LIST
  mkdocs_template_dumps
  TEMPLATE_FILES
  ${mkdocs_templates}
)

set(mkdocs_public_headers include/sen/gen/mkdocs.h)
set(mkdocs_sources
    src/mkdocs/mkdocs_generator.cpp
    src/mkdocs/mkdocs_templates.cpp
    src/mkdocs/mkdocs_templates.h
    src/mkdocs/classdoc_tree.cpp
    src/mkdocs/classdoc_tree.h
)

# -------------------------------------------------------------------------------------------------------------
# CPP Generator
# -------------------------------------------------------------------------------------------------------------

file(
  GLOB
  cpp_templates
  CONFIGURE_DEPENDS
  "src/cpp/templates/*.j2"
)

sen_internal_generate_template_headers(
  OUTDIR
  ${CMAKE_CURRENT_BINARY_DIR}/gen/cpp_templates
  GEN_FILE_LIST
  cpp_template_dumps
  TEMPLATE_FILES
  ${cpp_templates}
)

set(cpp_public_headers include/sen/gen/cpp.h)
set(cpp_sources src/cpp/cpp_generator.cpp src/cpp/cpp_templates.cpp src/cpp/cpp_templates.h)

# -------------------------------------------------------------------------------------------------------------
# TypeScript Generator
# -------------------------------------------------------------------------------------------------------------

file(
  GLOB
  typescript_templates
  CONFIGURE_DEPENDS
  "src/typescript/templates/*.j2"
)

sen_internal_generate_template_headers(
  OUTDIR
  ${CMAKE_CURRENT_BINARY_DIR}/gen/typescript_templates
  GEN_FILE_LIST
  typescript_template_dumps
  TEMPLATE_FILES
  ${typescript_templates}
)

set(typescript_public_headers include/sen/gen/typescript.h)
set(typescript_sources src/typescript/typescript_generator.cpp src/typescript/typescript_templates.cpp
                       src/typescript/typescript_templates.h
)

# -------------------------------------------------------------------------------------------------------------
# Library
# -------------------------------------------------------------------------------------------------------------

add_library(
  gen SHARED
  ${common_sources}
  ${json_public_headers}
  ${json_sources}
  ${json_template_dumps}
  ${kernel_stl_dumps}
  ${python_public_headers}
  ${python_sources}
  ${python_template_dumps}
  ${plantuml_public_headers}
  ${plantuml_sources}
  ${plantuml_template_dumps}
  ${mkdocs_public_headers}
  ${mkdocs_sources}
  ${mkdocs_template_dumps}
  ${cpp_public_headers}
  ${cpp_sources}
  ${cpp_template_dumps}
  ${typescript_public_headers}
  ${typescript_sources}
  ${typescript_template_dumps}
)
add_library(sen::gen ALIAS gen)
sen_internal_configure_lib(gen)
sen_enable_static_analysis(gen)

if(WIN32)
  set_property(TARGET gen PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS 1)
endif()

target_include_directories(
  gen
  PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/libs/gen/include>"
  PUBLIC "$<INSTALL_INTERFACE:libs/gen/include>"
  PRIVATE src
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/gen
)

# 3rd party deps stay PRIVATE so they don't leak into consumers (Sen guideline: no 3rd party
# deps in public headers). Public consumers see only Sen types + std types.
target_link_libraries(
  gen
  PUBLIC sen::core
  PRIVATE $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
  PRIVATE $<BUILD_INTERFACE:pantor::inja>
)

source_group("common" FILES ${common_sources})
source_group("json/public" FILES ${json_public_headers})
source_group("json/sources" FILES ${json_sources})
source_group("json/templates" FILES ${json_templates})
source_group("json/templates/dumps" FILES ${json_template_dumps})
source_group("python/public" FILES ${python_public_headers})
source_group("python/sources" FILES ${python_sources})
source_group("python/templates" FILES ${python_templates})
source_group("python/templates/dumps" FILES ${python_template_dumps})
source_group("plantuml/public" FILES ${plantuml_public_headers})
source_group("plantuml/sources" FILES ${plantuml_sources})
source_group("plantuml/templates" FILES ${plantuml_templates})
source_group("plantuml/templates/dumps" FILES ${plantuml_template_dumps})
source_group("mkdocs/public" FILES ${mkdocs_public_headers})
source_group("mkdocs/sources" FILES ${mkdocs_sources})
source_group("mkdocs/templates" FILES ${mkdocs_templates})
source_group("mkdocs/templates/dumps" FILES ${mkdocs_template_dumps})
source_group("cpp/public" FILES ${cpp_public_headers})
source_group("cpp/sources" FILES ${cpp_sources})
source_group("cpp/templates" FILES ${cpp_templates})
source_group("cpp/templates/dumps" FILES ${cpp_template_dumps})
source_group("typescript/public" FILES ${typescript_public_headers})
source_group("typescript/sources" FILES ${typescript_sources})
source_group("typescript/templates" FILES ${typescript_templates})
source_group("typescript/templates/dumps" FILES ${typescript_template_dumps})

sen_internal_install(gen)

install(
  FILES ${cpp_public_headers}
        ${json_public_headers}
        ${python_public_headers}
        ${plantuml_public_headers}
        ${mkdocs_public_headers}
        ${typescript_public_headers}
  DESTINATION libs/gen/include/sen/gen
)

if(SEN_BUILD_TESTS)
  add_subdirectory(test)
endif()
