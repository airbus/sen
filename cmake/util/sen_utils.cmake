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

# try to use ccache, if available (totally optional)
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  message(NOTICE "-- Using ccache to speedup builds")
  set(ccacheEnv
      CCACHE_BASEDIR=${CMAKE_BINARY_DIR}
      CCACHE_SLOPPINESS=clang_index_store,include_file_ctime,include_file_mtime,locale,pch_defines,time_macros
  )

  foreach(lang IN ITEMS C CXX CUDA)
    set(CMAKE_${lang}_COMPILER_LAUNCHER
        ${CMAKE_COMMAND}
        -E
        env
        ${ccacheEnv}
        ${CCACHE_PROGRAM}
    )
  endforeach()
endif()

if(NOT SEN_DISABLE_CLANG_TIDY)
  find_program(clang_tidy_cache_path NAMES "cltcache")

  if(clang_tidy_cache_path)
    find_program(_clang_tidy_path NAMES "clang-tidy" "clang-tidy-20")

    set(clang_tidy_path
        "${clang_tidy_cache_path};${_clang_tidy_path}"
        CACHE STRING "A combined command to run clang-tidy with caching wrapper"
    )
    message(NOTICE "-- Using cltcache to speedup builds")
  else()
    find_program(clang_tidy_path NAMES "clang-tidy" "clang-tidy-20")
  endif()
  message(STATUS "Clang-tidy enabled")
else()
  message(STATUS "Clang-tidy disabled")
endif()
