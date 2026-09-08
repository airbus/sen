# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# A package exported the way the guides tell a consumer to export one: Sen's helper, its own
# directory under the library directory, and a version file beside the config.
cmake_minimum_required(VERSION 3.21)
project(
  radar
  VERSION 1.2.3
  LANGUAGES NONE
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(${SEN_CMAKE_UTILS_DIR}/sen_misc_utils.cmake)

configure_exportable_packages(TARGET_NAME_CMAKEDIRS)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/radar-config-version.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/radar-config-version.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/radar
)
