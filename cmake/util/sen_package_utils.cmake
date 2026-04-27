# === sen_package_utils.cmake ==========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include_guard()

# Creates a Sen package: a SHARED library that combines user-written sources with code
# generated from STL or HLA FOM files, and registers the result with the Sen runtime.
# The produced target can be loaded at runtime by sen and participates in the type system.
#
# add_sen_package(
#   TARGET <name>
#     Name of the CMake target to create.
#
#   [MAINTAINER <name>]
#     Person or team responsible for this package. Defaults to "unknown".
#     Note: AUTHOR is a deprecated alias for MAINTAINER.
#
#   [DESCRIPTION <text>]
#     Human-readable description embedded in the package metadata.
#
#   [VERSION <version>]
#     Package version string. Defaults to the CMake project version.
#
#   [BASE_PATH <path>]
#     Root directory used to compute relative paths for generated files and
#     interface resolution. Defaults to CMAKE_CURRENT_SOURCE_DIR.
#
#   [EXPORT_NAME <name>]
#     Name under which the package is registered with the Sen runtime.
#     Defaults to TARGET.
#
#   [SCHEMA_PATH <path>]
#     Directory where the JSON schema describing the package's YAML configuration
#     will be written. Defaults to ${PROJECT_SOURCE_DIR}/schemas.
#
#   [GEN_HDR_FILES <variable>]
#     Output variable. Receives the list of header files produced by code
#     generation, useful for adding them to IDE source groups.
#
#   [CODEGEN_SETTINGS <file>]
#     Path to a settings file forwarded to the code generator (cli_gen).
#
#   [HLA_OUTPUT_DIR <path>]
#     Subdirectory within the generated output directory for HLA-derived files.
#     Only relevant when HLA_FOM_DIRS is used.
#
#   [SOURCES <files...>]
#     C++ source files implementing the package logic.
#
#   [DEPS <targets...>]
#     Public dependencies. Linked with PUBLIC linkage; their exported Sen types
#     are re-exported by this package.
#
#   [PRIVATE_DEPS <targets...>]
#     Private dependencies. Linked with PRIVATE linkage; not visible to consumers.
#
#   [STL_FILES <files...>]
#     Sen Type Language (.stl) files from which C++ code is generated.
#     Mutually exclusive with HLA_FOM_DIRS.
#
#   [HLA_FOM_DIRS <dirs...>]
#     Directories containing HLA FOM XML files from which C++ code is generated.
#     Mutually exclusive with STL_FILES.
#
#   [HLA_MAPPINGS_FILE <files...>]
#     HLA mapping files that customise FOM-to-C++ translation.
#     Requires HLA_FOM_DIRS.
#
#   [EXPORTED_CLASSES <names...>]
#     Explicit list of class names (within the package namespace) to register
#     with the Sen runtime. If omitted, the list is derived automatically by
#     scanning SOURCES for SEN_EXPORT_CLASS() macros.
#
#   [IS_COMPONENT]
#     Mark this package as a Sen component. Enables component-specific code
#     generation (e.g. embedding the component name in the schema).
#
#   [NO_SCHEMA]
#     Skip JSON schema generation. Use this when the package has no YAML
#     configuration interface or when schema generation is handled elsewhere.
#
#   [PUBLIC_SYMBOLS]
#     Export generated code symbols with public visibility. Required when the
#     generated headers are consumed by Python bindings or other shared libraries
#     that need to resolve the symbols at link time.
#
#   [TEST_TARGET <name>]
#     If set, also creates a STATIC library with this name. It links the same
#     sources as TARGET but exposes internal symbols, making them accessible
#     from unit tests without needing a shared-library boundary.
function(add_sen_package)

  set(_options IS_COMPONENT NO_SCHEMA PUBLIC_SYMBOLS)

  set(_one_value_args
      TARGET
      AUTHOR # AUTHOR is deprecated, please use MAINTAINER instead
      MAINTAINER
      DESCRIPTION
      VERSION
      BASE_PATH
      EXPORT_NAME
      SCHEMA_PATH
      GEN_HDR_FILES
      CODEGEN_SETTINGS
      HLA_OUTPUT_DIR
      TEST_TARGET
  )

  set(_multi_value_args
      SOURCES
      DEPS
      PRIVATE_DEPS
      STL_FILES
      HLA_FOM_DIRS
      HLA_MAPPINGS_FILE
      EXPORTED_CLASSES
  )

  cmake_parse_arguments(
    _arg
    "${_options}"
    "${_one_value_args}"
    "${_multi_value_args}"
    ${ARGN}
  )

  if(NOT _arg_TARGET)
    message(FATAL_ERROR "add_sen_package: no TARGET set")
  endif()

  if(NOT _arg_MAINTAINER AND NOT _arg_AUTHOR)
    set(_arg_MAINTAINER "unknown")
  elseif(_arg_AUTHOR AND NOT _arg_MAINTAINER)
    set(_arg_MAINTAINER "${_arg_AUTHOR}")
    message(
      DEPRECATION "${_arg_TARGET} uses still the deprecated AUTHOR argument, please use MAINTAINER instead."
    )
  endif()

  if(NOT _arg_VERSION)
    set(_arg_VERSION ${CMAKE_PROJECT_VERSION})
  endif()

  if(NOT _arg_EXPORT_NAME)
    set(_arg_EXPORT_NAME ${_arg_TARGET})
  endif()

  set(_schema_path ${PROJECT_SOURCE_DIR}/schemas)
  if(_arg_SCHEMA_PATH)
    set(_schema_path ${_arg_SCHEMA_PATH})
  endif()

  if(_arg_BASE_PATH)
    get_filename_component(_abs_base_path ${_arg_BASE_PATH} ABSOLUTE)
  else()
    set(_abs_base_path ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  if(_arg_IS_COMPONENT)
    set(_component_name ${_arg_TARGET})
  endif()

  set(_make_classes_visible NO)
  if(_arg_PUBLIC_SYMBOLS)
    set(_make_classes_visible YES)
  endif()

  set(_target_to_compile ${_arg_TARGET})
  set(_target_type SHARED)
  if(_arg_TEST_TARGET)
    set(_target_to_compile ${_arg_TARGET}_obj)
    set(_target_type OBJECT)
  endif()

  # output dir for the generated files
  set(_output_dir ${CMAKE_CURRENT_BINARY_DIR}/${_arg_TARGET}_generated)

  source_group("sources" FILES ${_arg_SOURCES})

  add_library(${_target_to_compile} ${_target_type} ${_arg_SOURCES})
  sen_configure_target(${_target_to_compile})
  set_target_properties(
    ${_target_to_compile} PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES
  )

  get_git_head_revision(git_ref_spec git_hash ALLOW_LOOKING_ABOVE_CMAKE_SOURCE_DIR)
  git_local_changes(git_status_str)
  target_compile_definitions(
    ${_target_to_compile}
    PRIVATE SEN_TARGET_NAME="${_arg_TARGET}"
            SEN_TARGET_MAINTAINER="${_arg_MAINTAINER}"
            SEN_TARGET_DESCRIPTION="${_arg_DESCRIPTION}"
            SEN_TARGET_VERSION="${_arg_VERSION}"
            SEN_COMPILER_STRING="${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
            GIT_REF_SPEC="${git_ref_spec}"
            GIT_HASH="${git_hash}"
            GIT_STATUS="${git_status_str}"
  )

  # link the target to sen::kernel and sen::core
  target_link_libraries(${_target_to_compile} PUBLIC sen::kernel)

  # set vars needed for the schemas (_exported_classes and _schema_file)
  if(NOT _arg_NO_SCHEMA)
    sen_collect_export_args(
      TARGET
      ${_target_to_compile}
      EXPORT_NAME
      ${_arg_EXPORT_NAME}
      USER_EXPORTED_CLASSES
      ${_arg_EXPORTED_CLASSES}
      EXPORT_CLASSES
      _exported_classes
    )
    set(_schema_file ${_schema_path}/${_arg_TARGET}.json)
    set_property(TARGET ${_target_to_compile} PROPERTY SCHEMA ${_schema_file})
  endif()

  # generate code if needed
  if(_arg_STL_FILES OR _arg_HLA_FOM_DIRS)
    add_library(${_arg_TARGET}_gen OBJECT)
    sen_configure_target(${_arg_TARGET}_gen)
    set_target_properties(
      ${_arg_TARGET}_gen PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES
    )

    if(_arg_STL_FILES)
      sen_generate_cpp(
        TARGET ${_arg_TARGET}_gen
        OUTPUT_DIR ${_output_dir}
        BASE_PATH ${_arg_BASE_PATH}
        STL_FILES ${_arg_STL_FILES}
        CODEGEN_SETTINGS ${_arg_CODEGEN_SETTINGS}
        SCHEMA_FILE ${_schema_file}
        SCHEMA_COMPONENT_NAME ${_component_name}
        GEN_HDR_FILES _gen_hdr_files VISIBLE_CLASSES ${_make_classes_visible}
      )
    else()
      sen_generate_cpp(
        TARGET ${_arg_TARGET}_gen
        OUTPUT_DIR ${_output_dir}
        HLA_OUTPUT_DIR ${_arg_HLA_OUTPUT_DIR}
        BASE_PATH ${_arg_BASE_PATH}
        HLA_FOM_DIRS ${_arg_HLA_FOM_DIRS}
        HLA_MAPPINGS_FILE ${_arg_HLA_MAPPINGS_FILE}
        CODEGEN_SETTINGS ${_arg_CODEGEN_SETTINGS}
        SCHEMA_FILE ${_schema_file}
        GEN_HDR_FILES _gen_hdr_files VISIBLE_CLASSES ${_make_classes_visible}
      )
    endif()

    # link all dependencies to the gen object library as public
    sen_add_dependencies(
      ${_arg_TARGET}_gen
      PRIVATE
      ${_arg_DEPS}
      ${_arg_PRIVATE_DEPS}
    )
    target_link_libraries(${_target_to_compile} PRIVATE ${_arg_TARGET}_gen)
    copy_target_properties(${_target_to_compile} ${_arg_TARGET}_gen)
  endif()

  # link dependencies
  sen_add_dependencies(${_target_to_compile} PUBLIC ${_arg_DEPS})
  sen_add_dependencies(${_target_to_compile} PRIVATE ${_arg_PRIVATE_DEPS})

  get_target_property(_target_exports_types ${_target_to_compile} SEN_EXPORTS_TYPES)
  if(_target_exports_types)
    # collect export args and generate exports file
    file(MAKE_DIRECTORY ${_output_dir})

    set(_exports_file "${_output_dir}/sen_exported_types.cpp")

    sen_collect_export_args(
      TARGET
      ${_target_to_compile}
      EXPORT_NAME
      ${_arg_EXPORT_NAME}
      EXPORT_ARGS
      _export_args
      EXPORTED_CLASSES
      ${_arg_EXPORTED_CLASSES}
      SOURCES
      ${_arg_SOURCES}
    )

    # generate the exports file
    add_custom_command(
      OUTPUT ${_exports_file}
      COMMAND_EXPAND_LISTS VERBATIM
      COMMAND sen::cli_gen cpp exports ${_export_args}
      DEPENDS ${_gen_files} sen::cli_gen
      WORKING_DIRECTORY ${_output_dir}
      COMMENT "Generating exports sen code for ${_arg_TARGET}"
    )

    # add the generated sources to the library
    target_sources(${_target_to_compile} PRIVATE ${_exports_file})
  endif()

  # output the list of generated header files
  if(_arg_GEN_HDR_FILES)
    set(${_arg_GEN_HDR_FILES}
        ${_gen_hdr_files}
        PARENT_SCOPE
    )
  endif()

  # create the final shared target
  if(NOT TARGET ${_arg_TARGET})
    add_library(${_arg_TARGET} SHARED)
    sen_configure_target(${_arg_TARGET})
    if(TARGET ${_arg_TARGET}_gen)
      target_link_libraries(${_arg_TARGET} PRIVATE ${_arg_TARGET}_gen)
    endif()
    target_link_libraries(${_arg_TARGET} PRIVATE ${_arg_TARGET}_obj)
    copy_target_properties(${_arg_TARGET} ${_arg_TARGET}_obj)
    set_property(
      TARGET ${_arg_TARGET}
      APPEND
      PROPERTY OBJECT_LIBRARY ${_arg_TARGET}_obj
    )
    target_link_libraries(${_arg_TARGET} PUBLIC ${_arg_DEPS})
    target_link_libraries(${_arg_TARGET} PRIVATE ${_arg_PRIVATE_DEPS})
  endif()

  # create final test target (static) if specified
  if(_arg_TEST_TARGET)
    add_library(${_arg_TEST_TARGET} STATIC)
    sen_configure_target(${_arg_TEST_TARGET})
    if(TARGET ${_arg_TARGET}_gen)
      target_link_libraries(${_arg_TEST_TARGET} PRIVATE ${_arg_TARGET}_gen)
    endif()
    target_link_libraries(${_arg_TEST_TARGET} PRIVATE ${_arg_TARGET}_obj)
    copy_target_properties(${_arg_TEST_TARGET} ${_arg_TARGET}_obj)
    target_link_libraries(${_arg_TEST_TARGET} PUBLIC ${_arg_DEPS})
    target_link_libraries(${_arg_TEST_TARGET} PRIVATE ${_arg_PRIVATE_DEPS})
  endif()

endfunction()

# Deprecated wrapper for add_sen_package(... PUBLIC_SYMBOLS).
# Kept for backwards compatibility â€” prefer add_sen_package() with PUBLIC_SYMBOLS directly.
macro(sen_generate_package)
  add_sen_package(${ARGV} PUBLIC_SYMBOLS)
endmacro()

# Convenience wrapper for add_sen_package(... IS_COMPONENT).
# Use this instead of add_sen_package() when creating a Sen component.
macro(add_sen_component)
  add_sen_package(${ARGV} IS_COMPONENT)
endmacro()
