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

# TODO (SEN-1735): Enable once the proxies managed in the LocalParticipant have been deduplicated.
## common environment for the interest filtering integration tests
#find_package(Python REQUIRED COMPONENTS Interpreter)
#set(_working_dir $<TARGET_FILE_DIR:sen::cli_run>)
#
## We check that the interest condition on a float property works properly in conjunction with an interest without conditions (selectAll) in the same and different components
#add_sen_integration_test(
#  interest_filtering_test_1
#  COMMAND
#  ${Python_EXECUTABLE}
#  ${CMAKE_CURRENT_SOURCE_DIR}/../run.py
#  ${_working_dir}
#  ${CMAKE_CURRENT_SOURCE_DIR}/config/test_1_proc_1.yaml
#  ${CMAKE_CURRENT_SOURCE_DIR}/config/test_1_proc_2.yaml
#  ${CMAKE_CURRENT_SOURCE_DIR}/config/test_1_proc_3.yaml
#  REQ_DEPS interest_filtering py ether kernel FLAKY USE_TESTCONTAINERS
#)
#
