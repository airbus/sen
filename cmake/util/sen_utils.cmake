# === sen_utils.cmake ==================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include_guard()

include(util/git_info)
include(CheckCXXCompilerFlag)

# cmake miscellaneous utils used in sen
include(util/sen_misc_utils)

# utils used for code generation
include(util/sen_codegen_utils)

# utils used to create Sen packages
include(util/sen_package_utils)

# try to use a compiler cache, if available (totally optional). ccache where it exists,
# sccache otherwise: there is no ccache for MSVC, so the Windows leg recompiled every
# translation unit on every run. The environment below is ccache's own and means nothing
# to sccache, so it is set only on that path.
find_program(SEN_COMPILER_CACHE NAMES ccache sccache)
if(SEN_COMPILER_CACHE)
  get_filename_component(_sen_cache_name "${SEN_COMPILER_CACHE}" NAME_WE)
  message(NOTICE "-- Using ${_sen_cache_name} to speedup builds")

  set(_sen_cache_env)
  if(_sen_cache_name STREQUAL "ccache")
    set(_sen_cache_env
        CCACHE_BASEDIR=${CMAKE_BINARY_DIR}
        # No time_macros: nothing bakes __DATE__ or __TIME__ in any more, and the setting told
        # ccache to serve an object whose embedded timestamp was from whenever it was first built.
        CCACHE_SLOPPINESS=clang_index_store,include_file_ctime,include_file_mtime,locale,pch_defines
    )
  endif()

  foreach(lang IN ITEMS C CXX CUDA)
    set(CMAKE_${lang}_COMPILER_LAUNCHER
        ${CMAKE_COMMAND}
        -E
        env
        ${_sen_cache_env}
        ${SEN_COMPILER_CACHE}
    )
  endforeach()

  # A compiler cache stores nothing against MSVC's separate program database, so a lane that
  # gained one would report hits of zero and no reason. Say so at configure time rather than
  # leaving it to be measured: the flags are not in the build log, which prints only objects.
  if(MSVC)
    message(NOTICE "-- MSVC release flags seen by the cache: ${CMAKE_CXX_FLAGS_RELEASE}")
    if(CMAKE_CXX_FLAGS_RELEASE MATCHES "/Zi" OR CMAKE_MSVC_DEBUG_INFORMATION_FORMAT MATCHES "ProgramDatabase")
      message(WARNING "MSVC is producing a program database; ${_sen_cache_name} will not cache. "
                      "Set CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded with CMP0141 NEW."
      )
    endif()
  endif()
endif()

if(NOT SEN_DISABLE_CLANG_TIDY)
  find_program(clang_tidy_cache_path NAMES "cltcache")

  if(clang_tidy_cache_path)
    find_program(_clang_tidy_path NAMES "clang-tidy-20" "clang-tidy" REQUIRED)

    set(clang_tidy_path
        "${clang_tidy_cache_path};${_clang_tidy_path}"
        CACHE STRING "A combined command to run clang-tidy with caching wrapper"
    )
    message(NOTICE "-- Using cltcache to speedup builds")
  else()
    # Versioned name first: an older clang-tidy from the system would otherwise
    # win and analyse with different checks. REQUIRED because analysis was asked
    # for; without it a missing tool leaves the lane green having analysed
    # nothing.
    find_program(clang_tidy_path NAMES "clang-tidy-20" "clang-tidy" REQUIRED)
  endif()
  message(STATUS "Clang-tidy enabled")
else()
  message(STATUS "Clang-tidy disabled")
endif()
