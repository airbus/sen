# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(tps/spdlog)

add_sen_package(
  TARGET log_emitter
  MAINTAINER "Enrique Parodi Spalazzi (enrique.parodi@airbus.com)"
  VERSION "0.0.1"
  DESCRIPTION "Test utility that emits log messages at various levels and to stdout/stderr"
  SOURCES src/log_emitter_impl.h src/log_emitter_impl.cpp
  STL_FILES stl/log_emitter/log_emitter.stl
  PRIVATE_DEPS spdlog::spdlog
)

sen_internal_install(log_emitter)
