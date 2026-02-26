# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET shapes
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Shapes example"
  SOURCES src/shapes.cpp src/shape_listener.cpp
  STL_FILES stl/shapes.stl stl/shape_listener.stl
  SCHEMA_PATH ${CMAKE_CURRENT_BINARY_DIR}
)
