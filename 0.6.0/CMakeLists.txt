# === CMakeLists.txt ===================================================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

find_program(mkdocs_exec NAMES mkdocs)

# ===================================================================================================================
# Mkdocs
# ===================================================================================================================
if(mkdocs_exec)

  # create snippets dir if it does not exist
  set(snippets_dir ${PROJECT_SOURCE_DIR}/docs/snippets)
  execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${snippets_dir})

  # create the name of the script that we will use to generate snippets
  if(WIN32)
    set(snippets_gen_script "${CMAKE_CURRENT_BINARY_DIR}/generate_snippets_$<CONFIG>.bat")
    set(sen_exec "sen")
  else()
    set(snippets_gen_script "${CMAKE_CURRENT_BINARY_DIR}/generate_snippets_$<CONFIG>.sh")
    set(sen_exec "./sen")
  endif()

  set(fom_base_dir ${PROJECT_SOURCE_DIR}/examples/packages/hla_fom)

  # write the script
  file(
    GENERATE
    OUTPUT ${snippets_gen_script}
    CONTENT
      "cd $<TARGET_FILE_DIR:cli_sen>\n\
${sen_exec} --help > ${snippets_dir}/sen.sh\n\
\n\
${sen_exec} run --help > ${snippets_dir}/sen_run.sh\n\
${sen_exec} file-to-array --help > ${snippets_dir}/sen_file_to_array.sh\n\
${sen_exec} generate --help > ${snippets_dir}/sen_generate.sh\n\
\n\
${sen_exec} generate cpp --help > ${snippets_dir}/sen_generate_cpp.sh\n\
${sen_exec} generate cpp stl --help > ${snippets_dir}/sen_generate_cpp_stl.sh\n\
${sen_exec} generate cpp fom --help > ${snippets_dir}/sen_generate_cpp_fom.sh\n\
${sen_exec} generate cpp exports --help > ${snippets_dir}/sen_generate_cpp_exports.sh\n\
\n\
${sen_exec} generate uml --help > ${snippets_dir}/sen_generate_uml.sh\n\
${sen_exec} generate uml stl --help > ${snippets_dir}/sen_generate_uml_stl.sh\n\
${sen_exec} generate uml fom --help > ${snippets_dir}/sen_generate_uml_fom.sh\n\
\n\
${sen_exec} generate mkdocs --help > ${snippets_dir}/sen_generate_mkdocs.sh\n\
${sen_exec} generate mkdocs stl --help > ${snippets_dir}/sen_generate_mkdocs_stl.sh\n\
${sen_exec} generate mkdocs fom --help > ${snippets_dir}/sen_generate_mkdocs_fom.sh\n\
\n\
${sen_exec} generate json package stl --help > ${snippets_dir}/sen_generate_json_package_stl.sh\n\
${sen_exec} generate json package fom --help > ${snippets_dir}/sen_generate_json_package_fom.sh\n\
\n\
${sen_exec} generate json component stl --help > ${snippets_dir}/sen_generate_json_component_stl.sh\n\
${sen_exec} generate json component fom --help > ${snippets_dir}/sen_generate_json_component_fom.sh\n\
\n\
${sen_exec} generate json schema --help > ${snippets_dir}/sen_generate_json_schema.sh\n\
\n\
${sen_exec} archive --help > ${snippets_dir}/sen_archive.sh\n\
${sen_exec} archive indexed --help > ${snippets_dir}/sen_archive_indexed.sh\n\
${sen_exec} archive info --help > ${snippets_dir}/sen_archive_info.sh\n\
\n\
${sen_exec} package --help > ${snippets_dir}/sen_package.sh\n\
${sen_exec} package init --help > ${snippets_dir}/sen_package_init.sh\n\
${sen_exec} package init-component --help > ${snippets_dir}/sen_package_init-component.sh\n\
\n\
${sen_exec} generate mkdocs fom --directories ${fom_base_dir}/rpr ${fom_base_dir}/netn ${fom_base_dir}/link16 -t \"Generated RPR, NETN & L16 Docs\" -o ${snippets_dir}/fom.md\n\
exit 0"
      FILE_PERMISSIONS
      OWNER_READ
      OWNER_WRITE
      OWNER_EXECUTE
      GROUP_READ
      GROUP_WRITE
      GROUP_EXECUTE
      WORLD_READ
      WORLD_WRITE
      WORLD_EXECUTE
  )

  sen_generate_uml(
    TARGET fom_uml
    OUT ${snippets_dir}/fom.plantuml
    HLA_FOM_DIRS ${fom_base_dir}/rpr ${fom_base_dir}/netn ${fom_base_dir}/link16
    CLASSES_ONLY
  )

  set(fom_doc_example_file ${snippets_dir}/fom.md)

  set(snippets
      ${snippets_dir}/sen.sh
      ${snippets_dir}/LICENSE.txt
      ${snippets_dir}/sen_run.sh
      ${snippets_dir}/sen_file_to_array.sh
      ${snippets_dir}/sen_generate.sh
      ${snippets_dir}/sen_generate_cpp.sh
      ${snippets_dir}/sen_generate_cpp_stl.sh
      ${snippets_dir}/sen_generate_cpp_fom.sh
      ${snippets_dir}/sen_generate_cpp_exports.sh
      ${snippets_dir}/sen_generate_uml.sh
      ${snippets_dir}/sen_generate_uml_stl.sh
      ${snippets_dir}/sen_generate_uml_fom.sh
      ${snippets_dir}/sen_generate_mkdocs.sh
      ${snippets_dir}/sen_generate_mkdocs_stl.sh
      ${snippets_dir}/sen_generate_mkdocs_fom.sh
      ${snippets_dir}/sen_generate_json_package_stl.sh
      ${snippets_dir}/sen_generate_json_package_fom.sh
      ${snippets_dir}/sen_generate_json_component_stl.sh
      ${snippets_dir}/sen_generate_json_component_fom.sh
      ${snippets_dir}/sen_generate_json_schema.sh
      ${snippets_dir}/sen_archive.sh
      ${snippets_dir}/sen_archive_indexed.sh
      ${snippets_dir}/sen_archive_info.sh
      ${snippets_dir}/sen_package.sh
      ${snippets_dir}/sen_package_init.sh
      ${snippets_dir}/sen_package_init-component.sh
      ${fom_doc_example_file}
  )

  if(WIN32)
    add_custom_command(
      OUTPUT ${snippets}
      COMMAND ${snippets_gen_script}
      COMMENT "generating snippets"
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
  else()
    add_custom_command(
      OUTPUT ${snippets}
      COMMAND sh ${snippets_gen_script}
      COMMENT "generating snippets"
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
  endif()

  add_custom_target(snippets_target DEPENDS ${snippets})
  set_target_properties(snippets_target PROPERTIES FOLDER ".cmake")

  add_dependencies(
    snippets_target
    fom_uml
    cli_sen
    cli_gen
    cli_run
    cli_package
    cli_archive
  )

  file(
    GLOB_RECURSE doc_sources
    LIST_DIRECTORIES false
    *.md
    *.gif
    *.png
    *.jpg
    *.txt
  )

  # when deploying docs automatically we can set the site-dir to a different path to ease the update process
  if(NOT DEFINED ENV{MKDOCS_SITE_DIR})
    set(ENV{MKDOCS_SITE_DIR} "${CMAKE_CURRENT_BINARY_DIR}/site")
  endif()

  # ===================================================================================================================
  # Build the licenses documentation
  # ===================================================================================================================

  set(_lic_page ${PROJECT_SOURCE_DIR}/docs/licenses/licensed_code_in_sen.md)
  file(WRITE ${_lic_page} "# Licensed Code in Sen\n")
  file(APPEND ${_lic_page} "\n")
  file(APPEND ${_lic_page} "The public API of Sen **has no dependencies**.\n\n")
  file(APPEND ${_lic_page} "Internally, some parts of the Sen _implementation_ uses third-party\n")
  file(APPEND ${_lic_page} "code that is licensed under specific open-source licenses from the \n")
  file(APPEND ${_lic_page} "original authors:\n")
  file(APPEND ${_lic_page} "\n")

  set(licenses_dir ${PROJECT_SOURCE_DIR}/build/foss_licenses)

  if(EXISTS "${licenses_dir}")
    file(
      GLOB sources_list
      LIST_DIRECTORIES true
      ${licenses_dir}/*
    )
    foreach(dir ${sources_list})
      if(IS_DIRECTORY ${dir})
        get_filename_component(dep_name ${dir} NAME)
        file(APPEND ${_lic_page} "## ${dep_name}\n")

        file(
          GLOB_RECURSE licenses_list
          LIST_DIRECTORIES false
          ${dir}/*
        )
        foreach(license ${licenses_list})
          get_filename_component(lic_name ${license} NAME)
          file(READ ${license} lic_content)

          file(APPEND ${_lic_page} "\n")
          file(APPEND ${_lic_page} "```text title=\"${lic_name}\"\n")
          file(APPEND ${_lic_page} "${lic_content}\n")
          file(APPEND ${_lic_page} "```\n")
          file(APPEND ${_lic_page} "\n")
        endforeach()
      endif()
    endforeach()
  endif()

  # ===================================================================================================================
  # Generate the mkdocs content
  # ===================================================================================================================

  add_custom_target(
    mkdocs
    DEPENDS ${doc_sources} ${snippets}
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/LICENSE.txt ${snippets_dir}/LICENSE.txt
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different ${PROJECT_SOURCE_DIR}/examples
            ${snippets_dir}/examples
    COMMAND
      ${CMAKE_COMMAND} -E copy
      ${PROJECT_SOURCE_DIR}/components/recorder/stl/sen/components/recorder/configuration.stl
      ${snippets_dir}/recorder_config.stl
    COMMAND
      ${CMAKE_COMMAND} -E copy
      ${PROJECT_SOURCE_DIR}/components/recorder/stl/sen/components/recorder/recorder.stl
      ${snippets_dir}/recorder.stl
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/components/logmaster/stl/logmaster.stl
            ${snippets_dir}/logmaster.stl
    COMMAND
      ${CMAKE_COMMAND} -E copy
      ${PROJECT_SOURCE_DIR}/components/replayer/stl/sen/components/replayer/configuration.stl
      ${snippets_dir}/replayer_config.stl
    COMMAND
      ${CMAKE_COMMAND} -E copy
      ${PROJECT_SOURCE_DIR}/components/replayer/stl/sen/components/replayer/replayer.stl
      ${snippets_dir}/replayer.stl
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/components/ether/stl/configuration.stl
            ${snippets_dir}/ether_config.stl
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/components/influx/stl/configuration.stl
            ${snippets_dir}/influx_config.stl
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/components/py/stl/python_interpreter.stl
            ${snippets_dir}/python_interpreter.stl
    COMMAND ${mkdocs_exec} build --config-file=${PROJECT_SOURCE_DIR}/mkdocs.yml
            --site-dir=$ENV{MKDOCS_SITE_DIR}
    COMMENT "generating main site"
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    VERBATIM
  )

  find_program(plantuml_exec NAMES plantuml)

  if(plantuml_exec)
    add_custom_target(
      plantuml_output
      DEPENDS fom_uml
      COMMAND plantuml -tsvg fom.plantuml
      WORKING_DIRECTORY ${snippets_dir}
    )

    add_dependencies(mkdocs snippets_target plantuml_output)
  endif()

  set_target_properties(mkdocs PROPERTIES FOLDER "docs")

endif()
