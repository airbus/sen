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

if(NOT SEN_DISABLE_CLANG_TIDY)
  # clang-tidy-cache is matus-chochlik/ctcache; cltcache is kept because a developer may already
  # have it. Either one hashes the preprocessed source, the arguments and the clang-tidy config,
  # so a finding cannot survive a change to any of them.
  find_program(clang_tidy_cache_path NAMES "clang-tidy-cache" "cltcache")

  if(clang_tidy_cache_path)
    find_program(_clang_tidy_path NAMES "clang-tidy-20" "clang-tidy" REQUIRED)

    set(clang_tidy_path
        "${clang_tidy_cache_path};${_clang_tidy_path}"
        CACHE STRING "A combined command to run clang-tidy with caching wrapper"
    )
    message(NOTICE "-- Using ${clang_tidy_cache_path} to speedup clang-tidy")
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
