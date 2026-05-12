# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/pybind11)

find_package(Python3 REQUIRED COMPONENTS Development)

set(sources
    module.cpp
    variant_conversion.cpp
    variant_conversion.h
    type_casters.h
)

# -------------------------------------------------------------------------------------------------------------
# library
# -------------------------------------------------------------------------------------------------------------

python3_add_library(sen_db_python MODULE ${sources})
target_link_libraries(sen_db_python PRIVATE pybind11::pybind11 sen::db sen::core)

sen_internal_configure_lib(sen_db_python)
sen_enable_static_analysis(sen_db_python)
sen_internal_install(sen_db_python)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # For some gcc version, we need to disable array-bounds checking as they
  # overidentiy issue inside pybind.

  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14.0 AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 12.0)
    target_compile_options(sen_db_python PRIVATE -Wno-array-bounds -Wno-stringop-overread)
  endif()
endif()

# -------------------------------------------------------------------------------------------------------------
# IDE grouping
# -------------------------------------------------------------------------------------------------------------

source_group("src" FILES ${sources})
