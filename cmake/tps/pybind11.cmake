# === pybind11.cmake ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include_guard()

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.27.0")
  cmake_policy(SET CMP0148 OLD)
endif()

# search for Python3 so that pybind11 does not do it on its own and possibly creates a version mismatch
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

find_package(pybind11 REQUIRED)
