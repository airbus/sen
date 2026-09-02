# === CMakeLists.txt =================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ====================================================================================================================

find_package(Doxygen)

# TODO(SEN-1727): the API reference is written into the source tree because
# mkdocs erases its own site directory, so output placed there would be deleted
# by the very build that publishes it. Putting it under MKDOCS_SITE_DIR needs
# doxygen to run after mkdocs rather than before, which is a change to the
# published layout and belongs with the question of whether the reference moves
# inside the site at all.
set(doxygen_output_dir ${PROJECT_SOURCE_DIR}/docs/doxygen_gen)

set(doxyfile_out ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
set(project_src_dir ${PROJECT_SOURCE_DIR})
set(doxygen_base_dir ${PROJECT_SOURCE_DIR}/docs/doxygen)

configure_file(Doxyfile.in ${doxyfile_out} @ONLY)

if(Doxygen_FOUND)
  add_custom_target(
    doxydocs
    COMMAND ${CMAKE_COMMAND} -E make_directory ${doxygen_output_dir}
    COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile_out}
    # EXCLUDE lists css, custom and html relative to this directory, so this is
    # where doxygen has to run for those to mean anything.
    WORKING_DIRECTORY ${doxygen_base_dir}
    COMMENT "generating the API reference"
  )
else()
  # Not fatal while configuring: docs_preview needs no doxygen, and making this
  # an error would stop the people that target exists for.
  add_custom_target(
    doxydocs
    COMMAND
      ${CMAKE_COMMAND} -E echo
      "doxygen was not found, so the API reference cannot be built. Install it, or use the docs_preview target."
    COMMAND ${CMAKE_COMMAND} -E false
  )
endif()

set_target_properties(doxydocs PROPERTIES FOLDER "docs")
