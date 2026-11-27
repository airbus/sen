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

add_test(
  NAME sen_package_object_exports
  COMMAND
    ${CMAKE_COMMAND} -DSEN_SOURCE_DIR=${PROJECT_SOURCE_DIR}
    -DTEST_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/object_exports
    -DTEST_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/object_exports -DTEST_GENERATOR=${CMAKE_GENERATOR}
    -DTEST_PLATFORM=${CMAKE_GENERATOR_PLATFORM} -DTEST_TOOLSET=${CMAKE_GENERATOR_TOOLSET}
    -DTEST_COMPILER=${CMAKE_CXX_COMPILER} -DTEST_CONFIG=$<CONFIG> -P
    ${CMAKE_CURRENT_SOURCE_DIR}/object_exports/run.cmake
)
add_sanitizer_options(sen_package_object_exports)
