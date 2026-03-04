# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/tracy)

add_sen_package(
  TARGET tracy
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Tracy-based tracer"
  SOURCES src/component.cpp
  DEPS sen::kernel
  PRIVATE_DEPS Tracy::TracyClient
  IS_COMPONENT
)

set_target_properties(tracy PROPERTIES FOLDER "components/tracy")

add_compile_definitions(TRACY_DELAYED_INIT TRACY_MANUAL_LIFETIME)

sen_internal_install(tracy)
