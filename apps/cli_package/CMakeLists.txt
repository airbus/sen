# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/cli11)
include(tps/json)
include(tps/inja)

set(package_templates
    templates/package/cmakelists_txt.j2
    templates/package/basic_types_stl.j2
    templates/package/basic_class_stl.j2
    templates/package/basic_class_header.j2
    templates/package/basic_class_source.j2
    templates/package/config_yaml.j2
)

set(component_templates
    templates/component/cmakelists_txt.j2
    templates/component/component_full_cpp.j2
    templates/component/component_basic_cpp.j2
    templates/component/component_with_cfg_cpp.j2
    templates/component/config_stl.j2
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/templates/package)
sen_internal_generate_template_headers(
  OUTDIR
  ${CMAKE_CURRENT_BINARY_DIR}/templates/package
  GEN_FILE_LIST
  template_dumps_package
  TEMPLATE_FILES
  ${package_templates}
)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/templates/component)
sen_internal_generate_template_headers(
  OUTDIR
  ${CMAKE_CURRENT_BINARY_DIR}/templates/component
  GEN_FILE_LIST
  template_dumps_component
  TEMPLATE_FILES
  ${component_templates}
)

add_executable(
  cli_package
  ${template_dumps_component}
  ${template_dumps_package}
  init_component.cpp
  init_package.cpp
  main.cpp
  util.cpp
  util.h
)

target_link_libraries(
  cli_package
  PRIVATE sen::core
  PRIVATE $<BUILD_INTERFACE:CLI11::CLI11>
  PRIVATE $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
  PRIVATE $<BUILD_INTERFACE:pantor::inja>
)

add_executable(sen::cli_package ALIAS cli_package)
sen_internal_configure_app(cli_package)
sen_internal_install(cli_package)

target_include_directories(
  cli_package
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/cli_package
)

source_group("templates/package/headers" FILES ${template_dumps_package})
source_group("templates/component/headers" FILES ${template_dumps_component})

sen_internal_add_resources(
  TARGET
  cli_package
  GROUP
  "templates/package"
  RESOURCE_FILES
  ${package_templates}
)

sen_internal_add_resources(
  TARGET
  cli_package
  GROUP
  "templates/component"
  RESOURCE_FILES
  ${component_templates}
)

sen_enable_static_analysis(cli_package)
