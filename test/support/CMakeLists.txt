# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

set(archive_sources src/archive_test_helpers.h src/archive_test_helpers.cpp)

# -------------------------------------------------------------------------------------------------------------
# library
# -------------------------------------------------------------------------------------------------------------

add_library(archive_test_helpers STATIC ${archive_sources})

sen_enable_static_analysis(archive_test_helpers)

target_include_directories(archive_test_helpers PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>")

target_link_libraries(archive_test_helpers PUBLIC sen::core sen::db)

# -------------------------------------------------------------------------------------------------------------
# IDE grouping
# -------------------------------------------------------------------------------------------------------------

source_group("sources" FILES ${archive_sources})

set_target_properties(archive_test_helpers PROPERTIES FOLDER "test/support")
