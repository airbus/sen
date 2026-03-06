# === install.cmake ====================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

# -------------------------------------------------------------------------------------------------------------
# targets
# -------------------------------------------------------------------------------------------------------------

# wrapper for exporting the target sen::sen, which is required by Conan
add_library(sen INTERFACE)
target_link_libraries(sen INTERFACE core db kernel)

# -------------------------------------------------------------------------------------------------------------
# configuration of install directories
# -------------------------------------------------------------------------------------------------------------

set(_cmakedir_desc "Directory relative to CMAKE_INSTALL to install the cmake configuration files")

set(CMAKE_INSTALL_CMAKEDIR
    "cmake/sen"
    CACHE STRING "${_cmakedir_desc}"
)

mark_as_advanced(CMAKE_INSTALL_CMAKEDIR)

# -------------------------------------------------------------------------------------------------------------
# export
# -------------------------------------------------------------------------------------------------------------

# Export the targets to a script
install(
  EXPORT sen_targets
  FILE sen_targets.cmake
  NAMESPACE sen::
  DESTINATION "${CMAKE_INSTALL_CMAKEDIR}"
)

# -------------------------------------------------------------------------------------------------------------
# configuration file for CMake-based user consumption
# -------------------------------------------------------------------------------------------------------------

# configure and install the -config.cmake.in files.
configure_exportable_packages(INTERFACES_CONFIG_DIRS ${CMAKE_CURRENT_LIST_DIR}/interfaces)

# Create a ConfigVersion.cmake file
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/SenConfigVersion.cmake
  VERSION ${sen_VERSION}
  COMPATIBILITY AnyNewerVersion
)

# Install the configVersion package
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/SenConfigVersion.cmake DESTINATION ${CMAKE_INSTALL_CMAKEDIR})

# Install required sen utils cmakefiles
install(
  FILES ${PROJECT_SOURCE_DIR}/cmake/util/sen_utils.cmake
        ${PROJECT_SOURCE_DIR}/cmake/util/sen_misc_utils.cmake
        ${PROJECT_SOURCE_DIR}/cmake/util/sen_codegen_utils.cmake
        ${PROJECT_SOURCE_DIR}/cmake/util/sen_package_utils.cmake
        ${PROJECT_SOURCE_DIR}/cmake/util/git_info.cmake
        ${PROJECT_SOURCE_DIR}/cmake/util/git_info.cmake.in
  DESTINATION ${CMAKE_INSTALL_CMAKEDIR}/util
)

# -------------------------------------------------------------------------------------------------------------
# licenses
# -------------------------------------------------------------------------------------------------------------

# our license
install(FILES ${PROJECT_SOURCE_DIR}/LICENSE.txt DESTINATION .)

# FOSS licenses
if(EXISTS "${PROJECT_SOURCE_DIR}/build/foss_licenses")
  install(DIRECTORY "${PROJECT_SOURCE_DIR}/build/foss_licenses" DESTINATION .)
endif()

# -------------------------------------------------------------------------------------------------------------
# CPack configuration
# -------------------------------------------------------------------------------------------------------------

if(WIN32)
  set(CPACK_GENERATOR ZIP)
else()
  set(CPACK_GENERATOR TGZ)
endif()
set(CPACK_PACKAGE_NAME "sen")
set(CPACK_PACKAGE_VENDOR "Airbus")
get_git_tags(tags_of_current_commit)
list(LENGTH tags_of_current_commit num_tags)
if(num_tags EQUAL 0)
  set(SEN_ZIP_VERSION "latest")
else()
  list(
    GET
    tags_of_current_commit
    0
    selected_tag
  )
  set(SEN_ZIP_VERSION ${selected_tag})
endif()

string(
  TOLOWER
    "${CPACK_PACKAGE_NAME}-${SEN_ZIP_VERSION}-${CMAKE_HOST_SYSTEM_PROCESSOR}-${CMAKE_SYSTEM_NAME}-${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}-${CMAKE_BUILD_TYPE}"
    CPACK_PACKAGE_FILE_NAME
)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
set(CPACK_VERBATIM_VARIABLES YES)

# RPM specifics.
#
# To create the rpm via command line we need to set following variables via cpack:
#   -DCPACK_PACKAGING_INSTALL_PREFIX=/opt/sen
#   -DCPACK_COMPONENTS_ALL='Unspecified;rpm'
#   -DCPACK_PACKAGE_RELOCATABLE=0
set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_GROUP "Utilities/Simulation")
set(CPACK_RPM_PACKAGE_LICENSE "Apache-2.0")
set(CPACK_RPM_PACKAGE_DESCRIPTION "For more information about Sen please visit the specified URL")

# This suppresses the automatic installation of linker build-id information under /usr/lib/
# This seems to be some Fedora feature also enabled by default for Debian rpmbuild (as of Debian 12)
set(CPACK_RPM_SPEC_MORE_DEFINE "%define _build_id_links none")

include(CPack)
