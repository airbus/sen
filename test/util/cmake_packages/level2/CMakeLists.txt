# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

if(WIN32)
  return()
endif()

# Package which depends on a package (level1) which depends on another package (level0)
# Additional stl file to extend the functionality provided by its dependencies.
add_sen_package(
  TARGET level2
  MAINTAINER "Sen Core Team (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Level2"
  SOURCES src/level2_class.h src/level2_class.cpp
  STL_FILES stl/level2.stl
  DEPS level1
  NO_SCHEMA PUBLIC_SYMBOLS
)

# Package which depends on a package generated as static (level1S_static) which depends on another package generated
# as static (level0_static).
# Additional stl file to extend the functionality provided by its dependencies.
add_sen_package(
  TARGET level2S
  MAINTAINER "Sen Core Team (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Level2"
  SOURCES src/level2_class_s.h src/level2_class_s.cpp
  STL_FILES stl/level2.stl
  DEPS level1S
  NO_SCHEMA PUBLIC_SYMBOLS
)

# smoke tests
add_sen_run_smoke_test(
  level2_smoke CONFIG_FILE ${PROJECT_SOURCE_DIR}/test/config/cmake_packages/level2_dynamic.yaml
)
add_sen_run_smoke_test(
  level2S_smoke CONFIG_FILE ${PROJECT_SOURCE_DIR}/test/config/cmake_packages/level2_static.yaml
)
