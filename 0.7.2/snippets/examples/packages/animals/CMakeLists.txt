# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET animals
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Example package"
  SOURCES src/animal.h src/dog_impl.cpp src/cat_impl.cpp
  STL_FILES stl/animal.stl stl/dog.stl stl/cat.stl
  SCHEMA_PATH ${CMAKE_CURRENT_BINARY_DIR}
)
