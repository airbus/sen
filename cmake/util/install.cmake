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

# sen_coverage_flags must be part of the export set because sen_configure_target
# links every internal target against it. CMake requires that all dependencies of
# exported targets are themselves exported or are known imported targets.
# When installed without coverage enabled this target is an empty INTERFACE library
# and is harmless to downstream consumers.
install(TARGETS sen_coverage_flags EXPORT sen_targets)

# -------------------------------------------------------------------------------------------------------------
# configuration of install directories
# -------------------------------------------------------------------------------------------------------------

set(_cmakedir_desc "Directory relative to CMAKE_INSTALL to install the cmake configuration files")

set(CMAKE_INSTALL_CMAKEDIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/sen"
    CACHE STRING "${_cmakedir_desc}"
)

mark_as_advanced(CMAKE_INSTALL_CMAKEDIR)

# A cache entry keeps its first value, so a build tree from before this move goes on installing the
# old layout with no diagnostic.
if(NOT
   CMAKE_INSTALL_CMAKEDIR
   STREQUAL
   "${CMAKE_INSTALL_LIBDIR}/cmake/sen"
)
  message(
    WARNING
      "CMAKE_INSTALL_CMAKEDIR is '${CMAKE_INSTALL_CMAKEDIR}', not the '${CMAKE_INSTALL_LIBDIR}/cmake/sen' "
      "this release installs to. A build tree configured before the package config moved keeps the old "
      "value: delete CMakeCache.txt, or pass -DCMAKE_INSTALL_CMAKEDIR=${CMAKE_INSTALL_LIBDIR}/cmake/sen, "
      "unless you set it deliberately."
  )
endif()

# Sen utils cmake files provided with the Sen package
set(CMAKE_UTILS_FILES
    ${PROJECT_SOURCE_DIR}/cmake/util/sen_utils.cmake
    ${PROJECT_SOURCE_DIR}/cmake/util/sen_misc_utils.cmake
    ${PROJECT_SOURCE_DIR}/cmake/util/sen_codegen_utils.cmake
    ${PROJECT_SOURCE_DIR}/cmake/util/sen_package_utils.cmake
    ${PROJECT_SOURCE_DIR}/cmake/util/git_info.cmake
    ${PROJECT_SOURCE_DIR}/cmake/util/git_info.cmake.in
    # configure_exportable_packages generates a forwarding config from this, so it has to travel
    # with the utils rather than stay in the source tree.
    ${PROJECT_SOURCE_DIR}/cmake/util/exportable-config-compat.cmake.in
    ${PROJECT_SOURCE_DIR}/cmake/util/exportable-config-version-compat.cmake.in
)

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

# Export used when the package is being consumed in conan editable mode
export(
  EXPORT sen_targets
  FILE "${CMAKE_BINARY_DIR}/sen_targets.cmake"
  NAMESPACE sen::
)

# -------------------------------------------------------------------------------------------------------------
# configuration file for CMake-based user consumption
# -------------------------------------------------------------------------------------------------------------

# configure and install the -config.cmake.in files.
configure_exportable_packages(INTERFACES_CONFIG_DIRS ${CMAKE_CURRENT_LIST_DIR}/interfaces)

# Create a ConfigVersion.cmake file
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/sen-config-version.cmake
  VERSION ${sen_VERSION}
  COMPATIBILITY AnyNewerVersion
)

# Install the configVersion package
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sen-config-version.cmake DESTINATION ${CMAKE_INSTALL_CMAKEDIR})

# Install required sen utils cmake files
install(FILES ${CMAKE_UTILS_FILES} DESTINATION ${CMAKE_INSTALL_CMAKEDIR}/util)

# Install spdlog
install(FILES ${PROJECT_SOURCE_DIR}/cmake/util/Findspdlog.cmake DESTINATION ${CMAKE_INSTALL_CMAKEDIR})

# A config at the location the previous layout used, with its version file, so anything told to put
# <prefix>/cmake on CMAKE_PREFIX_PATH keeps working.
configure_file(
  ${PROJECT_SOURCE_DIR}/cmake/util/sen-config-compat.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/compat/sen-config.cmake @ONLY
)
# Skipped when the real config already installs there, or the forwarder overwrites it and includes
# itself. A leading ./ is stripped first, since "./cmake/sen" names the same directory.
string(
  REGEX
  REPLACE "^\\./"
          ""
          _sen_real_cmakedir
          "${CMAKE_INSTALL_CMAKEDIR}"
)
if(NOT
   _sen_real_cmakedir
   STREQUAL
   "cmake/sen"
)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/compat/sen-config.cmake DESTINATION cmake/sen)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sen-config-version.cmake DESTINATION cmake/sen)
endif()

# We need the sen utils cmake files in the binary dir when working in conan editable mode
file(COPY ${CMAKE_UTILS_FILES} DESTINATION ${CMAKE_BINARY_DIR}/util)

# -------------------------------------------------------------------------------------------------------------
# licenses
# -------------------------------------------------------------------------------------------------------------

# our license
install(FILES ${PROJECT_SOURCE_DIR}/LICENSE.txt DESTINATION ${CMAKE_INSTALL_DOCDIR})

# Third-party shared objects the installed binaries need, resolved from the binaries rather than
# listed, so nothing ships by being remembered or is missed by being forgotten. The exclusions name
# the directories the target system provides.
#
# Not attempted on Windows: it would need DIRECTORIES naming where the DLLs live and
# PRE_EXCLUDE_REGEXES for the api-ms-* stubs, neither of which is supplied here.
#
# The framework and homebrew entries are load-bearing on macOS. Without them the resolver finds
# Python's framework binary, has no FRAMEWORK DESTINATION for it, and the install fails.
if(NOT WIN32)
  install(
    RUNTIME_DEPENDENCY_SET
    sen_runtime_deps
    POST_EXCLUDE_REGEXES
    "^/lib"
    "^/usr/lib"
    "^/usr/local/lib"
    "^/opt/rh"
    "^/nix/store"
    "^/System/Library"
    "^/Library/Frameworks"
    "^/opt/homebrew"
    "^/opt/local"
    RUNTIME
    DESTINATION
    ${CMAKE_INSTALL_BINDIR}
    LIBRARY
    DESTINATION
    ${CMAKE_INSTALL_LIBDIR}
  )
endif()

# FOSS licenses
if(EXISTS "${CMAKE_BINARY_DIR}/foss_licenses")
  install(DIRECTORY "${CMAKE_BINARY_DIR}/foss_licenses" DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()

# -------------------------------------------------------------------------------------------------------------
# resources
# -------------------------------------------------------------------------------------------------------------

# syntax highlighting
install(DIRECTORY "${CMAKE_SOURCE_DIR}/resources/syntax_highlighting"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/sen/resources"
)

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
