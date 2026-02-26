# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET aircrafts
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Dummy entities for testing"
  STL_FILES stl/dummy_aircraft.stl
  SOURCES src/dummy_aircraft_impl.cpp
  DEPS hla_fom sen::util
  SCHEMA_PATH ${CMAKE_CURRENT_BINARY_DIR}
)
