# === Findspdlog.cmake =================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(FindPackageHandleStandardArgs)

# spdlog types appear in the kernel's public signatures, so a component has to compile against the
# spdlog the kernel was built with -- component_api.h says so where it declares them. The headers
# installed beside Sen's own are that copy, and their version.h is the record of which one it is.
# Three levels: this module installs beside the config, at <prefix>/lib/cmake/sen. Hardcoded
# depth, kept in step by test_findspdlog.py, which builds its fixtures at the same place.
get_filename_component(_sen_spdlog_vendored_dir "${CMAKE_CURRENT_LIST_DIR}/../../../include" ABSOLUTE)

# Reads MAJOR.MINOR.PATCH from a spdlog header tree, leaving `out_var` empty when none is there.
function(_sen_read_spdlog_version include_dir out_var)
  set(${out_var}
      ""
      PARENT_SCOPE
  )

  if(NOT EXISTS "${include_dir}/spdlog/version.h")
    return()
  endif()

  file(READ "${include_dir}/spdlog/version.h" _text)
  set(_parts "")

  foreach(_field MAJOR MINOR PATCH)
    if(_text MATCHES "#define[ \t]+SPDLOG_VER_${_field}[ \t]+([0-9]+)")
      list(APPEND _parts "${CMAKE_MATCH_1}")
    endif()
  endforeach()

  list(LENGTH _parts _count)

  if(_count EQUAL 3)
    list(
      JOIN
      _parts
      "."
      _version
    )
    set(${out_var}
        "${_version}"
        PARENT_SCOPE
    )
  endif()
endfunction()

# fmt's inline namespace carries its version, so a different fmt behind the same spdlog renames
# every fmt symbol a consumer compiles.
function(_sen_read_fmt_version include_dir out_var)
  set(${out_var}
      ""
      PARENT_SCOPE
  )

  # fmt split base.h out of core.h at 11.0 and FMT_VERSION moved with it, so an older fmt has it in
  # core.h and no base.h.
  set(_header "")
  foreach(_candidate "${include_dir}/fmt/base.h" "${include_dir}/fmt/core.h")
    if(EXISTS "${_candidate}")
      set(_header "${_candidate}")
      break()
    endif()
  endforeach()

  if(NOT _header)
    return()
  endif()

  file(READ "${_header}" _text)
  if(_text MATCHES "#[ \t]*define[ \t]+FMT_VERSION[ \t]+([0-9]+)")
    set(${out_var}
        "${CMAKE_MATCH_1}"
        PARENT_SCOPE
    )
  endif()
endfunction()

_sen_read_spdlog_version("${_sen_spdlog_vendored_dir}" _sen_spdlog_expected_version)
_sen_read_fmt_version("${_sen_spdlog_vendored_dir}" _sen_fmt_expected_version)

find_package(spdlog CONFIG QUIET)

if(spdlog_FOUND OR TARGET spdlog::spdlog)
  # A target already there without spdlog_FOUND means the surrounding project supplied spdlog
  # itself. Saying found matters: otherwise find_dependency(spdlog) reports Sen as the missing one.
  set(spdlog_FOUND TRUE)
  set(_sen_spdlog_found_version "${spdlog_VERSION}")

  if(spdlog_DIR
     AND NOT
         spdlog_DIR
         STREQUAL
         "spdlog_DIR-NOTFOUND"
  )
    set(_sen_spdlog_found_where "the spdlog package at ${spdlog_DIR}")
  else()
    set(_sen_spdlog_found_where "an spdlog::spdlog target this project already defined")
  endif()
else()
  # Reached only when the search above found no config package. A consumer with their own spdlog
  # keeps it; a version that does not match the kernel's is reported below rather than replaced.
  if(EXISTS "${_sen_spdlog_vendored_dir}/spdlog/spdlog.h")
    set(SPDLOG_INCLUDE_DIR "${_sen_spdlog_vendored_dir}")
  else()
    # find_path caches, so it keeps its own name: one variable that is a cache entry down one path
    # and a plain one down the other holds a stale answer on the next configure.
    find_path(SPDLOG_SYSTEM_INCLUDE_DIR NAMES spdlog/spdlog.h)
    set(SPDLOG_INCLUDE_DIR "${SPDLOG_SYSTEM_INCLUDE_DIR}")
  endif()

  _sen_read_spdlog_version("${SPDLOG_INCLUDE_DIR}" spdlog_VERSION)

  # Reached only with a header actually found, so this can report spdlog as missing. VERSION_VAR as
  # well, or a consumer asking find_package(spdlog <version>) is told yes whatever it asked for.
  find_package_handle_standard_args(
    spdlog
    REQUIRED_VARS SPDLOG_INCLUDE_DIR
    VERSION_VAR spdlog_VERSION
  )

  if(spdlog_FOUND AND NOT TARGET spdlog::spdlog)
    add_library(spdlog::spdlog INTERFACE IMPORTED)
    set_target_properties(spdlog::spdlog PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${SPDLOG_INCLUDE_DIR}")

    # The library installed beside these headers. Linking it puts a consumer's loggers in the
    # kernel's registry; without it spdlog compiles header-only and the consumer has its own. The
    # defines follow the library.
    get_filename_component(_sen_spdlog_prefix "${SPDLOG_INCLUDE_DIR}/.." ABSOLUTE)
    find_library(
      SPDLOG_LIBRARY
      NAMES spdlog spdlogd
      HINTS "${_sen_spdlog_prefix}/lib" "${_sen_spdlog_prefix}/lib64"
      NO_DEFAULT_PATH
    )

    if(SPDLOG_LIBRARY)
      set_property(
        TARGET spdlog::spdlog
        APPEND
        PROPERTY INTERFACE_LINK_LIBRARIES "${SPDLOG_LIBRARY}"
      )
      set_property(
        TARGET spdlog::spdlog
        APPEND
        PROPERTY INTERFACE_COMPILE_DEFINITIONS "SPDLOG_COMPILED_LIB"
      )

      # SPDLOG_SHARED_LIB only for a library that is one: against a static archive it asks MSVC for
      # dllimport thunks the archive does not carry. A static spdlog is a registry per linked copy
      # whatever is defined, so warn instead.
      if(SPDLOG_LIBRARY MATCHES "\\.a$")
        message(WARNING "spdlog at ${SPDLOG_LIBRARY} is a static library. Loggers made in this component "
                        "will not be the kernel's: spdlog's registry is one per linked copy."
        )
      else()
        set_property(
          TARGET spdlog::spdlog
          APPEND
          PROPERTY INTERFACE_COMPILE_DEFINITIONS "SPDLOG_SHARED_LIB"
        )
      endif()

      if(MSVC)
        # logger has std::string and std::vector members that no dll-interface covers. spdlog's own
        # CMake sets these PUBLIC when it builds shared.
        set_property(
          TARGET spdlog::spdlog
          APPEND
          PROPERTY INTERFACE_COMPILE_OPTIONS /wd4251 /wd4275
        )
      endif()
    else()
      # Headers without a library are a packaging failure: spdlog compiles header-only, the
      # component gets a registry of its own, and the kernel's logger configuration never reaches it.
      message(WARNING "spdlog headers at ${SPDLOG_INCLUDE_DIR} with no library beside them. Loggers made in "
                      "this component will not be the kernel's: spdlog's registry is one per linked copy and "
                      "this one compiles its own. The install is incomplete."
      )
    endif()

    # fmt sits beside spdlog when spdlog was built against it, which is how Sen builds it. base.h
    # counts too: core.h is only a deprecation stub, and losing it would silently unset the define.
    # FMT_HEADER_ONLY is set with it because the install ships fmt's headers and no library, so a
    # consumer calling an external fmt would have nothing to link.
    if(EXISTS "${SPDLOG_INCLUDE_DIR}/fmt/core.h" OR EXISTS "${SPDLOG_INCLUDE_DIR}/fmt/base.h")
      set_property(
        TARGET spdlog::spdlog
        APPEND
        PROPERTY INTERFACE_COMPILE_DEFINITIONS "SPDLOG_FMT_EXTERNAL" "FMT_HEADER_ONLY"
      )

      # fmt's headers carry a static_assert that fails unless the literal encoding is UTF-8, and
      # MSVC reads a source file as the build machine's ANSI code page without this. Set here
      # because this is what hands a consumer the fmt headers that require it.
      if(MSVC)
        set_property(
          TARGET spdlog::spdlog
          APPEND
          PROPERTY INTERFACE_COMPILE_OPTIONS "/utf-8"
        )
      endif()
    endif()
  endif()

  set(_sen_spdlog_found_version "${spdlog_VERSION}")
  set(_sen_spdlog_found_where "${SPDLOG_INCLUDE_DIR}")
endif()

# spdlog types cross the kernel's API boundary, so a component compiled against a different spdlog
# does not share the kernel's loggers and may not link. Warn rather than refuse: a consumer whose
# package manager picked the version often cannot change it.
if(_sen_spdlog_expected_version AND NOT SEN_SUPPRESS_SPDLOG_VERSION_WARNING)
  if(NOT _sen_spdlog_found_version)
    message(
      WARNING "Sen was built against spdlog ${_sen_spdlog_expected_version}, and the spdlog in use"
              " (${_sen_spdlog_found_where}) does not report a version, so the two cannot be compared."
              " spdlog types appear in the kernel's API, so a mismatch misbehaves at run time rather than"
              " failing to build. Sen's own copy is at ${_sen_spdlog_vendored_dir}."
              " Set SEN_SUPPRESS_SPDLOG_VERSION_WARNING to silence this."
    )
  elseif(
    NOT
    _sen_spdlog_found_version
    VERSION_EQUAL
    _sen_spdlog_expected_version
  )
    message(
      WARNING "spdlog ${_sen_spdlog_found_version} is in use (${_sen_spdlog_found_where}), but Sen was"
              " built against ${_sen_spdlog_expected_version}. spdlog types appear in the kernel's API, so"
              " a component built this way does not share the kernel's loggers, and may fail to link."
              " Sen's own copy is at ${_sen_spdlog_vendored_dir}."
              " Set SEN_SUPPRESS_SPDLOG_VERSION_WARNING to silence this."
    )
  else()
    # The spdlog versions agree, so compare the fmt behind them: the spdlog version cannot see a
    # different fmt. The vendored branch sets SPDLOG_INCLUDE_DIR; a config package does not, so read
    # the first include directory off its target.
    set(_sen_found_include "${SPDLOG_INCLUDE_DIR}")
    if(NOT _sen_found_include AND TARGET spdlog::spdlog)
      get_target_property(_sen_target_includes spdlog::spdlog INTERFACE_INCLUDE_DIRECTORIES)
      if(_sen_target_includes)
        # A config package names the directory inside a per-configuration generator expression,
        # which EXISTS does not evaluate. The same strip as cmake/tps/spdlog.cmake.
        string(
          REGEX
          REPLACE "\\$<\\$<CONFIG:[^>]+>:(.*)>"
                  "\\1"
                  _sen_target_includes
                  "${_sen_target_includes}"
        )
        list(
          GET
          _sen_target_includes
          0
          _sen_found_include
        )
      endif()
    endif()

    if(_sen_fmt_expected_version AND _sen_found_include)
      _sen_read_fmt_version("${_sen_found_include}" _sen_fmt_found_version)
      if(_sen_fmt_found_version
         AND NOT
             _sen_fmt_found_version
             STREQUAL
             _sen_fmt_expected_version
      )
        message(
          WARNING "fmt ${_sen_fmt_found_version} sits beside the spdlog in use"
                  " (${_sen_spdlog_found_where}), but Sen was built against fmt"
                  " ${_sen_fmt_expected_version}. fmt's inline namespace carries its version, so the"
                  " symbols a component compiles are not the ones the kernel exports."
                  " Sen's own copy is at ${_sen_spdlog_vendored_dir}."
                  " Set SEN_SUPPRESS_SPDLOG_VERSION_WARNING to silence this."
        )
      endif()
    endif()
  endif()
endif()
