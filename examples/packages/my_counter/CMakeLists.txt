# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET my_counter
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "code of the Hello Sen tutorial"
  SOURCES src/counter.h src/counter.cpp
  STL_FILES stl/my_counter/counter.stl
)
