# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET annoying_component
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Executes a method call during object removal"
  SOURCES component.cpp
  IS_COMPONENT
)
