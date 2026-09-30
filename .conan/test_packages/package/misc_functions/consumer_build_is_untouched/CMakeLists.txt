# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# find_package(sen) has already run, and must not have decided how this project compiles its own
# code: a launcher here routes every compile through cmake -E env and replaced CCACHE_SLOPPINESS.
foreach(lang IN ITEMS C CXX CUDA)
  if(CMAKE_${lang}_COMPILER_LAUNCHER)
    message(FATAL_ERROR "find_package(sen) set CMAKE_${lang}_COMPILER_LAUNCHER to "
                        "'${CMAKE_${lang}_COMPILER_LAUNCHER}'"
    )
  endif()
endforeach()
