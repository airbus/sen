# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

cmake_minimum_required(VERSION 3.12)
project(test_package LANGUAGES CXX C)

find_package(sen REQUIRED CONFIG)

add_subdirectory(my_package)
