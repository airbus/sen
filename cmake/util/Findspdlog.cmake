# === Findspdlog.cmake =================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(FindPackageHandleStandardArgs)

find_package(spdlog CONFIG QUIET)

if(NOT spdlog_FOUND AND NOT TARGET spdlog::spdlog)
  find_path(SPDLOG_SYSTEM_INCLUDE_DIR NAMES spdlog/spdlog.h)

  # The vendored copy first: Sen's binaries are built against it, and compiling a consumer against
  # a different spdlog is an ABI mismatch with no diagnostic.
  if(EXISTS "${SEN_THIRD_PARTY_INCLUDE_DIR}/spdlog/spdlog.h")
    # Not <prefix>/include, which is already on a consumer's include path. The header is tested
    # for rather than the variable: find_package_handle_standard_args checks only that it is
    # non-empty, so a missing directory would report "found".
    set(SPDLOG_INCLUDE_DIR "${SEN_THIRD_PARTY_INCLUDE_DIR}")
  elseif(SPDLOG_SYSTEM_INCLUDE_DIR)
    # The fallback, for a tree installed without third_party/include -- which is what a
    # distribution package that unbundles it looks like.
    set(SPDLOG_INCLUDE_DIR "${SPDLOG_SYSTEM_INCLUDE_DIR}")
  endif()

  find_package_handle_standard_args(spdlog REQUIRED_VARS SPDLOG_INCLUDE_DIR)

  if(spdlog_FOUND AND NOT TARGET spdlog::spdlog)
    add_library(spdlog::spdlog INTERFACE IMPORTED)
    set_target_properties(spdlog::spdlog PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${SPDLOG_INCLUDE_DIR}")

    if(EXISTS "${SPDLOG_INCLUDE_DIR}/fmt/core.h")
      set_property(
        TARGET spdlog::spdlog
        APPEND
        PROPERTY INTERFACE_COMPILE_DEFINITIONS "SPDLOG_FMT_EXTERNAL"
      )
    endif()
  endif()
endif()
