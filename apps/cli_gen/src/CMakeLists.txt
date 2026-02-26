# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/cli11)
include(tps/json)
include(tps/inja)
include(tps/spdlog)

# -------------------------------------------------------------------------------------------------------------
# CPP Generator
# -------------------------------------------------------------------------------------------------------------

set(cpp_templates
    cpp/templates/struct_decl.j2
    cpp/templates/struct_impl.j2
    cpp/templates/enum_decl.j2
    cpp/templates/enum_impl.j2
    cpp/templates/variant_decl.j2
    cpp/templates/variant_impl.j2
    cpp/templates/sequence_decl.j2
    cpp/templates/sequence_impl.j2
    cpp/templates/optional_decl.j2
    cpp/templates/optional_impl.j2
    cpp/templates/alias_decl.j2
    cpp/templates/quantity_decl.j2
    cpp/templates/quantity_impl.j2
    cpp/templates/class_decl.j2
    cpp/templates/class_impl.j2
    cpp/templates/file_types.j2
    cpp/templates/file_impl.j2
    cpp/templates/file_package_exports.j2
)

sen_internal_generate_template_headers(
  OUTDIR
  ${CMAKE_CURRENT_BINARY_DIR}/cli_gen/cpp_templates
  GEN_FILE_LIST
  cpp_template_dumps
  TEMPLATE_FILES
  ${cpp_templates}
)

set(cpp_headers
    cpp/cpp_generator.h
    cpp/cpp_templates.h
    cpp/json_type_storage.h
    cpp/util.h
)

set(cpp_sources
    cpp/cpp_generator.cpp
    cpp/cpp_templates.cpp
    cpp/json_type_storage.cpp
    cpp/util.cpp
)

source_group("cpp/headers" FILES ${cpp_headers})
source_group("cpp/sources" FILES ${cpp_sources})

# -------------------------------------------------------------------------------------------------------------
# PlantUML Generator
# -------------------------------------------------------------------------------------------------------------

set(plantuml_templates
    plantuml/templates/class_decl.j2
    plantuml/templates/enum_decl.j2
    plantuml/templates/struct_decl.j2
    plantuml/templates/variant_decl.j2
    plantuml/templates/file_decl.j2
)

sen_internal_generate_template_headers(
  OUTDIR
  ${CMAKE_CURRENT_BINARY_DIR}/cli_gen/plantuml_templates
  GEN_FILE_LIST
  plantuml_template_dumps
  TEMPLATE_FILES
  ${plantuml_templates}
)

set(plantuml_headers plantuml/plantuml_generator.h)

set(plantuml_sources plantuml/plantuml_generator.cpp)

source_group("plantuml/headers" FILES ${plantuml_headers})
source_group("plantuml/sources" FILES ${plantuml_sources})

# -------------------------------------------------------------------------------------------------------------
# MKDocs Generator
# -------------------------------------------------------------------------------------------------------------

set(mkdocs_templates
    mkdocs/templates/alias_doc.j2
    mkdocs/templates/optional_doc.j2
    mkdocs/templates/class_doc.j2
    mkdocs/templates/class_diagram.j2
    mkdocs/templates/enum_doc.j2
    mkdocs/templates/file_doc.j2
    mkdocs/templates/quantity_doc.j2
    mkdocs/templates/sequence_doc.j2
    mkdocs/templates/struct_doc.j2
    mkdocs/templates/variant_doc.j2
)

sen_internal_generate_template_headers(
  OUTDIR
  ${CMAKE_CURRENT_BINARY_DIR}/cli_gen/mkdocs_templates
  GEN_FILE_LIST
  mkdocs_template_dumps
  TEMPLATE_FILES
  ${mkdocs_templates}
)

set(mkdocs_headers mkdocs/mkdocs_generator.h)
set(mkdocs_sources mkdocs/mkdocs_generator.cpp mkdocs/classdoc_tree.cpp mkdocs/classdoc_tree.h)

source_group("mkdocs/headers" FILES ${mkdocs_headers})
source_group("mkdocs/sources" FILES ${mkdocs_sources})

# -------------------------------------------------------------------------------------------------------------
# Python Generator
# -------------------------------------------------------------------------------------------------------------

set(python_templates
    python/templates/class_decl.j2
    python/templates/enum_decl.j2
    python/templates/struct_decl.j2
    python/templates/sequence_decl.j2
    python/templates/variant_decl.j2
    python/templates/alias_decl.j2
    python/templates/optional_decl.j2
    python/templates/quantity_decl.j2
    python/templates/file_decl.j2
)

sen_internal_generate_template_headers(
  OUTDIR
  ${CMAKE_CURRENT_BINARY_DIR}/cli_gen/python_templates
  GEN_FILE_LIST
  python_template_dumps
  TEMPLATE_FILES
  ${python_templates}
)

set(python_headers python/python_generator.h)
set(python_sources python/python_generator.cpp)

source_group("python/headers" FILES ${python_headers})
source_group("plantuml/sources" FILES ${python_sources})

# -------------------------------------------------------------------------------------------------------------
# JSON Generator
# -------------------------------------------------------------------------------------------------------------

set(json_templates
    json/templates/struct_def.j2
    json/templates/enum_def.j2
    json/templates/variant_def.j2
    json/templates/sequence_def.j2
    json/templates/optional_def.j2
    json/templates/alias_def.j2
    json/templates/quantity_def.j2
    json/templates/class_def.j2
    json/templates/file_schema.j2
    json/templates/config_schema.j2
    ${PROJECT_SOURCE_DIR}/libs/kernel/stl/sen/kernel/log.stl
    ${PROJECT_SOURCE_DIR}/libs/kernel/stl/sen/kernel/basic_types.stl
)

sen_internal_generate_template_headers(
  OUTDIR
  ${CMAKE_CURRENT_BINARY_DIR}/cli_gen/json_templates
  GEN_FILE_LIST
  json_template_dumps
  TEMPLATE_FILES
  ${json_templates}
)

set(json_headers json/json_generator.h json/json_templates.h)

set(json_sources json/json_generator.cpp json/json_templates.cpp)

source_group("json/headers" FILES ${json_headers})
source_group("json/sources" FILES ${json_sources})

# -------------------------------------------------------------------------------------------------------------
# Executable
# -------------------------------------------------------------------------------------------------------------

add_executable(
  cli_gen
  ${cpp_headers}
  ${cpp_sources}
  ${cpp_template_dumps}
  ${mkdocs_headers}
  ${mkdocs_sources}
  ${mkdocs_template_dumps}
  ${plantuml_headers}
  ${plantuml_sources}
  ${plantuml_template_dumps}
  ${python_headers}
  ${python_sources}
  ${python_template_dumps}
  ${json_headers}
  ${json_sources}
  ${json_template_dumps}
  main.cpp
)

sen_enable_static_analysis(cli_gen)

target_link_libraries(
  cli_gen
  PUBLIC sen::core
  PRIVATE $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
  PRIVATE $<BUILD_INTERFACE:pantor::inja>
  PRIVATE $<BUILD_INTERFACE:CLI11::CLI11>
  PRIVATE $<BUILD_INTERFACE:spdlog::spdlog>
)

add_executable(sen::cli_gen ALIAS cli_gen)
sen_internal_configure_app(cli_gen)
sen_internal_install(cli_gen)

sen_internal_add_resources(
  TARGET
  cli_gen
  GROUP
  "cpp/templates"
  RESOURCE_FILES
  ${cpp_templates}
)

source_group("cpp/templates/dumps" FILES ${cpp_template_dumps})

sen_internal_add_resources(
  TARGET
  cli_gen
  GROUP
  "plantuml/templates"
  RESOURCE_FILES
  ${plantuml_templates}
)

target_include_directories(
  cli_gen
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/cli_gen
)

source_group("plantuml/templates/dumps" FILES ${plantuml_template_dumps})

sen_internal_add_resources(
  TARGET
  cli_gen
  GROUP
  "python/templates"
  RESOURCE_FILES
  ${python_templates}
)

source_group("python/templates/dumps" FILES ${python_template_dumps})

sen_internal_add_resources(
  TARGET
  cli_gen
  GROUP
  "mkdocs/templates"
  RESOURCE_FILES
  ${mkdocs_templates}
)

source_group("mkdocs/templates/dumps" FILES ${mkdocs_template_dumps})

sen_internal_add_resources(
  TARGET
  cli_gen
  GROUP
  "json/templates"
  RESOURCE_FILES
  ${json_templates}
)

source_group("json/templates/dumps" FILES ${json_template_dumps})
