# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

find_package(Doxygen)

if(Doxygen_FOUND)

  # if MKDOCS_SITE_DIR is defined we have to put the output doxydocs into a different folder
  if(DEFINED ENV{MKDOCS_SITE_DIR})
    set(doxygen_output_dir $ENV{MKDOCS_SITE_DIR}/manual/doxygen)
  else()
    set(doxygen_output_dir ${PROJECT_SOURCE_DIR}/docs/doxygen_gen/)
  endif()

  # doxyfile generation
  set(doxyfile_in Doxyfile.in)
  set(doxyfile_out ${CMAKE_CURRENT_BINARY_DIR}/docs/doxygen/Doxyfile)
  set(project_src_dir ${PROJECT_SOURCE_DIR})
  set(doxygen_base_dir ${PROJECT_SOURCE_DIR}/docs/doxygen)

  configure_file(${doxyfile_in} ${doxyfile_out} @ONLY)

  set(doxygen_index_file ${doxygen_output_dir}/html/index.html)

  # create output directory at configure and build time (the mkdocs target erases the directory)
  file(MAKE_DIRECTORY ${doxygen_output_dir})

  add_custom_command(
    OUTPUT ${doxygen_output_dir} COMMAND ${CMAKE_COMMAND} -E make_directory ${doxygen_output_dir}
  )

  add_custom_target(
    doxydocs
    DEPENDS ${doxygen_output_dir}
    COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile_out} WORKING_DIR ${doxygen_output_dir}
  )

  set_target_properties(doxydocs PROPERTIES FOLDER "docs")
endif()
