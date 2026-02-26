# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET inheritance
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Shows how to implement inheritance"
  SOURCES src/my_class.h
          src/my_class.cpp
          src/my_subclass.h
          src/my_subclass.cpp
          src/my_reactionclass.h
          src/my_reactionclass.cpp
  STL_FILES stl/inheritance/my_class.stl
            stl/inheritance/my_subclass.stl
            stl/inheritance/my_reactionclass.stl
            stl/inheritance/basic_types.stl
  SCHEMA_PATH ${CMAKE_CURRENT_BINARY_DIR}
)

set_target_properties(inheritance PROPERTIES FOLDER "examples/basic")
set_target_properties(inheritance PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_sen_run_smoke_test(inheritance_smoke CONFIG_FILE config/inheritance.yaml)
