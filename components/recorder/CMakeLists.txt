# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/spdlog)

set(_recorder_stl_files stl/sen/components/recorder/configuration.stl stl/sen/components/recorder/recorder.stl)

add_sen_package(
  TARGET recorder
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Allows recording objects"
  SOURCES src/component.cpp
          src/util.h
          src/util.cpp
          src/recorder.h
          src/recorder.cpp
  BASE_PATH ${CMAKE_CURRENT_SOURCE_DIR}
  STL_FILES ${_recorder_stl_files}
  DEPS sen::db
  PRIVATE_DEPS spdlog::spdlog
  IS_COMPONENT
)

set_target_properties(recorder PROPERTIES FOLDER "components/recorder")

# install interfaces files
install(FILES ${_recorder_stl_files} DESTINATION interfaces/stl/sen/components/recorder)

sen_internal_install(recorder)

install(FILES ${PROJECT_SOURCE_DIR}/schemas/recorder.json DESTINATION schemas)

# tests
add_subdirectory(test)
