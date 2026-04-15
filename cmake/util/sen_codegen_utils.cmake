# === sen_codegen_utils.cmake ==========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include_guard()

# Generates code from STL or HLA FOM files and adds the result to an existing target.
# The target must already exist before calling this function.
# For the common case of building a full Sen package, prefer add_sen_package() instead.
#
# sen_generate_code(
#   TARGET <name>
#     An already-created CMake target to which generated sources will be added.
#
#   [OUTPUT_DIR <path>]
#     Directory where generated files are written.
#     Defaults to ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_generated.
#
#   [BASE_PATH <path>]
#     Root directory for import resolution and relative-path computation.
#     Defaults to CMAKE_CURRENT_SOURCE_DIR.
#
#   [LANG <cpp|py>]
#     Output language for generated code. Defaults to cpp.
#     Use py to generate Python bindings instead of C++ headers.
#
#   [CODEGEN_SETTINGS <file>]
#     Path to a settings file forwarded to the code generator (cli_gen).
#
#   [GEN_HDR_FILES <variable>]
#     Output variable. Receives the list of generated header files,
#     useful for adding them to IDE source groups.
#
#   [SCHEMA_FILE <path>]
#     If set, triggers JSON schema generation and writes the schema to this path.
#
#   [SCHEMA_COMPONENT_NAME <name>]
#     When set alongside SCHEMA_FILE, the schema is generated as a component
#     schema (rather than a package schema) with this name embedded.
#
#   [HLA_OUTPUT_DIR <path>]
#     Subdirectory within OUTPUT_DIR for HLA-derived files.
#     Only relevant when HLA_FOM_DIRS is used.
#
#   [STL_FILES <files...>]
#     Sen Type Language (.stl) files from which code is generated.
#     Mutually exclusive with HLA_FOM_DIRS.
#
#   [HLA_FOM_DIRS <dirs...>]
#     Directories containing HLA FOM XML files from which code is generated.
#     Mutually exclusive with STL_FILES.
#
#   [HLA_MAPPINGS_FILE <files...>]
#     HLA mapping files that customise FOM-to-code translation.
#     Requires HLA_FOM_DIRS.
#
#   [VISIBLE_CLASSES <YES|NO>]
#     Whether generated class symbols are exported with public visibility.
#     Defaults to NO. Set to YES when the generated headers must be consumed
#     across shared-library boundaries (e.g. Python bindings).
function(sen_generate_code)

  set(_one_value_args
      TARGET
      OUTPUT_DIR
      BASE_PATH
      LANG
      CODEGEN_SETTINGS
      GEN_HDR_FILES
      SCHEMA_FILE
      SCHEMA_COMPONENT_NAME
      HLA_OUTPUT_DIR
      VISIBLE_CLASSES
  )
  set(_multi_value_args STL_FILES HLA_FOM_DIRS HLA_MAPPINGS_FILE)

  cmake_parse_arguments(
    _arg
    "${_options}"
    "${_one_value_args}"
    "${_multi_value_args}"
    ${ARGN}
  )

  if(NOT _arg_TARGET)
    message(FATAL_ERROR "  sen_generate_code: no TARGET set")
  endif()

  if(NOT TARGET ${_arg_TARGET})
    message(
      FATAL_ERROR
        "  sen_generate_code: specified TARGET has not been created. Define the target before calling this function"
    )
  endif()

  if(_arg_STL_FILES AND _arg_HLA_FOM_DIRS)
    message(FATAL_ERROR "  sen_generate_code: STL_FILES and HLA_FOM_DIRS cannot be present at the same time")
  endif()

  if(_arg_HLA_MAPPINGS_FILE AND NOT _arg_HLA_FOM_DIRS)
    message(
      FATAL_ERROR "  sen_generate_code: HLA_MAPPINGS_FILE is defined, but no HLA_FOM_DIRS were specified"
    )
  endif()

  if(NOT _arg_LANG)
    set(_arg_LANG cpp)
  endif()

  sen_configure_target(${_arg_TARGET})
  target_link_libraries(${_arg_TARGET} PRIVATE sen::core)

  # output dir for the generated files
  if(_arg_OUTPUT_DIR
     AND (NOT
          _arg_OUTPUT_DIR
          STREQUAL
          ""
         )
  )
    set(_output_dir ${_arg_OUTPUT_DIR})
  else()
    set(_output_dir ${CMAKE_CURRENT_BINARY_DIR}/${_arg_TARGET}_generated)
  endif()

  file(MAKE_DIRECTORY ${_output_dir})

  # include generated code dirs to target include directories
  target_include_directories(${_arg_TARGET} PUBLIC "$<BUILD_INTERFACE:${_output_dir}>")

  # configure json schema generation args
  set(_generate_schema NO)
  if(DEFINED _arg_SCHEMA_FILE
     AND (NOT
          _arg_SCHEMA_FILE
          STREQUAL
          ""
         )
  )
    set(_generate_schema YES)
    set(_schema_type "package")
    if(DEFINED _arg_SCHEMA_COMPONENT_NAME
       AND (NOT
            _arg_SCHEMA_COMPONENT_NAME
            STREQUAL
            ""
           )
    )
      set(_schema_type "component")
      set(_component_name_opt "-n ${_arg_SCHEMA_COMPONENT_NAME}")
    endif()
  endif()

  # get the absolute base path
  if(DEFINED _arg_BASE_PATH AND NOT (_arg_BASE_PATH STREQUAL ""))
    get_filename_component(_abs_base_path ${_arg_BASE_PATH} ABSOLUTE)
  else()
    set(_abs_base_path ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  set_property(TARGET ${_arg_TARGET} PROPERTY BASE_PATH ${_abs_base_path})

  # set the include path as extra options for the code generation
  set_property(
    TARGET ${_arg_TARGET}
    APPEND
    PROPERTY SEN_IMPORT_DIRS -i ${_abs_base_path}
  )

  # set the public symbols option
  if(_arg_LANG STREQUAL cpp AND _arg_VISIBLE_CLASSES)
    set(_public_symbols_opt "--public-symbols")
  endif()

  # generated headers to be returned in the end
  set(_gen_hdr_files)

  # process the input codegen settings
  if(DEFINED _arg_CODEGEN_SETTINGS AND NOT (_arg_CODEGEN_SETTINGS STREQUAL ""))
    get_filename_component(_abs_settings_path ${_arg_CODEGEN_SETTINGS} ABSOLUTE)
    set(_settings_file_option --settings ${_abs_settings_path})
  endif()

  if(_arg_STL_FILES)

    # get any imported dirs
    set(_extra_options "$<TARGET_PROPERTY:${_arg_TARGET},SEN_IMPORT_DIRS>")

    foreach(_stl_file ${_arg_STL_FILES})

      # get the absolute path to the sen file
      get_filename_component(_abs_stl_file ${_stl_file} ABSOLUTE)
      get_filename_component(_stl_file_name ${_stl_file} NAME)
      set_property(
        TARGET ${_arg_TARGET}
        APPEND
        PROPERTY STL_FILES ${_abs_stl_file}
      )

      # compute output dir for this file, we start with the general out dir
      set(_file_output_dir ${_output_dir})

      file(
        RELATIVE_PATH
        _rel_base
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${_abs_base_path}
      )
      file(
        RELATIVE_PATH
        _rel_input
        ${_abs_base_path}
        ${_abs_stl_file}
      )

      get_filename_component(
        _diff
        ${_rel_input}
        DIRECTORY
        BASE_DIR
        ${_rel_base}
      )

      set(_file_include_dir "${_file_output_dir}")
      set(_file_output_dir "${_file_output_dir}/${_diff}")
      set(_output_file_prefix "${_file_output_dir}/${_stl_file_name}")

      set(_base_path_opt)
      if(NOT
         ${_diff}
         STREQUAL
         ""
      )
        set(_base_path_opt -b ${_diff}/)
      endif()

      # create output directory
      file(MAKE_DIRECTORY ${_file_output_dir})

      if(_arg_LANG STREQUAL py)
        file(WRITE "${_file_output_dir}/__init__.py" "")
        file(WRITE "${_file_output_dir}/py.typed" "")
      endif()

      # compute the files for this .stl
      if(${_arg_LANG} STREQUAL cpp)
        set(_file_output_headers ${_output_file_prefix}.h)
        set(_file_output_files ${_file_output_headers} ${_output_file_prefix}.cpp)
      elseif(${_arg_LANG} STREQUAL py)
        get_filename_component(_stl_file_name_we ${_stl_file} NAME_WE)
        # replace forbidden characters with _ to comply with python naming standards
        string(
          REPLACE -
                  _
                  _stl_file_name_we
                  ${_stl_file_name_we}
        )
        string(
          REPLACE .
                  _
                  _stl_file_name_we
                  ${_stl_file_name_we}
        )
        set(_file_output_headers ${_output_dir}/${_stl_file_name_we}.py)
        set(_file_output_files ${_file_output_headers})
      else()
        message(FATAL_ERROR "  sen_generate_cpp: invalid output language ${_arg_LANG}")
      endif()

      add_custom_command(
        OUTPUT ${_file_output_files}
        COMMAND_EXPAND_LISTS VERBATIM
        COMMAND sen::cli_gen ${_arg_LANG} ${_public_symbols_opt} stl ${_settings_file_option} ${_base_path_opt}
                ${_abs_stl_file} ${_extra_options}
        DEPENDS ${_abs_stl_file} sen::cli_gen ${_abs_settings_path}
        WORKING_DIRECTORY ${_file_output_dir}
        COMMENT "Generating sen code for ${_abs_stl_file} in ${_file_output_dir}"
      )

      list(APPEND _gen_hdr_files ${_file_output_headers})
      list(APPEND _stl_files ${_abs_stl_file})
      target_sources(${_arg_TARGET} PRIVATE ${_abs_stl_file} ${_file_output_files})
      set_property(
        TARGET ${_arg_TARGET}
        APPEND
        PROPERTY EXPORT_FILES ${_abs_stl_file}
      )
    endforeach()

    # generate schema file if needed
    if(_generate_schema)
      add_custom_command(
        OUTPUT ${_arg_SCHEMA_FILE}
        COMMAND_EXPAND_LISTS VERBATIM
        COMMAND sen::cli_gen json ${_schema_type} stl -b ${_abs_base_path} ${_stl_files} ${_extra_options} -o
                ${_arg_SCHEMA_FILE} ${_component_name_opt}
        DEPENDS ${_stl_files} sen::cli_gen ${_abs_settings_path}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Generating json schema code for ${_arg_TARGET} in ${_arg_SCHEMA_FILE}"
      )

      # add schema file to the generated files
      target_sources(${_arg_TARGET} PRIVATE ${_arg_SCHEMA_FILE})
    endif()

    # indicate that the target exports sen types
    set_property(TARGET ${_arg_TARGET} PROPERTY SEN_EXPORTS_TYPES YES)

    if(${_arg_LANG} STREQUAL py)
      set_property(TARGET ${_arg_TARGET} PROPERTY SEN_IS_PYTHON YES)
    endif()

  endif(_arg_STL_FILES)

  # generate from HLA FOM XMLs
  if(_arg_HLA_FOM_DIRS)

    set(_abs_fom_dirs)
    set(_fom_generated_files)

    # output dir for the generated files
    if(_arg_HLA_OUTPUT_DIR)
      set(_output_dir ${_output_dir}/${_arg_HLA_OUTPUT_DIR})
      file(MAKE_DIRECTORY ${_output_dir})
      target_include_directories(${_arg_TARGET} PUBLIC "$<BUILD_INTERFACE:${_output_dir}>")
    endif()

    foreach(_fom_dir ${_arg_HLA_FOM_DIRS})

      get_filename_component(_abs_fom_dir ${_fom_dir} ABSOLUTE)
      list(APPEND _abs_fom_dirs ${_abs_fom_dir})
      set_property(
        TARGET ${_arg_TARGET}
        APPEND
        PROPERTY HLA_FOM_DIRS ${_abs_fom_dir}
      )

      # get the xml files
      file(
        GLOB _xml_files
        LIST_DIRECTORIES false
        "${_fom_dir}/*.xml"
      )

      foreach(_xml_file ${_xml_files})

        # compute _xml_stem
        get_filename_component(_xml_file_full_name_not_lower ${_xml_file} NAME)

        # make it lower case
        string(TOLOWER ${_xml_file_full_name_not_lower} _xml_file_full_name)

        # replace forbidden characters with _ to comply with python naming standards
        if(${_arg_LANG} STREQUAL py)
          string(
            REPLACE -
                    _
                    _xml_file_full_name
                    ${_xml_file_full_name}
          )
          string(
            REPLACE "\."
                    _
                    _xml_file_full_name
                    ${_xml_file_full_name}
          )
        endif()

        # compute output prefix
        get_filename_component(_fom_final_dir ${_fom_dir} NAME)
        set(_output_file_prefix "${_output_dir}/${_fom_final_dir}/${_xml_file_full_name}")

        # compute the files for this .xml
        if(${_arg_LANG} STREQUAL cpp)
          set(_file_output_headers ${_output_file_prefix}.h ${_output_file_prefix}.traits.h)
          set(_file_output_files ${_file_output_headers} ${_output_file_prefix}.cpp)
        elseif(${_arg_LANG} STREQUAL py)
          # replace the XML extension of the FOM file with the .py extension
          string(
            REPLACE _xml
                    .py
                    _output_file_swapped_extension
                    ${_output_file_prefix}
          )
          set(_file_output_headers ${_output_file_swapped_extension})
          set(_file_output_files ${_output_file_swapped_extension})
        else()
          message(FATAL_ERROR "  sen_generate_cpp: invalid output language ${_arg_LANG}")
        endif()

        list(APPEND _gen_hdr_files ${_file_output_headers})
        target_sources(${_arg_TARGET} PRIVATE ${_file_output_files})
        list(APPEND _fom_generated_files ${_file_output_files})
        set_property(
          TARGET ${_arg_TARGET}
          APPEND
          PROPERTY EXPORT_FILES ${_xml_file_full_name}
        )
      endforeach()
    endforeach()

    # add HLA root files to the output files list if building cpp
    if(${_arg_LANG} STREQUAL cpp)
      set(_hla_root_files ${_output_dir}/hla.stl.cpp ${_output_dir}/hla.stl.h ${_output_dir}/hla.stl.traits.h)
      target_sources(${_arg_TARGET} PRIVATE ${_hla_root_files})
      list(APPEND _fom_generated_files ${_hla_root_files})
    endif()

    set(_mapping_opt)
    if(_arg_HLA_MAPPINGS_FILE)
      set(_abs_mappings)

      foreach(_mappings_file ${_arg_HLA_MAPPINGS_FILE})
        get_filename_component(_abs_mapping_file ${_mappings_file} ABSOLUTE)
        list(APPEND _xml_files ${_abs_mapping_file})
        list(APPEND _abs_mappings ${_abs_mapping_file})
      endforeach()

      list(
        JOIN
        _abs_mappings
        ","
        _all_mappings
      )
      set(_mapping_opt "--mappings=${_all_mappings}")
    endif()

    target_sources(${_arg_TARGET} PRIVATE ${_xml_files})

    add_custom_command(
      OUTPUT ${_fom_generated_files}
      COMMAND_EXPAND_LISTS VERBATIM
      COMMAND sen::cli_gen ${_arg_LANG} ${_public_symbols_opt} fom ${_settings_file_option} ${_mapping_opt}
              --directories=${_abs_fom_dirs}
      DEPENDS sen::cli_gen ${_xml_files} ${_abs_settings_path}
      WORKING_DIRECTORY ${_output_dir}
      COMMENT "Generating sen code for ${_arg_HLA_FOM_DIRS} in ${_output_dir}"
    )

    if(_generate_schema)
      add_custom_command(
        OUTPUT ${_arg_SCHEMA_FILE}
        COMMAND_EXPAND_LISTS VERBATIM
        COMMAND sen::cli_gen json ${_schema_type} fom ${_mapping_opt} --directories=${_abs_fom_dirs}
                ${_extra_options} -o ${_arg_SCHEMA_FILE} ${_component_name_opt}
        DEPENDS sen::cli_gen ${_xml_files}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMENT "Generating json schema code for ${_arg_HLA_FOM_DIRS} in ${_arg_SCHEMA_FILE}"
      )

      # add schema file to the generated files
      target_sources(${_arg_TARGET} PRIVATE ${_arg_SCHEMA_FILE})
    endif()

    # indicate that the target exports sen types
    set_property(TARGET ${_arg_TARGET} PROPERTY SEN_EXPORTS_TYPES YES)

    if(${_arg_LANG} STREQUAL py)
      set_property(TARGET ${_arg_TARGET} PROPERTY SEN_IS_PYTHON YES)
      set_property(TARGET ${_arg_TARGET} PROPERTY SEN_GEN_DIR ${_output_dir})
    endif()

  endif()

  # output the list of generated header files
  if(_arg_GEN_HDR_FILES)
    set(${_arg_GEN_HDR_FILES}
        ${_gen_hdr_files}
        PARENT_SCOPE
    )
  endif()

endfunction()

# Convenience wrapper for sen_generate_code() that generates C++ code (the default).
# Accepts the same arguments as sen_generate_code() except LANG, which is fixed to cpp.
macro(sen_generate_cpp)
  sen_generate_code(${ARGV})
endmacro()

# Convenience wrapper for sen_generate_code() that generates Python bindings.
# Accepts the same arguments as sen_generate_code() except LANG, which is fixed to py.
macro(sen_generate_python)
  sen_generate_code(${ARGV} LANG py)
endmacro()

# Generates a PlantUML diagram from STL or HLA FOM files.
# Creates a custom target that runs the diagram generator on demand.
#
# sen_generate_uml(
#   TARGET <name>
#     Name of the custom CMake target that triggers diagram generation.
#
#   OUT <file>
#     Path to the output .puml file to be written.
#
#   [BASE_PATH <path>]
#     Root directory for import resolution. Defaults to CMAKE_CURRENT_SOURCE_DIR.
#
#   [STL_FILES <files...>]
#     Sen Type Language (.stl) files to diagram. Mutually exclusive with HLA_FOM_DIRS.
#
#   [HLA_FOM_DIRS <dirs...>]
#     Directories containing HLA FOM XML files to diagram.
#     Mutually exclusive with STL_FILES.
#
#   [HLA_MAPPINGS_FILE <files...>]
#     HLA mapping files forwarded to the diagram generator. Requires HLA_FOM_DIRS.
#
#   [CLASSES_ONLY]
#     Include only class and interface definitions in the diagram.
#     Omits primitive types, structs, and enumerations.
#
#   [TYPES_ONLY]
#     Include only type definitions (structs, enums, aliases).
#     Omits class and interface diagrams.
#
#   [TYPES_ONLY_NO_ENUMS]
#     Like TYPES_ONLY but also omits the individual enumerator values,
#     showing only the enum names.
function(sen_generate_uml)

  set(_options CLASSES_ONLY TYPES_ONLY TYPES_ONLY_NO_ENUMS)
  set(_one_value_args TARGET BASE_PATH OUT)
  set(_multi_value_args STL_FILES HLA_FOM_DIRS HLA_MAPPINGS_FILE)

  cmake_parse_arguments(
    _arg
    "${_options}"
    "${_one_value_args}"
    "${_multi_value_args}"
    ${ARGN}
  )

  if(NOT _arg_TARGET)
    message(FATAL_ERROR "sen_generate_uml: no TARGET set")
  endif()

  if(NOT _arg_OUT)
    message(FATAL_ERROR "sen_generate_uml: no OUT set")
  endif()

  if(_arg_STL_FILES AND _arg_HLA_FOM_DIRS)
    message(FATAL_ERROR "sen_generate_uml: STL_FILES and HLA_FOM_DIRS cannot be present at the same time")
  endif()

  if(_arg_HLA_MAPPINGS_FILE AND NOT _arg_HLA_FOM_DIRS)
    message(FATAL_ERROR "sen_generate_uml: HLA_MAPPINGS_FILE is defined, but no HLA_FOM_DIRS were specified")
  endif()

  set(_flags)
  if(_arg_CLASSES_ONLY)
    set(_flags --only-classes)
  elseif(_arg_TYPES_ONLY)
    set(_flags --only-types)
  elseif(_arg_TYPES_ONLY_NO_ENUMS)
    set(_flags --only-types --no-enumerators)
  endif()

  # get the absolute base path
  if(_arg_BASE_PATH)
    get_filename_component(_abs_base_path ${_arg_BASE_PATH} ABSOLUTE)
  else()
    set(_abs_base_path ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  if(_arg_STL_FILES)
    set(_stl_files ${_arg_STL_FILES})

    foreach(_stl_file ${_stl_files})
      get_filename_component(_abs_stl_file ${_stl_file} ABSOLUTE)
      list(APPEND _input_files_list ${_abs_stl_file})
    endforeach()

    add_custom_target(
      ${_arg_TARGET}
      COMMAND sen::cli_gen uml stl ${_input_files_list} -i ${_abs_base_path} --output ${_arg_OUT} ${_flags}
      DEPENDS sen::cli_gen ${_input_files_list}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Generating plantuml diagram for ${_input_files_list} in ${_arg_OUT}"
      VERBATIM COMMAND_EXPAND_LISTS
    )
  endif()

  if(_arg_HLA_FOM_DIRS)
    set(_input_xmls)
    set(_abs_fom_dirs)

    # compute _input_xmls and _abs_fom_dirs
    foreach(_fom_dir ${_arg_HLA_FOM_DIRS})
      get_filename_component(_abs_fom_dir ${_fom_dir} ABSOLUTE)
      list(APPEND _abs_fom_dirs ${_abs_fom_dir})

      file(
        GLOB _xml_files
        LIST_DIRECTORIES false
        "${_fom_dir}/*.xml"
      )
      list(APPEND _input_xmls ${_xml_file})
    endforeach()

    set(_mapping_opt)
    if(_arg_HLA_MAPPINGS_FILE)
      get_filename_component(_abs_mapping_file ${_arg_HLA_MAPPINGS_FILE} ABSOLUTE)
      list(APPEND _input_xmls ${_abs_mapping_file})
      set(_mapping_opt "--mappings=${_abs_mapping_file}")
    endif()

    add_custom_target(
      ${_arg_TARGET}
      COMMAND sen::cli_gen uml fom ${_mapping_opt} --directories=${_abs_fom_dirs} --output ${_arg_OUT}
              ${_flags}
      DEPENDS sen::cli_gen ${_input_xmls}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Generating plantuml diagram for ${_arg_HLA_FOM_DIRS} in ${_arg_OUT}"
      VERBATIM COMMAND_EXPAND_LISTS
    )

  endif()
endfunction()

# Invokes a Python script at build time to generate a YAML configuration file.
# Creates a custom target that re-runs the script whenever its inputs change.
# If mypy is found, also creates a <TARGET>_check target that type-checks the script.
#
# sen_generate_yaml(
#   TARGET <name>
#     Name of the custom CMake target that triggers YAML generation.
#
#   SCRIPT <file>
#     Path to the Python script that produces the YAML. The script is called with
#     OUTPUT as its only argument: python script.py <output_file>.
#
#   OUTPUT <file>
#     Path where the generated YAML file will be written.
#
#   [DEPS <targets...>]
#     Targets this script depends on. Python-language targets (created with
#     sen_generate_python) automatically have their generated directory prepended
#     to PYTHONPATH so the script can import the generated types.
#)
function(sen_generate_yaml)

  set(_options)
  set(_one_value_args TARGET SCRIPT OUTPUT)
  set(_multi_value_args)
  set(_multi_value_args DEPS)

  find_program(_mypy_exec mypy PATHS $ENV{HOME}/.local/bin)

  cmake_parse_arguments(
    _arg
    "${_options}"
    "${_one_value_args}"
    "${_multi_value_args}"
    ${ARGN}
  )

  if(NOT _arg_TARGET)
    message(FATAL_ERROR "  sen_generate_yaml: no TARGET set")
  endif()

  if(NOT _arg_SCRIPT)
    message(FATAL_ERROR "  sen_generate_yaml: no SCRIPT set")
  endif()

  if(NOT _arg_OUTPUT)
    message(FATAL_ERROR "  sen_generate_yaml: no OUTPUT set")
  endif()

  foreach(_dep ${_arg_DEPS})
    get_property(
      _is_python
      TARGET ${_dep}
      PROPERTY SEN_IS_PYTHON
    )
    if(_is_python)
      get_property(
        _gen_dir
        TARGET ${_dep}
        PROPERTY SEN_GEN_DIR
      )
      if(NOT _python_path)
        set(_python_path ${_gen_dir})
      else()
        set(_python_path ${_python_path}:${_gen_dir})
      endif()
    endif()
  endforeach()

  if(NOT _python_path)
    set(_path_cmd)
  else()
    set(_path_cmd
        ${CMAKE_COMMAND}
        -E
        env
        PYTHONPATH=${_python_path}
    )
  endif()

  if(_mypy_exec)
    add_custom_target(
      ${_arg_TARGET}_check
      COMMAND ${_path_cmd} ${_mypy_exec} ${_arg_SCRIPT}
      DEPENDS ${_arg_DEPS} ${_arg_SCRIPT}
      WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
      COMMENT "Checking sen python config file ${_arg_SCRIPT}"
      VERBATIM COMMAND_EXPAND_LISTS
    )
  endif()

  add_custom_target(
    ${_arg_TARGET}
    COMMAND ${_path_cmd} ${Python3_EXECUTABLE} ${_arg_SCRIPT} ${_arg_OUTPUT}
    DEPENDS ${_arg_DEPS} ${_arg_SCRIPT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMENT "Generating sen config file"
    VERBATIM COMMAND_EXPAND_LISTS
  )

  if(_mypy_exec)
    add_dependencies(${_arg_TARGET} ${_arg_TARGET}_check)
  endif()

endfunction()
