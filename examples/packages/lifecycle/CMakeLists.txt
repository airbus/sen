# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET lifecycle
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "the object lifecycle hooks, shown in howto_guides/objects.md"
  SOURCES src/phase_recorder.h src/phase_recorder.cpp
  STL_FILES stl/lifecycle/phase_recorder.stl
)
