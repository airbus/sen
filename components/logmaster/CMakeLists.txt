# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/spdlog)

add_sen_component(
  TARGET logmaster
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Allows controlling the loggers"
  SOURCES src/component.cpp
          src/logger_impl.h
          src/logger_impl.cpp
          src/logmaster_impl.h
          src/logmaster_impl.cpp
  STL_FILES stl/logmaster.stl
  DEPS sen::kernel
  PRIVATE_DEPS spdlog::spdlog
)

set_target_properties(logmaster PROPERTIES FOLDER "components/logmaster")

sen_internal_install(logmaster)

install(FILES ${PROJECT_SOURCE_DIR}/schemas/logmaster.json DESTINATION schemas)
