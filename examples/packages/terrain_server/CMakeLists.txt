# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET terrain_server
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Dummy terrain server and client using the HLA object model"
  STL_FILES stl/basic_types.stl stl/terrain_server.stl stl/terrain_client.stl
  SOURCES src/terrain_server_impl.cpp src/terrain_client_impl.cpp
  DEPS hla_fom
  SCHEMA_PATH ${CMAKE_CURRENT_BINARY_DIR}
)
