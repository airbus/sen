# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

if(WIN32)
  return()
endif()

# Package depending on another package (level0) with additional stl file to extend the base package functionality.
add_sen_package(
  TARGET level1
  MAINTAINER "Sen Core Team (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Level1"
  SOURCES src/level1_class.h src/level1_class.cpp
  STL_FILES stl/level1.stl
  DEPS level0
  NO_SCHEMA PUBLIC_SYMBOLS
)

target_include_directories(level1 PUBLIC src)

# Package consuming another package (level0_static) as a static dependency. Includes an additional stl file to extend
# the functionality.
add_sen_package(
  TARGET level1S
  MAINTAINER "Sen Core Team (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Level1"
  SOURCES src/level1_class_s.h src/level1_class_s.cpp
  STL_FILES stl/level1.stl
  DEPS level0
  NO_SCHEMA PUBLIC_SYMBOLS
)

target_include_directories(level1S PUBLIC src)

# smoke tests
add_sen_run_smoke_test(
  level1_smoke CONFIG_FILE ${PROJECT_SOURCE_DIR}/test/config/cmake_packages/level1_dynamic.yaml
)
add_sen_run_smoke_test(
  level1S_smoke CONFIG_FILE ${PROJECT_SOURCE_DIR}/test/config/cmake_packages/level1_static.yaml
)
