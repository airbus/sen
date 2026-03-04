# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET dummy_fom
  HLA_FOM_DIRS hla/rpr
  HLA_MAPPINGS_FILE hla/mapping.xml
  NO_SCHEMA PUBLIC_SYMBOLS
)

add_sen_package(
  TARGET dummy_entities
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Dummy entities for testing"
  SOURCES src/dummy_aircraft.cpp src/util.h src/util.cpp
  DEPS dummy_fom
  NO_SCHEMA
)

set_target_properties(dummy_entities PROPERTIES FOLDER "test/dummy_entities")
