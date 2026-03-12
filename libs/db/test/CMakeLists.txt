# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

set(db_tests
    annotation_test.cpp
    creation_test.cpp
    deletion_test.cpp
    event_test.cpp
    input_test.cpp
    keyframe_test.cpp
    output_test.cpp
    property_change_test.cpp
    snapshot_test.cpp
    util_test.cpp
)

add_sen_unit_test_suite(
  db_test
  ${db_tests}
  LINK_DEPS
  sen::core
  sen::kernel
  sen::db
)

sen_generate_cpp(
  TARGET db_test
  STL_FILES stl/db_test_class.stl
  GEN_HDR_FILES public_generated_files
)

target_include_directories(
  db_test SYSTEM
  INTERFACE
  PRIVATE ${CMAKE_SOURCE_DIR}/libs/db/include ${CMAKE_SOURCE_DIR}/libs/db/src
)

set_target_properties(
  db_test PROPERTIES FOLDER "test" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)
