# === sen_internal_utils.cmake =========================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include_guard()

# ===================================================================================================================
# global configuration
# ===================================================================================================================

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.2.1)
  message(FATAL_ERROR "GCC ${CMAKE_CXX_COMPILER_VERSION} is not supported.")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_LINK_WHAT_YOU_USE OFF "Enable this to get feedback about the linkage")

# Pinned before GNUInstallDirs, which only sets what is not already defined. A plain variable rather
# than a cache entry, so -DCMAKE_INSTALL_LIBDIR is ignored: conan's toolchain, the installer and the
# archive check all spell lib, and lib64 support is the RPM generator's problem, not this line's.
set(CMAKE_INSTALL_LIBDIR lib)

# Before the run path below, which is derived from the directories it defines.
include(GNUInstallDirs)

# Components are opened by bare name, so the loader resolves them through libkernel's run path. That
# has to be relative to the library itself or loading depends on the working directory.
#
# Derived rather than written as ../lib so it cannot drift. The FULL_ forms, so it stays correct if
# the pin above is ever lifted and libdir becomes absolute.
file(
  RELATIVE_PATH
  _sen_bin_to_lib
  "${CMAKE_INSTALL_FULL_BINDIR}"
  "${CMAKE_INSTALL_FULL_LIBDIR}"
)

if(UNIX AND NOT APPLE)
  set(CMAKE_INSTALL_RPATH "$ORIGIN/${_sen_bin_to_lib}:$ORIGIN/")
  set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
elseif(APPLE)
  # The same intent in mach-o terms. Nothing builds this today -- no macOS profile and no macOS
  # runner -- so it is here to keep the two loaders from drifting apart.
  set(CMAKE_INSTALL_RPATH "@loader_path/${_sen_bin_to_lib};@loader_path")
  set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
endif()

# for organizing projects into folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER ".cmake")

# ===================================================================================================================
# functions
# ===================================================================================================================

# Internal function that sets the target version and folder to be a library
function(sen_internal_configure_lib target_name)
  sen_configure_target(${target_name})
  set_property(GLOBAL APPEND PROPERTY SEN_INTERNAL_TARGETS ${target_name})
  set_target_properties(
    ${target_name}
    PROPERTIES OUTPUT_NAME ${target_name}
               VERSION ${sen_VERSION}
               CLEAN_DIRECT_OUTPUT 1
               FOLDER "libs"
  )

endfunction()

# Internal function that sets the target version and folder to be an application
function(sen_internal_configure_app target_name)
  sen_configure_target(${target_name})
  set_property(GLOBAL APPEND PROPERTY SEN_INTERNAL_TARGETS ${target_name})
  set_target_properties(
    ${target_name}
    PROPERTIES OUTPUT_NAME ${target_name}
               VERSION ${sen_VERSION}
               CLEAN_DIRECT_OUTPUT 1
               FOLDER "apps"
  )

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(${target_name} PUBLIC -rdynamic)
  endif()
endfunction()

# Helper to add a bunch of files to a target as private sources
function(sen_internal_add_resources)
  set(_options)
  set(_one_value_args TARGET GROUP)
  set(_multi_value_args RESOURCE_FILES)

  cmake_parse_arguments(
    sen_internal_add_resources
    "${_options}"
    "${_one_value_args}"
    "${_multi_value_args}"
    ${ARGN}
  )

  source_group(${sen_internal_add_resources_GROUP} FILES ${sen_internal_add_resources_RESOURCE_FILES})
  target_sources(${sen_internal_add_resources_TARGET} PRIVATE ${sen_internal_add_resources_RESOURCE_FILES})
endfunction()

function(sen_internal_generate_template_headers)

  set(_options)
  set(_one_value_args OUTDIR GEN_FILE_LIST)
  set(_multi_value_args TEMPLATE_FILES)

  cmake_parse_arguments(
    sen_internal_generate_template_headers
    "${_options}"
    "${_one_value_args}"
    "${_multi_value_args}"
    ${ARGN}
  )

  file(MAKE_DIRECTORY ${sen_internal_generate_template_headers_OUTDIR})

  set(_dumpfiles)
  foreach(_template_file ${sen_internal_generate_template_headers_TEMPLATE_FILES})
    get_filename_component(_template_name ${_template_file} NAME_WE)

    set(_outfile ${sen_internal_generate_template_headers_OUTDIR}/${_template_name}.h)

    sen_file_to_cpp(
      IN
      ${_template_file}
      OUT
      ${_outfile}
      VARNAME
      ${_template_name}
    )

    list(APPEND _dumpfiles ${_outfile})

  endforeach()

  set(${sen_internal_generate_template_headers_GEN_FILE_LIST}
      ${_dumpfiles}
      PARENT_SCOPE
  )

endfunction()

# Guard the rest of a Sen component's CMakeLists. Place at the top, above any
# include(tps/<dep>) - declares the SEN_BUILD_<NAME> cache option (default ON, idempotent),
# and returns from the calling file if the option is OFF so no Conan deps get pulled in.
#
# Must be a macro (not a function) for return() to exit the calling CMakeLists file.
#
# Arguments:
#   NAME        - component name (lower case). Used to derive SEN_BUILD_<NAME>.
#   DESCRIPTION - (optional) human-readable description for the option cache entry.
macro(sen_internal_component_guard)
  set(_one_value_args NAME DESCRIPTION)
  cmake_parse_arguments(
    _guard
    ""
    "${_one_value_args}"
    ""
    ${ARGN}
  )

  if(NOT _guard_NAME)
    message(FATAL_ERROR "sen_internal_component_guard: NAME is required")
  endif()

  string(TOUPPER "${_guard_NAME}" _guard_upper)
  set(_guard_option_name "SEN_BUILD_${_guard_upper}")

  if(NOT _guard_DESCRIPTION)
    set(_guard_DESCRIPTION "Build the ${_guard_NAME} component")
  endif()

  option(${_guard_option_name} "${_guard_DESCRIPTION}" ON)

  if(NOT ${_guard_option_name})
    return()
  endif()
endmacro()

function(sen_internal_install target_name)
  # install the gen object library (cmake requires it)
  if(TARGET ${target_name}_gen)
    get_target_property(_type ${target_name}_gen TYPE)
    if(_type STREQUAL "OBJECT_LIBRARY")
      install(
        TARGETS ${target_name}_gen
        EXPORT sen_targets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      )
    endif()
  endif()

  # install the obj library (cmake requires it)
  # TODO SEN-1354: Investigate why cmake requires to add the obj target to the installation export group
  if(TARGET ${target_name}_obj)
    get_target_property(_type ${target_name}_obj TYPE)
    if(_type STREQUAL "OBJECT_LIBRARY")
      install(
        TARGETS ${target_name}_obj
        EXPORT sen_targets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
      )
    endif()
  endif()

  # Python has not searched PATH for an extension module's dependencies since 3.8, so on Windows the
  # module must sit beside the DLLs it links or a plain import fails.
  set(_library_destination ${CMAKE_INSTALL_LIBDIR})
  if(WIN32)
    get_target_property(_target_type ${target_name} TYPE)
    if(_target_type STREQUAL "MODULE_LIBRARY")
      set(_library_destination ${CMAKE_INSTALL_BINDIR})
    endif()
  endif()

  # RUNTIME_DEPENDENCY_SET collects what these binaries actually link, so a third-party shared
  # object is shipped because it is needed rather than because someone remembered it. install.cmake
  # resolves the set once, after every target has been added to it -- on the platforms where it can
  # resolve at all, which is why enrolling is conditional on the same thing.
  set(_runtime_dependency_set RUNTIME_DEPENDENCY_SET sen_runtime_deps)
  if(WIN32)
    set(_runtime_dependency_set "")
  endif()

  install(
    TARGETS ${target_name}
    EXPORT sen_targets
    ${_runtime_dependency_set}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${_library_destination}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

endfunction()

# Bake a consumer's npm prod-dep licenses into ${CMAKE_BINARY_DIR}/foss_licenses/<target>/.
# install.cmake ships that tree; docs/CMakeLists.txt renders it into the licenses page.
# The consumer must list `license-checker-rseidelsohn` in devDependencies.
#
# ESBUILD_METAFILE (optional): when set, narrows the bake to packages actually present in the
# bundle. Without it, every prod dep on disk is attributed (over-inclusive when peer-dep
# transitives don't ship).
function(sen_internal_bake_npm_licenses)
  set(_one_value_args
      TARGET
      SOURCE_DIR
      NPM_SENTINEL
      NPM_EXEC
      ESBUILD_METAFILE
  )
  cmake_parse_arguments(
    _args
    ""
    "${_one_value_args}"
    ""
    ${ARGN}
  )

  foreach(
    _required IN
    ITEMS TARGET
          SOURCE_DIR
          NPM_SENTINEL
          NPM_EXEC
  )
    if(NOT _args_${_required})
      message(FATAL_ERROR "sen_internal_bake_npm_licenses: ${_required} is required")
    endif()
  endforeach()

  get_filename_component(_node_bin_dir ${_args_NPM_EXEC} DIRECTORY)
  set(_out_dir "${CMAKE_BINARY_DIR}/foss_licenses/${_args_TARGET}")
  set(_script ${PROJECT_SOURCE_DIR}/cmake/util/bake_npm_licenses.mjs)
  # Slug for cmake target / sentinel filenames; raw TARGET remains the on-disk dir + docs header.
  string(
    REGEX
    REPLACE "[^A-Za-z0-9_]"
            "_"
            _target_slug
            "${_args_TARGET}"
  )
  set(_sentinel ${CMAKE_CURRENT_BINARY_DIR}/${_target_slug}_npm_licenses.stamp)

  set(_extra_args)
  set(_extra_deps)
  if(_args_ESBUILD_METAFILE)
    list(APPEND _extra_args --esbuild-metafile=${_args_ESBUILD_METAFILE})
    list(APPEND _extra_deps ${_args_ESBUILD_METAFILE})
  endif()

  add_custom_command(
    OUTPUT ${_sentinel}
    COMMAND ${CMAKE_COMMAND} -E env --modify "PATH=path_list_prepend:${_node_bin_dir}" ${_node_bin_dir}/node
            ${_script} ${_args_SOURCE_DIR} "${_out_dir}" ${_extra_args}
    COMMAND ${CMAKE_COMMAND} -E touch ${_sentinel}
    DEPENDS ${_args_NPM_SENTINEL} ${_script} ${_extra_deps}
    COMMENT "Aggregating npm licenses for ${_args_TARGET} -> ${_out_dir}"
    VERBATIM
  )
  add_custom_target(${_target_slug}_npm_licenses ALL DEPENDS ${_sentinel})
  set_target_properties(${_target_slug}_npm_licenses PROPERTIES FOLDER "licenses")

  # The documentation page is written from this tree and has to wait for it.
  set_property(GLOBAL APPEND PROPERTY SEN_LICENSE_TARGETS ${_target_slug}_npm_licenses)
endfunction()

# cmake graphviz generation
if(SEN_ENABLE_CMAKE_TARGET_GRAPH)
  set(GRAPHVIZ_IGNORE_TARGETS
      "runtime_.*;level.*;transport_.*;.*test.*;annoying.*;dummy.*;publish_types.*;inherit.*;repeated.*;.*monkey.*;"
  )

  configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/util/graphviz_options.cmake.in ${CMAKE_BINARY_DIR}/CMakeGraphVizOptions.cmake
  )

  find_package(Python3 COMPONENTS Interpreter)
  set(_pre_file "pre_sen.dot")
  set(_post_file "sen.dot")
  if(NOT Python3_Interpreter_FOUND)
    set(_pre_file "sen.dot")
    message(WARNING "Python interpreter not found: graph will not be colored")
  endif()

  add_custom_target(
    cmake_target_graph COMMAND ${CMAKE_COMMAND} --graphviz=${_pre_file} -S ${CMAKE_SOURCE_DIR} -B
                               ${CMAKE_BINARY_DIR}
  )
  if(Python3_Interpreter_FOUND)
    add_custom_command(
      TARGET cmake_target_graph
      POST_BUILD
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/cmake/util/color_cmake_target_graph.py ${_pre_file}
              ${_post_file}
    )
  endif()

  find_program(dot_executable dot)
  if(NOT dot_executable)
    message(WARNING "'dot' executable not found. Dependency graph cannot be generated.")
  else()
    add_custom_command(
      TARGET cmake_target_graph
      POST_BUILD
      COMMAND ${dot_executable} -Tsvg ${CMAKE_BINARY_DIR}/${_post_file} -o ${CMAKE_SOURCE_DIR}/sen.svg
      COMMENT "Generating graph: sen.svg"
    )
  endif()
endif()

# configure coverage as an interface target so flags apply only to sen targets,
# not to every target in the directory tree (e.g. third-party add_subdirectory targets).
add_library(sen_coverage_flags INTERFACE)
if(SEN_COVERAGE_ENABLE)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(
      sen_coverage_flags
      INTERFACE -g
                -O0
                -fno-inline
                --coverage
    )
    target_link_options(sen_coverage_flags INTERFACE --coverage)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/coverage_data/coverage-%p-%m.profraw" SEN_COVERAGE_DATA_DIR)
    target_compile_options(
      sen_coverage_flags
      INTERFACE -g
                -O0
                -fno-inline
                -fprofile-instr-generate=${SEN_COVERAGE_DATA_DIR}
                -fcoverage-mapping
    )
    target_link_options(
      sen_coverage_flags
      INTERFACE
      -fprofile-instr-generate=${SEN_COVERAGE_DATA_DIR}
      -fcoverage-mapping
    )
  endif()
endif()

if(MSVC)
  add_compile_options(/bigobj)
  # Without this MSVC reads a source file as the build machine's ANSI code page and encodes
  # narrow literals back into it. Our sources are UTF-8 and carry no BOM, so their bytes survive
  # only because CP1252 happens to map them one to one; a machine with a multi-byte code page
  # would corrupt them with no diagnostic. /utf-8 sets both charsets, so the build no longer
  # depends on the locale it runs in.
  add_compile_options(/utf-8)
endif()

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads)
