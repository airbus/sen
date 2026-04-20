# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

add_sen_package(
  TARGET term_showcase
  MAINTAINER "Sen Team <sen-dev@airbus.com>"
  VERSION "0.0.1"
  DESCRIPTION
    "Showcase class with methods that exercise every editor in the term component's guided-input form"
  SOURCES src/showcase_impl.h src/showcase_impl.cpp src/worker_impl.h
  STL_FILES stl/term_showcase/term_showcase.stl
)

sen_internal_install(term_showcase)
