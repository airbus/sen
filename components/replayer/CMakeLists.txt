# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/spdlog)

set(_replayer_stl_files stl/sen/components/replayer/configuration.stl stl/sen/components/replayer/replayer.stl)

add_sen_component(
  TARGET replayer
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Allows replaying archives"
  SOURCES src/component.cpp
          src/replayed_object.h
          src/replayed_object.cpp
          src/replayed_object_proxy.h
          src/replayed_object_proxy.cpp
          src/replayer.h
          src/replayer.cpp
          src/replay.h
          src/replay.cpp
          src/util.h
          src/util.cpp
  BASE_PATH ${CMAKE_CURRENT_SOURCE_DIR}
  STL_FILES ${_replayer_stl_files}
  DEPS sen::db
  PRIVATE_DEPS spdlog::spdlog
)

set_target_properties(replayer PROPERTIES FOLDER "components/replayer")

# install interfaces files
install(FILES ${_replayer_stl_files} DESTINATION interfaces/stl/sen/components/replayer)

sen_internal_install(replayer)

install(FILES ${PROJECT_SOURCE_DIR}/schemas/replayer.json DESTINATION schemas)
