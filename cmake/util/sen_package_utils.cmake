# === sen_package_utils.cmake ==========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include_guard()

# Adds a sen package that implements Sen generated code. Generated code can be obtained from STL files or XML FOM files
#
# add_sen_package(
#  TARGET <target>
#  [MAINTAINER <maintainer>]
#  [DESCRIPTION <description>]
#  [VERSION <version>]
#  [BASE_PATH base_path]
#  [EXPORT_NAME name of the Sen package exported, defaults to target's name if not set]
#  [SCHEMA_PATH path]
#  [GEN_HDR_FILES variable]
#  [CODEGEN_SETTINGS file]
#  [HLA_OUTPUT_DIR path]
#  [SOURCES [files...]]
#  [DEPS [dependencies...]]
#  [PRIVATE_DEPS [dependencies...]]
#  [STL_FILES [files...]]
#  [HLA_FOM_DIRS [dirs...]]
#  [HLA_MAPPINGS_FILE [files...]]
#  [EXPORTED_CLASSES name of the classes exported by the package (if null, the exported classes are parsed automatically from the source files)]
#  [IS_COMPONENT | NO_SCHEMA | PUBLIC_SYMBOLS]
#  [TEST_TARGET name] if set, a (static) test target is created (it exposes the internal package symbols)
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
      COMMAND sen::cli_gen exp_package ${_export_args}
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

# Creates a package with public symbol visibility
# NOTE: this function is kept for retro-compatibility.
macro(sen_generate_package)
  add_sen_package(${ARGV} PUBLIC_SYMBOLS)
endmacro()

# Version of the add_sen_package function that creates a Sen component
macro(add_sen_component)
  add_sen_package(${ARGV} IS_COMPONENT)
endmacro()
