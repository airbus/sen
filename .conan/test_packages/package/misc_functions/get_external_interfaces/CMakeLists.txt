# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

function(
  assert_installed_path
  target
  property
  expected_path
)
  get_target_property(actual_path ${target} ${property})
  get_filename_component(actual_path "${actual_path}" REALPATH)
  get_filename_component(expected_path "${expected_path}" REALPATH)
  if(NOT
     "${actual_path}"
     STREQUAL
     "${expected_path}"
  )
    message(FATAL_ERROR "Unexpected ${property}: '${actual_path}'; expected '${expected_path}'")
  endif()
endfunction()

set(original_base_path "${CMAKE_CURRENT_SOURCE_DIR}/original_build_tree")
set(install_dir "${CMAKE_CURRENT_SOURCE_DIR}/installed")

add_library(cmake_external_interfaces INTERFACE IMPORTED)
set_target_properties(
  cmake_external_interfaces
  PROPERTIES BASE_PATH "${original_base_path}"
             STL_FILES "${original_base_path}/stl/first.stl"
             HLA_FOM_DIRS "${original_base_path}/hla"
             HLA_MAPPINGS "${original_base_path}/hla/mapping.xml"
)

get_external_interfaces(
  TARGET
  cmake_external_interfaces
  INSTALLATION_DIR
  ${install_dir}
)

assert_installed_path(cmake_external_interfaces INSTALL_BASE_PATH "${install_dir}/interfaces")
assert_installed_path(cmake_external_interfaces INSTALL_STL_FILES "${install_dir}/interfaces/stl/first.stl")
assert_installed_path(cmake_external_interfaces INSTALL_HLA_FOM_DIRS "${install_dir}/hla")
assert_installed_path(cmake_external_interfaces INSTALL_HLA_MAPPINGS "${install_dir}/hla/mapping.xml")
