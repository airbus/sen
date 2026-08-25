# === CMakeLists.txt ==================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# =====================================================================================================================

add_sen_benchmark(
  util_benchmark
  dr/dead_reckoner_benchmark.cpp
  LINK_DEPS
  sen::core
  sen::util
)

target_include_directories(util_benchmark PRIVATE ${CMAKE_SOURCE_DIR}/libs/util/src/dr)
