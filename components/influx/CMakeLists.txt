# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/spdlog)
include(tps/asio)

add_sen_package(
  TARGET influx
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Ethernet transport for sen"
  SOURCES src/component.cpp
          src/data_point.h
          src/data_point.cpp
          src/database.h
          src/database.cpp
          src/recorder.h
          src/recorder.cpp
          src/tcp.h
          src/udp.h
          src/util.h
          src/util.cpp
  STL_FILES stl/configuration.stl
  PRIVATE_DEPS asio::asio spdlog::spdlog
  IS_COMPONENT
)

sen_enable_static_analysis(influx)

set_target_properties(influx PROPERTIES FOLDER "components/influx")

sen_internal_install(influx)

install(FILES ${PROJECT_SOURCE_DIR}/schemas/influx.json DESTINATION schemas)
