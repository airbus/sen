# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_test(
  NAME sen_collect_export_args
  COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/sen_collect_export_args_test.cmake
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# A plain add_test does not get the sanitizer options the suite helpers attach, and the lane's
# canary refuses any test without a log path: a finding there would be written nowhere.
add_sanitizer_options(sen_collect_export_args)
