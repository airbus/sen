# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_unit_test_suite(
  kernel_test
  bus/remote_interest_handler_test.cpp
  test_kernel/test_kernel_test.cpp
  LINK_DEPS
  sen::core
  sen::kernel
)

sen_generate_cpp(
  TARGET kernel_test
  STL_FILES test_kernel/stl/my_class.stl
  GEN_HDR_FILES public_generated_files
)

target_include_directories(
  kernel_test
  PRIVATE ${PROJECT_SOURCE_DIR}/libs/kernel/src
  PRIVATE ${PROJECT_SOURCE_DIR}/libs/kernel/src/bus
  PRIVATE ${asio_SOURCE_DIR}
  PRIVATE ${concurrentqueue_SOURCE_DIR}
  PRIVATE ${spdlog_SOURCE_DIR}/include
)

set_target_properties(
  kernel_test PROPERTIES FOLDER "test" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)
