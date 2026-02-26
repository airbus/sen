# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/spdlog)
include(tps/asio)
include(tps/concurrentqueue)

add_sen_component(
  TARGET ether
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Ethernet transport for sen"
  SOURCES src/util.h
          src/util.cpp
          src/util_win32.cpp
          src/util_posix.cpp
          src/bus_handler.h
          src/bus_handler.cpp
          src/multicast_beamer.h
          src/multicast_beamer.cpp
          src/beamer.h
          src/beamer.cpp
          src/tcp_discovery_hub.h
          src/tcp_discovery_hub.cpp
          src/tcp_beamer.h
          src/tcp_beamer.cpp
          src/tcp_beam_tracker.h
          src/tcp_beam_tracker.cpp
          src/multicast_beam_tracker.h
          src/multicast_beam_tracker.cpp
          src/beam_tracker.h
          src/beam_tracker.cpp
          src/component.cpp
          src/discovery.h
          src/discovery.cpp
          src/ether_transport.h
          src/ether_transport.cpp
          src/process_handler.h
          src/process_handler.cpp
          src/output_queue.h
          src/shared_buffer_sequence.h
  STL_FILES stl/discovery.stl stl/runtime.stl stl/configuration.stl
  PRIVATE_DEPS
    sen::kernel
    asio::asio
    spdlog::spdlog
    concurrentqueue::concurrentqueue
)

sen_enable_static_analysis(ether)

set_target_properties(ether PROPERTIES FOLDER "components/ether")

sen_internal_install(ether)

install(FILES ${PROJECT_SOURCE_DIR}/schemas/ether.json DESTINATION schemas)
