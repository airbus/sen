# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET fibonacci
  MAINTAINER "Sen Core Team (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Illustrates the usage of deferred functions"
  SOURCES src/fibonacci.cpp
  STL_FILES stl/fibonacci.stl
  CODEGEN_SETTINGS src/codegen_settings.json
  SCHEMA_PATH ${CMAKE_CURRENT_BINARY_DIR}
)
