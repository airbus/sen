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
get_filename_component(_sen_spdlog_vendored_dir "${CMAKE_CURRENT_LIST_DIR}/../../include" ABSOLUTE)

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

_sen_read_spdlog_version("${_sen_spdlog_vendored_dir}" _sen_spdlog_expected_version)

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
  # The vendored copy before any system one. A system spdlog is the least likely to match the kernel,
  # and the mismatch it produces is not a build error but a component that misbehaves once running.
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
    endif()
  endif()

  set(_sen_spdlog_found_version "${spdlog_VERSION}")
  set(_sen_spdlog_found_where "${SPDLOG_INCLUDE_DIR}")
endif()

# spdlog types cross the kernel's API boundary, so a component compiled against a different spdlog
# links and then misbehaves. Warn rather than refuse: a consumer whose package manager picked the
# version often cannot change it, and refusing would stop a build that works today.
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
              " a component built this way links and then misbehaves at run time."
              " Sen's own copy is at ${_sen_spdlog_vendored_dir}."
              " Set SEN_SUPPRESS_SPDLOG_VERSION_WARNING to silence this."
    )
  endif()
endif()
