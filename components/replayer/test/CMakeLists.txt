# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

set(replayer_tests replayer_test.cpp replay_test.cpp replayed_object_test.cpp)

add_sen_unit_test_suite(
  replayer_test
  ${replayer_tests}
  LINK_DEPS
  sen::core
  sen::kernel
  sen::db
  replayer_for_testing
  archive_test_helpers
)

sen_generate_cpp(
  TARGET replayer_test
  STL_FILES stl/replay_test_class.stl
  GEN_HDR_FILES public_generated_files
)

target_include_directories(
  replayer_test SYSTEM
  INTERFACE
  PRIVATE ${CMAKE_SOURCE_DIR}/components/replayer/src
)

target_compile_definitions(replayer_test PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

set_target_properties(
  replayer_test PROPERTIES FOLDER "test" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)
