# === sen_misc_utils.cmake =============================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include_guard()

function(sen_enable_static_analysis target_name)
  if(NOT SEN_DISABLE_CLANG_TIDY)
    if(MSVC)
      set_property(TARGET ${target_name} PROPERTY VS_GLOBAL_EnableClangTidyCodeAnalysis true)
    else()
      if(NOT clang_tidy_path)
        message(WARNING "clang-tidy disabled for target ${target_name}")
      else()
        set_property(TARGET ${target_name} PROPERTY CXX_CLANG_TIDY "${clang_tidy_path};-extra-arg=-std=c++17")
      endif()
    endif()
  endif()
endfunction()

# private function to collect all arguments needed for exporting the package
function(sen_collect_export_args)

  set(_one_value_args
      TARGET
      EXPORT_NAME
      EXPORT_ARGS
      EXPORT_CLASSES
  )

  set(_multi_value_args USER_EXPORTED_CLASSES SOURCES)

  cmake_parse_arguments(
    _arg
    "${_options}"
    "${_one_value_args}"
    "${_multi_value_args}"
    ${ARGN}
  )

  set(_export_args)
  set(_export_classes)

  # get the list of classes implemented and exported by this package
  if(_arg_USER_EXPORTED_CLASSES)
    foreach(_class ${_arg_USER_EXPORTED_CLASSES})
      list(
        APPEND
        _export_args
        -i
        ${_arg_EXPORT_NAME}.${_class}
      )
      list(APPEND _export_classes ${_arg_EXPORT_NAME}.${_class})
    endforeach()
  else()
    foreach(_source ${_arg_SOURCES})
      file(STRINGS ${_source} _file_strings)

      foreach(_line ${_file_strings})
        string(
          REGEX MATCH
                "^[ \t]*SEN_EXPORT_CLASS\((.*)\)"
                _name
                ${_line}
        )

        if(_name)
          string(FIND ${_name} "(" _open_paren)
          math(EXPR _start "${_open_paren} + 1")
          string(
            SUBSTRING ${_name}
                      ${_start}
                      -1
                      _name_trailing_paren
          )
          string(LENGTH ${_name_trailing_paren} _len)
          math(EXPR _end "${_len} - 1")
          string(
            SUBSTRING ${_name_trailing_paren}
                      0
                      ${_end}
                      _final
          )

          list(
            APPEND
            _export_args
            -i
            ${_arg_EXPORT_NAME}.${_final}
          )
          list(APPEND _export_classes ${_arg_EXPORT_NAME}.${_final})
        endif()
      endforeach()
    endforeach()
  endif()

  # input files to export
  get_target_property(_input_files ${_arg_TARGET} EXPORT_FILES)
  if(NOT
     _input_files
     STREQUAL
     "_input_files-NOTFOUND"
  )
    foreach(_file ${_input_files})
      list(
        APPEND
        _export_args
        -f
        ${_file}
      )
    endforeach()
  endif()

  # dependencies to export
  get_target_property(_deps ${_arg_TARGET} EXPORT_DEPS)
  if(NOT
     _deps
     STREQUAL
     "_deps-NOTFOUND"
  )
    foreach(_dep ${_deps})
      list(
        APPEND
        _export_args
        -d
        ${_dep}
      )
    endforeach()
  endif()

  # package to export
  list(
    APPEND
    _export_args
    -p
    ${_arg_EXPORT_NAME}
  )

  if(_arg_EXPORT_ARGS)
    set(${_arg_EXPORT_ARGS}
        ${_export_args}
        PARENT_SCOPE
    )
  endif()

  if(_arg_EXPORT_CLASSES)
    set(${_arg_EXPORT_CLASSES}
        ${_export_classes}
        PARENT_SCOPE
    )
  endif()

endfunction()

function(
  copy_target_property
  to_target
  from_target
  property_name
)
  get_property(
    _val
    TARGET ${from_target}
    PROPERTY ${property_name}
  )

  if(NOT
     "${_val}"
     STREQUAL
     "_val-NOTFOUND"
  )
    set_property(
      TARGET ${to_target}
      APPEND
      PROPERTY ${property_name} ${_val}
    )
  endif()
endfunction()

# Links a Sen dependency to a target, exporting Sen types if needed
function(
  sen_add_dependency
  target_name
  scope
  dep_target_name
)
  if(NOT TARGET ${target_name} OR NOT TARGET ${dep_target_name})
    message(FATAL_ERROR "add_dependency: both ${target_name} and ${dep_target_name} need to be valid targets!")
  endif()

  copy_target_property(${target_name} ${dep_target_name} SEN_IMPORT_DIRS)

  if(scope STREQUAL "PUBLIC")
    # get exported types from dependencies
    get_target_property(_dep_exports_types ${dep_target_name} SEN_EXPORTS_TYPES)
    if(_dep_exports_types)
      set_property(TARGET ${target_name} PROPERTY SEN_EXPORTS_TYPES YES)

      # only add to EXPORT_DEPS if the dependency is a SHARED or INTERFACE library
      get_target_property(_dep_type ${dep_target_name} TYPE)
      if(_dep_type STREQUAL "SHARED_LIBRARY" OR _dep_type STREQUAL "INTERFACE_LIBRARY")
        set_property(
          TARGET ${target_name}
          APPEND
          PROPERTY EXPORT_DEPS ${dep_target_name}
        )
      endif()
    endif()
  endif()

  target_link_libraries(${target_name} ${scope} ${dep_target_name})
endfunction()

function(sen_add_dependencies TARGET SCOPE)

  # if no dependencies are specified, return early
  if(NOT ARGN)
    return()
  endif()

  foreach(_item ${ARGN})
    sen_add_dependency(${TARGET} ${SCOPE} ${_item})
  endforeach()
endfunction()

# Generates a source file containing the data of another file as a character array
# sen_file_to_cpp(IN input_file OUT output_file VARNAME var_name)
function(sen_file_to_cpp)

  set(_options)
  set(_one_value_args IN OUT VARNAME)
  set(_multi_value_args)

  cmake_parse_arguments(
    sen_file_to_cpp
    "${_options}"
    "${_one_value_args}"
    "${_multi_value_args}"
    ${ARGN}
  )

  if(NOT sen_file_to_cpp_IN)
    message(FATAL_ERROR "sen_file_to_cpp: no IN file set")
  endif()

  if(NOT sen_file_to_cpp_OUT)
    message(FATAL_ERROR "sen_file_to_cpp: no OUT file set")
  endif()

  if(NOT sen_file_to_cpp_VARNAME)
    message(FATAL_ERROR "sen_file_to_cpp: no VARNAME set")
  endif()

  get_filename_component(_abs_in_file ${sen_file_to_cpp_IN} ABSOLUTE)
  get_filename_component(_abs_out_file ${sen_file_to_cpp_OUT} ABSOLUTE)

  add_custom_command(
    OUTPUT ${sen_file_to_cpp_OUT}
    COMMAND_EXPAND_LISTS VERBATIM
    COMMAND sen::cli_sen file-to-array -i ${_abs_in_file} -o ${_abs_out_file} -v ${sen_file_to_cpp_VARNAME}
    DEPENDS ${_abs_in_file} sen::cli_sen
    COMMENT "generating ${_abs_in_file} into ${_abs_out_file}"
  )

endfunction()

# Given a target of a project and the route path of where this project is installed externally,
# create a target_name_interfaces target where its properties are filled with information of where
# its files are located in the external installation.
#
# get_external_interfaces(
#  TARGET <targetName>
#  INSTALLATION_DIR <directory>
#)
function(get_external_interfaces)

  set(_options)
  set(_one_value_args INSTALLATION_DIR TARGET)

  cmake_parse_arguments(
    _arg
    "${_options}"
    "${_one_value_args}"
    "${_multi_value_args}"
    ${ARGN}
  )

  if(NOT _arg_TARGET)
    message(FATAL_ERROR "get_external_interfaces: no TARGET set")
  elseif(NOT TARGET ${_arg_TARGET})
    message(FATAL_ERROR "get_external_interfaces: TARGET ${_arg_TARGET} is not a valid target. \
        Please check the interfaces consumption guide on Sen's official documentation for further information."
    )
  endif()

  if(NOT _arg_INSTALLATION_DIR)
    message(FATAL_ERROR "get_external_interfaces: no INSTALLATION_DIR set")

  elseif(NOT EXISTS ${_arg_INSTALLATION_DIR})
    message(
      FATAL_ERROR
        "get_external_interfaces: INSTALLATION_DIR ${_arg_INSTALLATION_DIR} could not be found on the system. \
        Please check the interfaces consumption guide on Sen's official documentation for further information."
    )

  endif()

  # obtain properties at the time of generation.
  get_filename_component(_abs_install_dir ${_arg_INSTALLATION_DIR} ABSOLUTE)

  get_target_property(_build_stl_files ${_arg_TARGET} STL_FILES)
  get_target_property(_build_hla_fom_dirs ${_arg_TARGET} HLA_FOM_DIRS)
  get_target_property(_build_base_path ${_arg_TARGET} BASE_PATH)
  string(
    REGEX MATCH
          NOTFOUND
          _stls_present
          ${_build_stl_files}
  )
  string(
    REGEX MATCH
          NOTFOUND
          _hla_fom_dirs_present
          ${_build_hla_fom_dirs}
  )

  # set base_path that should be common to all interfaces
  set_property(TARGET ${_arg_TARGET} PROPERTY INSTALL_BASE_PATH ${_abs_install_dir}/interfaces/)

  # set stl files if found
  if(_build_stl_files)
    foreach(stl_file ${_build_stl_files})
      string(
        REPLACE ${_build_base_path}
                ""
                relative_stl_path
                ${stl_file}
      )
      set(installation_stl "${_abs_install_dir}/interfaces/${relative_stl_path}")

      if(NOT EXISTS ${installation_stl})
        message(
          FATAL_ERROR
            "get_external_interfaces: Could not obtain external interfaces for TARGET ${_arg_TARGET}: Path ${installation_stl} could not be found on system.\
            Please, contact with the maintainers of the target. \n\
            Possible reasons of the failure: \n\
            \tWrongly inputted BASE_PATH on original package generation.\n\
            \tNo INSTALL directive on the STL files.\n\
            \tINSTALL directory not compliant with the BASE_PATH. \n\

            Report this information to the developer when suggesting a fix: \n\
            \tFile was originally built in ${stl_file}\n\
            \tFile was expected to be in ${_abs_install_dir}/interfaces/${relative_stl_path} \

            Please check the interfaces consumption guide on Sen's official documentation for further information.
                "
        )
      else()
        set_property(
          TARGET ${_arg_TARGET}
          APPEND
          PROPERTY INSTALL_STL_FILES ${installation_stl}
        )
      endif()
    endforeach()
  endif()

  # set hla fom dirs if found
  if(_build_hla_fom_dirs)
    foreach(fom_dir ${_build_hla_fom_dirs})
      string(
        REPLACE ${_build_base_path}
                ""
                relative_fom_dir_path
                ${fom_dir}
      )
      set(installation_fom_dir "${_abs_install_dir}/${relative_fom_dir_path}")

      if(NOT EXISTS ${installation_fom_dir})
        message(
          FATAL_ERROR
            "get_external_interfaces: Could not obtain external interfaces for TARGET ${_arg_TARGET}: Directory ${installation_stl} could not be found on system.\
            Please, contact with the maintainers of the target. \n\
            Possible reasons of the failure: \n\
            \tWrongly inputted BASE_PATH on original package generation.\n\
            \tNo INSTALL directive on the FOM directories.\n\
            \tINSTALL directory not compliant with the BASE_PATH. \n\

            Report this information to the developer when suggesting a fix: \n\
            \tFile was originally built in ${stl_file}\n\
            \tFile was expected to be in ${_abs_install_dir}/${relative_stl_path}\

            Please check the interfaces consumption guide on Sen's official documentation for further information.
                "
        )
      else()
        set_property(
          TARGET ${_arg_TARGET}
          APPEND
          PROPERTY INSTALL_HLA_FOM_DIRS ${installation_fom_dir}
        )
      endif()
    endforeach()
  endif()
endfunction()

# Parse all the -config.cmake.in files in the current directory and install the generated -config.cmake files into the target
# installation directory. The INTERFACES_CONFIG_DIR argument can specify a list of directories other than the current list directory where more -config.cmake.in files can be found.
# configure_exportable_packages(
#   INTERFACES_CONFIG_DIRS [directory1, directory2, ...]
# )
function(configure_exportable_packages)
  # Create a ConfigVersion.cmake file
  include(CMakePackageConfigHelpers)

  # If enabled, the packages/libs cmake config files are installed in separated directories with the target names
  set(_options TARGET_NAME_CMAKEDIRS)

  set(_multi_value_args INTERFACES_CONFIG_DIRS)

  cmake_parse_arguments(
    _arg
    "${_options}"
    "${_one_value_args}"
    "${_multi_value_args}"
    ${ARGN}
  )

  file(GLOB cmake_config_files ${CMAKE_CURRENT_LIST_DIR}/*-config.cmake.in)

  # iterate argument directories and add their cmake configs if the argument is present
  if(_arg_INTERFACES_CONFIG_DIRS)
    foreach(directory ${_arg_INTERFACES_CONFIG_DIRS})
      file(GLOB additional_dir_files ${directory}/*-config.cmake.in)
      list(APPEND cmake_config_files ${additional_dir_files})
    endforeach()
  endif()

  foreach(file ${cmake_config_files})
    # remove the .in extension of the file to generate the output name
    string(
      REGEX
      REPLACE "\\.[^.]*$"
              ""
              output_file
              "${file}"
    )
    get_filename_component(output_file_name ${output_file} NAME)

    if(_arg_TARGET_NAME_CMAKEDIRS)
      set(auxName ${output_file_name})
      get_filename_component(auxName ${auxName} NAME_WE)
      string(
        REPLACE "-config"
                ""
                auxName
                "${auxName}"
      )
      set(CMAKE_INSTALL_CMAKEDIR "cmake/${auxName}")
    endif()

    configure_package_config_file(
      ${file} ${CMAKE_CURRENT_BINARY_DIR}/${output_file_name}
      INSTALL_DESTINATION ${CMAKE_INSTALL_CMAKEDIR}
      NO_SET_AND_CHECK_MACRO NO_CHECK_REQUIRED_COMPONENTS_MACRO
    )

    # Install the config file to the target dir
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${output_file_name} DESTINATION ${CMAKE_INSTALL_CMAKEDIR})
  endforeach()

endfunction()

# Transfers properties from the code generation target to the final package target
function(copy_target_properties to_target from_target)
  copy_target_property(${to_target} ${from_target} BASE_PATH)
  copy_target_property(${to_target} ${from_target} SEN_IMPORT_DIRS)
  copy_target_property(${to_target} ${from_target} STL_FILES)
  copy_target_property(${to_target} ${from_target} HLA_FOM_DIRS)
  copy_target_property(${to_target} ${from_target} EXPORT_FILES)
  copy_target_property(${to_target} ${from_target} SEN_EXPORTS_TYPES)
  copy_target_property(${to_target} ${from_target} SEN_IS_PYTHON)
  copy_target_property(${to_target} ${from_target} SEN_GEN_DIR)
  copy_target_property(${to_target} ${from_target} SCHEMA)
  target_include_directories(
    ${to_target} PUBLIC $<TARGET_PROPERTY:${from_target},INTERFACE_INCLUDE_DIRECTORIES>
  )
endfunction()

# Combines a set of packages or component schemas into one
#
# sen_combine_schemas(
#  OUTPUT file
#  SCHEMAS schemas...
#)
function(sen_combine_schemas)

  set(_one_value_args OUTPUT)

  set(_multi_value_args SCHEMAS)

  cmake_parse_arguments(
    _arg
    "${_options}"
    "${_one_value_args}"
    "${_multi_value_args}"
    ${ARGN}
  )

  if(NOT _arg_OUTPUT)
    message(FATAL_ERROR "  sen_combine_schemas: no OUTPUT provided")
  endif()

  if(NOT _arg_SCHEMAS)
    message(FATAL_ERROR "  sen_combine_schemas: no SCHEMAS provided")
  endif()

  add_custom_command(
    OUTPUT ${_arg_OUTPUT}
    COMMAND_EXPAND_LISTS VERBATIM
    COMMAND sen::cli_gen json schema ${_arg_SCHEMAS} -o ${_arg_OUTPUT}
    DEPENDS ${_arg_SCHEMAS} sen::cli_gen
  )

endfunction()

# Configures a given target with the compiler and linker flags that are required by all sen assets
function(sen_configure_target target_name)

  if(NOT TARGET ${target_name})
    message(FATAL_ERROR "sen_configure_target: target_name needs to be a valid target")
  endif()

  target_compile_definitions(${target_name} PRIVATE "$<$<CONFIG:Debug>:DEBUG>" "$<$<CONFIG:Release>:NDEBUG>")

  # propagate C++17 requirement to consumers and enforce it on the target itself
  target_compile_features(${target_name} PUBLIC cxx_std_17)

  set_target_properties(
    ${target_name}
    PROPERTIES POSITION_INDEPENDENT_CODE ON
               INTERFACE_POSITION_INDEPENDENT_CODE ON
               CXX_STANDARD_REQUIRED ON
               CXX_EXTENSIONS OFF
  )

  if(MSVC)
    target_compile_options(
      ${target_name}
      PRIVATE
        /MP
        /W3 # baseline reasonable warnings
        /w14263 # function: member function does not override any base class virtual member function
        /w14265 # classname: class has virtual functions, but destructor is not virtual
        /w14287 # operator: unsigned/negative constant mismatch
        /we4289 # nonstandard extension used: loop control variable declared in the for-loop is used outside
        /w14296 # operator: expression is always 'boolean_value'
        /w14311 # variable: pointer truncation from 'type1' to 'type2'
        /w14545 # expression before comma evaluates to a function which is missing an argument list
        /w14546 # function call before comma missing argument list
        /w14547 # operator: operator before comma has no effect; expected operator with side-effect
        /w14549 # operator: operator before comma has no effect; did you intend 'operator'?
        /w14555 # expression has no effect; expected expression with side- effect
        /w14640 # Enable warning on thread un-safe static member initialization
        /w14826 # Conversion from 'type1' to 'type_2' is sign-extended. This may cause unexpected runtime behavior.
        /w14905 # wide string literal cast to 'LPSTR'
        /w14906 # string literal cast to 'LPWSTR'
        /w14928 # illegal copy-initialization; more than one user-defined conversion has been implicitly applied
        /wd4702 # unreachable code disabled due too many false positives
        /wd4244 # too many false positives
        /wd4996 # VS ignores definitions of _CRT_SECURE_NO_WARNINGS and _CRT_SECURE_NO_DEPRECATE
        /wd4250 # ignore dominance warnings for virtual inheritance
        /wd4251 # ignore warnings for not exporting explicitly all members of a class
        /external:W0
        /external:anglebrackets
        /permissive- # standards conformance mode for MSVC compiler.
        /EHsc
        /nologo
        /bigobj
        "$<$<CONFIG:Debug>:/MDd;/Od;/RTC1>"
        "$<$<CONFIG:Release>:/O2;/Ox;/Ob2;/MD;/GR;/c>"
    )

    target_compile_definitions(${target_name} PRIVATE _WIN32_WINNT=0x0A00)

    # disable manifest generation
    target_link_options(${target_name} PRIVATE /MANIFEST:NO)
  else()

    set(common_clang_gcc_options_
        -Wall
        -Werror # warnings are errors
        -Wextra # reasonable and standard
        -Wextra-semi # semicolon after in-class function definition
        -Wcast-align # warn for performance problem casts
        -Wunused # warn on anything being unused
        -Wnon-virtual-dtor # virtual destructor if virtual functions present
        -Wno-shadow # variable declarations shadows one of a parent context
        -Wno-overloaded-virtual # warn if you overload (not override) a virtual function
        -Wpedantic # warn if non-standard C++ is used
        -Wdouble-promotion # warn if float is implicitly promoted to double
        -Wimplicit-fallthrough # warn on statements that fallthrough without an explicit annotation
        -Wmisleading-indentation # warn if indentation implies blocks where blocks do not exist
        -Wno-missing-braces
        -Wno-noexcept-type
        -Wno-empty-body
        -Wno-strict-aliasing
        -Wno-deprecated-copy
        -Wno-variadic-macros
        -fsigned-char
        -fexceptions
        -ftls-model=global-dynamic
        -fPIC
        "$<$<CONFIG:Debug>:-O0;-g>"
        "$<$<CONFIG:Release>:-O3>"
    )

    set(common_linker_options_ -Wno-undef)

    if(SEN_USE_SANITIZER STREQUAL None)
      list(APPEND common_linker_options_ LINKER:--exclude-libs,ALL)
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      target_compile_options(${target_name} PRIVATE ${common_clang_gcc_options_})
      target_link_options(${target_name} PRIVATE ${common_linker_options_})
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${target_name} PRIVATE ${common_clang_gcc_options_})
      target_link_options(${target_name} PRIVATE ${common_linker_options_})
    endif()

    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12.0)
      target_compile_options(
        ${target_name} PRIVATE -Wno-attributes # TODO(SEN-1558): remove after code gen rework
      )
    endif()
  endif()

  set_property(TARGET ${target_name} PROPERTY LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
  )# .exe and .dll
  set_property(TARGET ${target_name} PROPERTY RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
  )# .so and .dylib
  set_property(TARGET ${target_name} PROPERTY ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib"
  )# .a and .lib

  # link coverage flags if coverage was enabled (interface target created in root CMakeLists.txt)
  if(TARGET sen_coverage_flags)
    target_link_libraries(${target_name} PRIVATE sen_coverage_flags)
  endif()
endfunction()
