# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# A package whose model takes a member from a file outside its FOM directory. The source file
# below calls the accessor the contributed attribute generates, so the chain from
# HLA_EXTENSIONS_FILE through the generator to compilable code is checked by building this.
add_sen_package(
  TARGET fom_extension_model
  HLA_FOM_DIRS hla
  HLA_EXTENSIONS_FILE extensions/site_sensor.xml
  NO_SCHEMA PUBLIC_SYMBOLS
)

add_sen_package(
  TARGET fom_extension
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "A FOM package with a contributed attribute"
  SOURCES src/uses_contributed_attribute.cpp
  DEPS fom_extension_model
  NO_SCHEMA
)

set_target_properties(fom_extension_model PROPERTIES FOLDER "test/fom_extension")
set_target_properties(fom_extension PROPERTIES FOLDER "test/fom_extension")
