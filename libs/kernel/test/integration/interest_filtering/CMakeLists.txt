# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET interest_filtering
  STL_FILES stl/interest_filtering.stl
  SOURCES src/interest_filtering.cpp
  NO_SCHEMA
  PRIVATE_DEPS spdlog::spdlog test_helpers
)

# common environment for the interest filtering integration tests
find_package(Python REQUIRED COMPONENTS Interpreter)
set(_working_dir $<TARGET_FILE_DIR:sen::cli_run>)
set(_script ${CMAKE_CURRENT_SOURCE_DIR}/../run.py)

# We check that the interest condition on a float property works properly in conjunction with an interest without conditions (selectAll) in the same and different components
add_sen_integration_test(
  interest_filtering_float_query_filtering
  COMMAND
  ${Python_EXECUTABLE}
  ${_script}
  ${_working_dir}
  ${CMAKE_CURRENT_SOURCE_DIR}/config/float_query_filtering_1.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/config/float_query_filtering_2.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/config/float_query_filtering_3.yaml
  REQ_DEPS interest_filtering py ether kernel FLAKY USE_TESTCONTAINERS
)

# Checks that, with multiple subscriptions attached to a single LocalParticipant. Only one proxy per object is created
add_sen_integration_test(
  interest_filtering_multi_subscription
  COMMAND
  ${Python_EXECUTABLE}
  ${_script}
  ${_working_dir}
  ${CMAKE_CURRENT_SOURCE_DIR}/config/multi_subscription_1.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/config/multi_subscription_2.yaml
  ${CMAKE_CURRENT_SOURCE_DIR}/config/multi_subscription_3.yaml
  REQ_DEPS interest_filtering py ether kernel FLAKY USE_TESTCONTAINERS
)
