# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

set(public_headers include/sen/test_support/archive_test_helpers.h)

# -------------------------------------------------------------------------------------------------------------
# library
# -------------------------------------------------------------------------------------------------------------

add_library(archive_test_helpers INTERFACE)
target_sources(
  archive_test_helpers
  INTERFACE FILE_SET
            HEADERS
            BASE_DIRS
            include/
            FILES
            ${public_headers}
)

sen_enable_static_analysis(archive_test_helpers)

target_include_directories(
  archive_test_helpers INTERFACE "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_link_libraries(archive_test_helpers INTERFACE sen::core sen::db)

# -------------------------------------------------------------------------------------------------------------
# IDE grouping
# -------------------------------------------------------------------------------------------------------------

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include/ FILES ${public_headers})

set_target_properties(archive_test_helpers PROPERTIES FOLDER "test/support")
