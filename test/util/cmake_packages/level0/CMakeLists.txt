# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

if(WIN32)
  return()
endif()

# Basic package with stl and sources.
add_sen_package(
  TARGET level0
  MAINTAINER "Sen Core Team (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Level0"
  SOURCES src/level0_class.h src/level0_class.cpp
  STL_FILES stl/level0.stl
  NO_SCHEMA PUBLIC_SYMBOLS
)

target_include_directories(level0 PUBLIC src)

# Package with no stl files, providing a new implementation of a class declared in the level0 package.
add_sen_package(
  TARGET level0_addition
  MAINTAINER "Sen Core Team (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Level0 additional class"
  SOURCES src/level0_class_addition.h src/level0_class_addition.cpp
  DEPS level0
  NO_SCHEMA PUBLIC_SYMBOLS
)

# smoke tests
add_sen_run_smoke_test(level0_smoke CONFIG_FILE ${PROJECT_SOURCE_DIR}/test/config/cmake_packages/level0.yaml)
add_sen_run_smoke_test(
  level0_addition_smoke CONFIG_FILE ${PROJECT_SOURCE_DIR}/test/config/cmake_packages/level0_addition.yaml
)
