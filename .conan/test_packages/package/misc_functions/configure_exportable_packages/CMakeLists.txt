# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

set(CMAKE_INSTALL_CMAKEDIR "cmake/exportable_packages")
set(EXPORTABLE_PACKAGE_VALUE "configured-by-helper")

configure_exportable_packages(INTERFACES_CONFIG_DIRS ${CMAKE_CURRENT_LIST_DIR}/additional)

set(CONFIGURE_EXPORTABLE_PACKAGES_INSTALL_ROOT "${CMAKE_CURRENT_BINARY_DIR}/install")
set(CONFIGURE_EXPORTABLE_PACKAGES_INSTALL_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/cmake_install.cmake")
configure_file(
  test_configure_exportable_packages.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/test_configure_exportable_packages.cmake @ONLY
)

add_test(NAME cmake.configure_exportable_packages.install_and_consume
         COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/test_configure_exportable_packages.cmake
)
