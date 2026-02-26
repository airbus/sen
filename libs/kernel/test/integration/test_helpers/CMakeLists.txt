# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

sen_generate_package(
  TARGET test_helpers
  SOURCES include/test_helpers/helpers.h
  STL_FILES stl/test_helpers/tester.stl
  BASE_PATH stl
  CODEGEN_SETTINGS codegen_settings.json
)

target_include_directories(test_helpers PUBLIC include)
