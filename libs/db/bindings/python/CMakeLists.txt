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

# -------------------------------------------------------------------------------------------------------------
# IDE grouping
# -------------------------------------------------------------------------------------------------------------

source_group("src" FILES ${sources})
